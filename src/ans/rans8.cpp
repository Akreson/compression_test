
static constexpr u32 Rans8L = 1 << 23;

struct Rans8Encoder
{
	u32 State;

	Rans8Encoder() = default;

	void inline init()
	{
		State = Rans8L;
	}

	static inline u32 renorm(u32 StateToNorm, u8** OutP, u32 Freq, u32 ScaleBit)
	{
		u32 Max = ((Rans8L >> ScaleBit) << 8) * Freq;
		if (StateToNorm >= Max)
		{
			u8* Out = *OutP;
			do
			{
				*--Out = (u8)(StateToNorm & 0xff);
				StateToNorm >>= 8;
			} while (StateToNorm >= Max);

			*OutP = Out;
		}

		return StateToNorm;
	}

	void inline encode(u8** OutP, u32 CumStart, u32 Freq, u32 ScaleBit)
	{
		u32 NormState = Rans8Encoder::renorm(State, OutP, Freq, ScaleBit);
		State = ((NormState / Freq) << ScaleBit) + (NormState % Freq) + CumStart;
	}

	static inline u32 renorm(u32 StateToNorm, u8** OutP, u32 Max)
	{
		if (StateToNorm >= Max)
		{
			u8* Out = *OutP;
			do
			{
				*--Out = (u8)(StateToNorm & 0xff);
				StateToNorm >>= 8;
			} while (StateToNorm >= Max);

			*OutP = Out;
		}

		return StateToNorm;
	}

	void inline encode(u8** OutP, rans_enc_sym32* Sym)
	{
		u32 NormState = Rans8Encoder::renorm(State, OutP, Sym->Max);
		u32 q = (((u64)NormState * (u64)Sym->RcpFreq) >> 32) >> Sym->RcpShift;
		State = NormState + Sym->Bias + q * Sym->CmplFreq;
	}

	void inline flush(u8** OutP)
	{
		u32 EndState = State;
		u8* Out = *OutP;

		Out -= 4;
		Out[0] = (u8)(State >> 0);
		Out[1] = (u8)(State >> 8);
		Out[2] = (u8)(State >> 16);
		Out[3] = (u8)(State >> 24);

		*OutP = Out;
	}
};

struct Rans8Decoder
{
	u32 State;

	Rans8Decoder() = default;

	inline void init(u8** InP)
	{
		u8* In = *InP;

		State = In[0] << 0;
		State |= In[1] << 8;
		State |= In[2] << 16;
		State |= In[3] << 24;

		In += 4;
		*InP = In;
	}

	inline u32 decodeGet(u32 ScaleBit)
	{
		u32 Result = State & ((1 << ScaleBit) - 1);
		return Result;
	}

	inline void decodeAdvance(u8** InP, u32 CumStart, u32 Freq, u32 ScaleBit)
	{
		u32 Mask = (1 << ScaleBit) - 1;
		State = Freq * (State >> ScaleBit) + (State & Mask) - CumStart;
		if (State < Rans8L)
		{
			u8* In = *InP;
			do
			{
				State = State << 8 | *In++;
			} while (State < Rans8L);
			*InP = In;
		}
	}

	void inline decodeAdvance(u8** InP, rans_dec_sym32* Sym, u32 ScaleBit)
	{
		decodeAdvance(InP, Sym->CumStart, Sym->Freq, ScaleBit);
	}
};
