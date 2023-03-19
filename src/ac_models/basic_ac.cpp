
class BasicACByteModel
{
	u16 CumFreq[258]; // (0...255 = 256) + end_of_stream + total_cum_freq

	static constexpr u32 CumFreqArraySize = ArrayCount(CumFreq);
	static constexpr u32 TotalIndex = CumFreqArraySize - 1;

public:
	static constexpr u32 EndOfStreamSymbolIndex = CumFreqArraySize - 2;

	BasicACByteModel()
	{
		reset();
	}

	void reset()
	{
		for (u32 i = 0; i < CumFreqArraySize; ++i)
		{
			CumFreq[i] = i;
		}
	}

	void update(u32 Symbol)
	{
		Assert((Symbol < CumFreqArraySize));

		for (u32 i = Symbol + 1; i < CumFreqArraySize; ++i)
		{
			CumFreq[i] += 1;
		}

		if (CumFreq[CumFreqArraySize - 1] >= FREQ_MAX_VALUE)
		{
			for (u32 i = 1; i < CumFreqArraySize; ++i)
			{
				u16 Freq = CumFreq[i];
				Freq = (Freq + 1) / 2;

				u16 PrevFreq = CumFreq[i - 1];
				if (Freq <= PrevFreq)
				{
					Freq = PrevFreq + 1;
				}

				CumFreq[i] = Freq;
			}
		}
	}

	prob getProb(u32 Symbol) const
	{
		Assert(Symbol <= EndOfStreamSymbolIndex);

		prob Result;
		Result.lo = CumFreq[Symbol];
		Result.hi = CumFreq[Symbol + 1];
		Result.scale = CumFreq[TotalIndex];

		return Result;
	}

	prob getSymbolFromFreq(u32 Freq, u32* Byte) const
	{
		prob Result = {};

		for (u32 i = 0; i < CumFreqArraySize; ++i)
		{
			if (Freq < CumFreq[i + 1])
			{
				*Byte = i;
				Result = getProb(i);
				break;
			}
		}

		return Result;
	}

	u32 getTotal() const
	{
		u32 Result = CumFreq[TotalIndex];
		return Result;
	}

	inline prob getEndStreamProb() const
	{
		prob Result = getProb(EndOfStreamSymbolIndex);
		return Result;
	}
};