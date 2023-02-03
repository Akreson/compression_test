static constexpr u32 ProbBit = 14;
static constexpr u32 ProbScale = 1 << ProbBit;
static constexpr u32 RansByteL = 1 << 24;

//TODO: see if compiler with O2 will pass "this" to func
struct RansByteEncoder
{
	u32 State;

	RansByteEncoder() = default;

	void inline init()
	{
		State = RansByteL;
	}

	static inline u32 renorm(u32 UpdateState, u8** OutP, u32 Freq, u32 ScaleBit)
	{
		u32 Max = ((RansByteL >> ScaleBit) << 8 ) * Freq;
		if (UpdateState >= Max)
		{
			u8* Out = *OutP;
			do
			{
				*--Out = UpdateState & 0xff;
				UpdateState >>= 8;
			} while (UpdateState >= Max);

			*OutP = Out;
		}

		return UpdateState;
	}

	void encode(u8** OutP, u32 CumStart, u32 Freq, u32 ScaleBit)
	{
		u32 NormState = RansByteEncoder::renorm(State, OutP, Freq, ScaleBit);
		State = ((NormState / Freq) << ScaleBit) + (NormState % Freq) + CumStart;
	}

	void flush(u8** OutP)
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

struct RansByteDecoder
{
	u32 State;

	RansByteDecoder() = default;

	inline void init(u8** InP)
	{
		uint8_t* In = *InP;

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
		//u32 CurrState = State;
		State = Freq * (State >> ScaleBit) + (State & Mask) - CumStart;
		if (State < RansByteL)
		{
			u8* In = *InP;
			do
			{
				State = State << 8 | *In++;
			} while (State < RansByteL);
			*InP = In;
		}
	}
};