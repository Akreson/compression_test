
// (0...255 = 256) + end_of_stream + total_cum_freq
class StaticByteModel
{
	u32 CumFreq[258];

	static constexpr u32 FreqArraySize = ArrayCount(CumFreq);

public:
	static constexpr u32 EndOfStreamSymbolIndex = FreqArraySize - 2;

	StaticByteModel()
	{
		reset();
	}

	void reset()
	{
		for (int i = 0; i < FreqArraySize; ++i)
		{
			CumFreq[i] = i;
		}
	}

	void update(u32 Symbol)
	{
		Assert((Symbol < FreqArraySize));

		for (u32 i = Symbol + 1; i < FreqArraySize; ++i)
		{
			CumFreq[i] += 1;
		}

		if (CumFreq[FreqArraySize - 1] >= FreqMaxValue)
		{
			CumFreq[0] = (CumFreq[0] + 1) / 2;
			for (u32 i = 1; i < FreqArraySize; ++i)
			{
				u32 Freq = CumFreq[i];
				Freq = (Freq + 1) / 2;

				u32 PrevFreq = CumFreq[i - 1];
				if (Freq <= PrevFreq)
				{
					Freq = PrevFreq + 1;
				}

				CumFreq[i] = Freq;
			}
		}
	}

	prob getProb(u32 Symbol)
	{
		Assert(Symbol <= EndOfStreamSymbolIndex);

		prob Result;
		Result.lo = CumFreq[Symbol];
		Result.hi = CumFreq[Symbol + 1];
		Result.scale = CumFreq[FreqArraySize - 1];

		return Result;
	}

	prob getByteFromFreq(u32 Freq, u32* Byte)
	{
		prob Result = {};

		for (u32 i = 0; i < FreqArraySize; ++i)
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

	u32 getCount()
	{
		u32 Result = CumFreq[FreqArraySize - 1];
		return Result;
	}

	inline prob getEndStreamProb()
	{
		prob Result = getProb(EndOfStreamSymbolIndex);
		return Result;
	}
};