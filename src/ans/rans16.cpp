#include <smmintrin.h>

static constexpr u32 Rans16L = 1 << 16;

struct Rans16Enc
{
	u32 State;

	Rans16Enc() = default;

	void inline init()
	{
		State = Rans16L;
	}

	static inline u32 renorm(u32 StateToNorm, u16** OutP, u32 Freq, u32 ScaleBit)
	{
		u64 Max = ((Rans16L >> ScaleBit) << 16) * Freq;
		if (StateToNorm >= Max)
		{
			*OutP -= 1;
			**OutP = (u16)(StateToNorm & 0xffff);
			StateToNorm >>= 16;
			Assert(StateToNorm < Max);
		}

		return StateToNorm;
	}

	void inline encode(u16** OutP, u32 CumStart, u32 Freq, u32 ScaleBit)
	{
		u32 NormState = Rans16Enc::renorm(State, OutP, Freq, ScaleBit);
		State = ((NormState / Freq) << ScaleBit) + (NormState % Freq) + CumStart;
	}

	void inline flush(u16** OutP)
	{
		u32 EndState = State;
		u16* Out = *OutP;

		Out -= 2;
		Out[0] = (u16)(State >> 0);
		Out[1] = (u16)(State >> 16);

		*OutP = Out;
	}
};

struct Rans16Dec
{
	u32 State;

	Rans16Dec() = default;

	inline void init(u16** InP)
	{
		u16* In = *InP;

		State = In[0] << 0;
		State |= In[1] << 16;

		In += 2;
		*InP = In;
	}

	inline u32 decodeGet(u32 ScaleBit)
	{
		u32 Result = State & ((1 << ScaleBit) - 1);
		return Result;
	}

	inline void decodeAdvance(u16** InP, u32 CumStart, u32 Freq, u32 ScaleBit)
	{
		u32 Mask = (1 << ScaleBit) - 1;
		State = Freq * (State >> ScaleBit) + (State & Mask) - CumStart;
		if (State < Rans16L)
		{
			State = (State << 16) | **InP;
			*InP += 1;
			Assert(State >= Rans16L)
		}
	}

	template<u32 N>
	inline u8 decodeSym(rans_sym_table<N>& Tab, u32 CumFreqBound, u32 ScaleBit)
	{
		Assert(IsPowerOf2(CumFreqBound));
		u32 Slot = State & (CumFreqBound - 1);
		
		/*u32 Val = Tab.Slot[Slot].Val;
		u32 Freq = Val & 0xffff;
		u32 Bias = Val >> 16;
		State = Freq * (State >> ScaleBit) + Bias;*/

		State = Tab.Slot[Slot].Freq * (State >> ScaleBit) + Tab.Slot[Slot].Bias;
		u8 Sym = Tab.Slot2Sym[Slot];
		return Sym;
	}

	inline void decodeRenorm(u16** InP)
	{
		if (State < Rans16L)
		{
			State = (State << 16) | **InP;
			*InP += 1;
			Assert(State >= Rans16L)
		}
	}
};

struct Rans16DecSIMD
{
	union
	{
		__m128i simd;
		Rans16Dec lane[4];
	} State;

	inline void init(u16** In)
	{
		State.simd = _mm_loadu_si128(reinterpret_cast<const __m128i*>(*In));
		*In += 8;
	}

	template<u32 N>
	inline u32 decodeSym(rans_sym_table<N>& Tab, u32 CumFreqBound, u32 ScaleBit)
	{
		Assert(IsPowerOf2(CumFreqBound));
		
		__m128i State_4x = State.simd;
		__m128i Slots =_mm_and_si128(State_4x, _mm_set1_epi32(CumFreqBound - 1));
		u32 Index0 = _mm_cvtsi128_si32(Slots);
		u32 Index1 = _mm_extract_epi32(Slots, 1);
		u32 Index2 = _mm_extract_epi32(Slots, 2);
		u32 Index3 = _mm_extract_epi32(Slots, 3);

		u32 Symbols = Tab.Slot2Sym[Index0] | (Tab.Slot2Sym[Index1] << 8) | (Tab.Slot2Sym[Index2] << 16) | (Tab.Slot2Sym[Index3] << 24);

		__m128i FreqBiasLo, FreqBiasHi;
		FreqBiasLo = _mm_cvtsi32_si128(Tab.Slot[Index0].Val);
		FreqBiasLo = _mm_insert_epi32(FreqBiasLo, Tab.Slot[Index1].Val, 1);
		FreqBiasHi = _mm_cvtsi32_si128(Tab.Slot[Index2].Val);
		FreqBiasHi = _mm_insert_epi32(FreqBiasHi, Tab.Slot[Index3].Val, 1);
		__m128i FreqBias = _mm_unpacklo_epi64(FreqBiasLo, FreqBiasHi);

		__m128i ScaledState_4x = _mm_srli_epi32(State_4x, ScaleBit);
		__m128i Freq_4x = _mm_and_si128(FreqBias, _mm_set1_epi32(0xffff));
		__m128i Bias_4x = _mm_srli_epi32(FreqBias, 16);
		State.simd = _mm_add_epi32(_mm_mullo_epi32(Freq_4x, ScaledState_4x), Bias_4x);

		return Symbols;
	}

	inline void decodeRenorm(u16** In)
	{
#define _ 0x80 // move 0 to this elem on _mm_shuffle_epi8
		static ALIGN(const u8, Shuffles[16][16], 16) =
		{
			{ _,_,_,_, _,_,_,_, _,_,_,_, _,_,_,_ }, // 0000
			{ 0,1,_,_, _,_,_,_, _,_,_,_, _,_,_,_ }, // 0001
			{ _,_,_,_, 0,1,_,_, _,_,_,_, _,_,_,_ }, // 0010
			{ 0,1,_,_, 2,3,_,_, _,_,_,_, _,_,_,_ }, // 0011
			{ _,_,_,_, _,_,_,_, 0,1,_,_, _,_,_,_ }, // 0100
			{ 0,1,_,_, _,_,_,_, 2,3,_,_, _,_,_,_ }, // 0101
			{ _,_,_,_, 0,1,_,_, 2,3,_,_, _,_,_,_ }, // 0110
			{ 0,1,_,_, 2,3,_,_, 4,5,_,_, _,_,_,_ }, // 0111
			{ _,_,_,_, _,_,_,_, _,_,_,_, 0,1,_,_ }, // 1000
			{ 0,1,_,_, _,_,_,_, _,_,_,_, 2,3,_,_ }, // 1001
			{ _,_,_,_, 0,1,_,_, _,_,_,_, 2,3,_,_ }, // 1010
			{ 0,1,_,_, 2,3,_,_, _,_,_,_, 4,5,_,_ }, // 1011
			{ _,_,_,_, _,_,_,_, 0,1,_,_, 2,3,_,_ }, // 1100
			{ 0,1,_,_, _,_,_,_, 2,3,_,_, 4,5,_,_ }, // 1101
			{ _,_,_,_, 0,1,_,_, 2,3,_,_, 4,5,_,_ }, // 1110
			{ 0,1,_,_, 2,3,_,_, 4,5,_,_, 6,7,_,_ }, // 1111
		};
#undef _

		static const u8 InMoveCount[16] = { 0,1,1,2, 1,2,2,3, 1,2,2,3, 2,3,3,4 };

		const u32 BiasVal = 1 << 31;
		__m128i State_4x = State.simd;
		__m128i BiasedState_4x = _mm_xor_si128(State_4x, _mm_set1_epi32(BiasVal));
		__m128i GtMask = _mm_cmpgt_epi32(_mm_set1_epi32(Rans16L - BiasVal), BiasedState_4x);
		u32 Mask = _mm_movemask_ps(_mm_castsi128_ps(GtMask));

		__m128i MemVals = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(*In)); //  4 16bit values
		__m128i ShiftedState_4x = _mm_slli_epi32(State_4x, 16);
		__m128i ShuffMask = _mm_load_si128(reinterpret_cast<const __m128i*>(Shuffles[Mask]));
		__m128i NewState_4x = _mm_or_si128(ShiftedState_4x, _mm_shuffle_epi8(MemVals, ShuffMask));
		State.simd = _mm_blendv_epi8(State_4x, NewState_4x, GtMask);

		*In += InMoveCount[Mask];
	}
};