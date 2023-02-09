static constexpr u32 Rans16L = 1 << 16;

struct Rans16Encoder
{
	u32 State;

	Rans16Encoder() = default;

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
		u32 NormState = Rans16Encoder::renorm(State, OutP, Freq, ScaleBit);
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

struct Rans16Decoder
{
	u32 State;

	Rans16Decoder() = default;

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