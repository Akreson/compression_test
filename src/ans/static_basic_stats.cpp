
struct SymbolStats
{
	u32 Freq[256];
	u32 CumFreq[257];

	SymbolStats() = default;

	void countSymbol(u8* Input, u64 Size)
	{
		ZeroSize(Freq, sizeof(Freq));
		CountByte(Freq, Input, Size);
	}

	void normalize(u32 TargetTotal)
	{
		Assert(TargetTotal >= 256);

		caclCumFreq();

		u16 NormFreq[256] = {};
		u16 NormCumFreq[257] = {};

		Normalize(Freq, CumFreq, NormFreq, NormCumFreq, 256, TargetTotal);
		for (u32 i = 0; i < 256; i++)
		{
			Freq[i] = NormFreq[i];
			CumFreq[i + 1] = NormCumFreq[i + 1];
		}
	}

	void fastNormalize(u32 Size, u32 TargetTotal)
	{
		u16 NormFreq[256] = {};
		FastNormalize(Freq, NormFreq, Size, 256, TargetTotal);

		for (u32 i = 0; i < 256; i++)
		{
			Freq[i] = NormFreq[i];
		}
		caclCumFreq();
	}

	void optimalNormalize(u32 TargetTotal)
	{
		u32 TotalSum = 0;
		for (u32 i = 0; i < 256; i++)
		{
			TotalSum += Freq[i];
		}

		u16 NormFreq[256] = {};
		OptimalNormalize(Freq, NormFreq, TotalSum, 256, TargetTotal);

		for (u32 s = 0; s < 256; s++)
		{
			Freq[s] = NormFreq[s];
		}
		caclCumFreq();
	}

private:
	void caclCumFreq()
	{
		CalcCumFreq(Freq, CumFreq, 256);
	}
};