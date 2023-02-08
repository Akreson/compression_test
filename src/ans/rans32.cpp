
static constexpr u64 Rans32L = 1ull << 31;

struct Rans32Encoder
{
	u64 State;

	Rans32Encoder() = default;

	void inline init()
	{
		State = Rans32L;
	}

	static inline u64 renorm(u64 StateToNorm, u32** OutP, u32 Freq, u32 ScaleBit)
	{
		u64 Max = ((Rans32L >> ScaleBit) << 32) * Freq;
		if (StateToNorm >= Max)
		{
			*OutP -= 1;
			**OutP = (u32)(StateToNorm & 0xffffffff);
			StateToNorm >>= 32;
			Assert(StateToNorm < Max);
		}

		return StateToNorm;
	}

	void inline encode(u32** OutP, u32 CumStart, u32 Freq, u32 ScaleBit)
	{
		u64 NormState = Rans32Encoder::renorm(State, OutP, Freq, ScaleBit);
		State = ((NormState / Freq) << ScaleBit) + (NormState % Freq) + CumStart;
	}

	void inline encode(u32** OutP, rans_enc_sym64* Sym, u32 ScaleBit)
	{
		u64 NormState = Rans32Encoder::renorm(State, OutP, Sym->Freq, ScaleBit);
		
		u64 q = MulHi64(NormState, Sym->RcpFreq) >> Sym->RcpShift;
		State = NormState + Sym->Bias + q * Sym->CmplFreq;
	}

	void flush(u32** OutP)
	{
		u32* Out = *OutP;

		Out -= 2;
		Out[0] = (u32)(State >> 0);
		Out[1] = (u32)(State >> 32);

		*OutP = Out;
	}
};

struct Rans32Decoder
{
	u64 State;

	Rans32Decoder() = default;

	inline void init(u32** InP)
	{
		u32* In = *InP;

		State = (u64)In[0] << 0;
		State |= (u64)In[1] << 32;

		In += 2;
		*InP = In;
	}

	inline u32 decodeGet(u32 ScaleBit)
	{
		u32 Result = State & ((1 << ScaleBit) - 1);
		return Result;
	}

	inline void decodeAdvance(u32** InP, u32 CumStart, u32 Freq, u32 ScaleBit)
	{
		u32 Mask = (1 << ScaleBit) - 1;
		State = Freq * (State >> ScaleBit) + (State & Mask) - CumStart;
		if (State < Rans32L)
		{
			State = (State << 32) | **InP;
			*InP += 1;
			Assert(State >= Rans32L)
		}
	}

	void inline decodeAdvance(u32** InP, rans_dec_sym64* Sym, u32 ScaleBit)
	{
		decodeAdvance(InP, Sym->CumStart, Sym->Freq, ScaleBit);
	}
};