class SimpleOrder1AC
{
	u16 Freq[257][257];
	u16 Total[257];
	u32 PrevSymbol;

	static constexpr u32 FreqArraySize = ArrayCount(Freq[0]);

public:
	static constexpr u32 EndOfStreamSymbolIndex = FreqArraySize - 1;

	SimpleOrder1AC()
	{
		reset();
	}

	void reset()
	{
		PrevSymbol = 0;
		MemSet<u16>(reinterpret_cast<u16*>(Freq), sizeof(Freq) / sizeof(u16), 1);
		MemSet<u16>(Total, ArrayCount(Total), 257);
	}

	void update(u32 Symbol)
	{
		u16* CtxFreq = &Freq[PrevSymbol][0];

		CtxFreq[Symbol]++;
		Total[PrevSymbol]++;

		if (Total[PrevSymbol] >= FREQ_MAX_VALUE)
		{
			Total[PrevSymbol] = 0;

			for (u32 i = 0; i < FreqArraySize; ++i)
			{
				u32 Freq = CtxFreq[i];
				Freq = (Freq + 1) / 2;

				CtxFreq[i] = Freq;
				Total[PrevSymbol] += Freq;
			}
		}

		PrevSymbol = Symbol;
	}

	prob getProb(u32 Symbol) const
	{
		Assert(Symbol <= EndOfStreamSymbolIndex);

		prob Result = {};
		const u16* CtxFreq = &Freq[PrevSymbol][0];

		for (u32 i = 0; i < Symbol; i++)
		{
			Result.lo += CtxFreq[i];
		}
		Result.hi = Result.lo + CtxFreq[Symbol];
		Result.scale = Total[PrevSymbol];

		return Result;
	}

	prob getSymbolFromFreq(u32 DecodeFreq, u32* Byte) const
	{
		prob Result = {};
		const u16* CtxFreq = &Freq[PrevSymbol][0];

		u32 CumFreq = 0;
		u32 SymbolIndex = 0;
		for (; SymbolIndex < FreqArraySize; ++SymbolIndex)
		{
			CumFreq += CtxFreq[SymbolIndex];
			if (CumFreq > DecodeFreq) break;
		}

		Result.hi = CumFreq;
		Result.lo = Result.hi - CtxFreq[SymbolIndex];
		Result.scale = Total[PrevSymbol];
		
		*Byte = SymbolIndex;

		return Result;
	}

	inline u32 getTotal() const
	{
		u32 Result = Total[PrevSymbol];
		return Result;
	}

	inline prob getEndStreamProb() const
	{
		prob Result = {};
		const u16* CtxFreq = &Freq[PrevSymbol][0];

		Result.hi = Result.scale = Total[PrevSymbol];
		Result.lo = Result.hi - CtxFreq[FreqArraySize - 1];
		return Result;
	}
};