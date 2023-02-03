// just mimic Fabian's example https://github.com/rygorous/ryg_rans

struct SymbolStats
{
	u32 Freq[256];
	u32 CumFreq[257];

	SymbolStats() = default;

	void countSymbol(u8* Input, u64 Size)
	{
		ZeroSize(Freq, sizeof(Freq));

		for (u64 i = 0; i < Size; i++)
		{
			Freq[Input[i]]++;
		}
	}

	void normalize(u32 TargetTotal)
	{
		Assert(TargetTotal >= 256);

		caclUnnormCumFreq();
		u32 CurrTotal = CumFreq[256];

		for (u32 i = 1; i < 257; i++)
		{
			CumFreq[i] = ((u64)TargetTotal * CumFreq[i]) / CurrTotal;
		}

		for (u32 i = 0; i < 256; i++)
		{
			if (Freq[i] && (CumFreq[i] == CumFreq[i + 1]))
			{
				u32 BestFreq = ~0u;
				s32 BestStealIndex = -1;
				for (u32 j = 0; j < 256; j++)
				{
					u32 CheckFreq = CumFreq[j + 1] - CumFreq[j];
					if ((CheckFreq > 1) && (CheckFreq < BestFreq))
					{
						BestFreq = CheckFreq;
						BestStealIndex = j;
					}
				}

				Assert(BestStealIndex != -1);
				if (BestStealIndex < i)
				{
					for (u32 j = BestStealIndex + 1; j <= i; j++)
					{
						CumFreq[j]--;
					}
				}
				else
				{
					for (u32 j = i + 1; j <= BestStealIndex; j++)
					{
						CumFreq[j]++;
					}
				}
			}
		}

		Assert((CumFreq[0] == 0) && (CumFreq[256] == TargetTotal));
		for (u32 i = 0; i < 256; i++)
		{
			if (Freq[i] == 0)
			{
				Assert(CumFreq[i + 1] == CumFreq[i])
			}
			else
			{
				Assert(CumFreq[i + 1] > CumFreq[i])
			}

			Freq[i] = CumFreq[i + 1] - CumFreq[i];
		}
	}

private:
	void caclUnnormCumFreq()
	{
		CumFreq[0] = 0;
		for (u32 i = 0; i < 256; i++)
		{
			CumFreq[i + 1] = CumFreq[i] + Freq[i];
		}
	}
};