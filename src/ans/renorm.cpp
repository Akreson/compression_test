#include <algorithm>

inline void
CountByte(u32* Freq, u8* Input, u64 Size)
{
	for (u64 i = 0; i < Size; i++)
	{
		Freq[Input[i]]++;
	}
}

template<typename T> inline void
CalcCumFreq(T* Freq, T* CumFreq, u32 SymCount)
{
	CumFreq[0] = 0;
	for (u32 i = 0; i < SymCount; i++)
	{
		CumFreq[i + 1] = CumFreq[i] + Freq[i];
	}
}

// Fabian rANS tutorial example https://github.com/rygorous/ryg_rans
void
Normalize(u32* Freq, u32* CumFreq, u16* NormFreq, u16* NormCumFreq, u32 SymCount, u32 TargetTotal)
{
	Assert(IsPowerOf2(TargetTotal) && (TargetTotal >= 256));

	u32 CurrTotal = CumFreq[SymCount];

	for (u32 i = 1; i < (SymCount+1); i++)
	{
		NormCumFreq[i] = ((u64)TargetTotal * CumFreq[i]) / CurrTotal;
	}

	for (u32 i = 0; i < SymCount; i++)
	{
		if (Freq[i] && (NormCumFreq[i] == NormCumFreq[i + 1]))
		{
			u32 BestFreq = ~0u;
			s32 BestStealIndex = -1;
			for (u32 j = 0; j < SymCount; j++)
			{
				u32 CheckFreq = NormCumFreq[j + 1] - NormCumFreq[j];
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
					NormCumFreq[j]--;
				}
			}
			else
			{
				for (u32 j = i + 1; j <= BestStealIndex; j++)
				{
					NormCumFreq[j]++;
				}
			}
		}
	}

	Assert((NormCumFreq[0] == 0) && (NormCumFreq[SymCount] == TargetTotal));
	for (u32 i = 0; i < SymCount; i++)
	{
		if (Freq[i] == 0)
		{
			Assert(NormCumFreq[i + 1] == NormCumFreq[i])
		}
		else
		{
			Assert(NormCumFreq[i + 1] > NormCumFreq[i])
		}

		NormFreq[i] = NormCumFreq[i + 1] - NormCumFreq[i];
	}
}

// Yann Collet FSE fast normalization
void
FastNormalize(u32* Freq, u16* NormFreq, u32 Size, u32 SymCount, u32 TargetTotalLog)
{
	Assert(TargetTotalLog < 16);

	static u32 const rtbTable[] = { 0, 473195, 504333, 520860, 550000, 700000, 750000, 830000 };

	const u64 Scale = 62 - TargetTotalLog;
	const u64 Step = (1ull << 62) / Size;
	const u64 vStep = 1ULL << (Scale - 20);
	s32 ToDistribute = 1 << TargetTotalLog;
	u32 Largest = 0, LargestP = 0;

	for (u32 s = 0; s < SymCount; s++)
	{
		if (Freq[s] > 0)
		{
			u32 Proba = (u32)((Freq[s] * Step) >> Scale);
			u64 RestToBeat;
			if (Proba < 8)
			{
				RestToBeat = vStep * rtbTable[Proba];
				Proba += (Freq[s] * Step) - ((u64)Proba << Scale) > RestToBeat;
			}

			if (Proba > LargestP)
			{
				LargestP = Proba;
				Largest = s;
			}

			NormFreq[s] = Proba;
			ToDistribute -= Proba;
		}
	}

	Assert(-ToDistribute <= (NormFreq[Largest] >> 1));
	NormFreq[Largest] += ToDistribute;
}

struct sort_sym
{
	// sort array of "sym" by comparing "rank"
	u32 sym;
	f32 rank;

	sort_sym() { }
	sort_sym(u32 s, f32 r) : sym(s), rank(r) { }
	bool operator < (const sort_sym& rhs) const
	{
		return rank < rhs.rank;
	}
};

// Charles Bloom optimal normalization http://cbloomrants.blogspot.com/2014/02/02-11-14-understanding-ans-10.html
void
OptimalNormalize(u32* FromFreq, u16* NormFreq, u32 FromTotalSum, u32 SymCount, u32 TargetTotal)
{
	Assert(IsPowerOf2(TargetTotal));
	Assert(FromTotalSum > 0);

	f64 Scale = (f64)TargetTotal / (f64)FromTotalSum;
	u32 NormSum = 0;

	for (u32 i = 0; i < SymCount; i++)
	{
		if (FromFreq[i] == 0)
		{
			NormFreq[i] = 0;
		}
		else
		{
			f64 FreqScaled = FromFreq[i] * Scale;
			u32 Down = (u32)FreqScaled;

			NormFreq[i] = (FreqScaled * FreqScaled < Down * (Down + 1)) ? Down : Down + 1;

			Assert(NormFreq[i] > 0);
			NormSum += NormFreq[i];
		}
	}

	s32 CorrectionCount = TargetTotal - NormSum;
	if (CorrectionCount != 0)
	{
		s32 CorrectionSign = (CorrectionCount > 0) ? 1 : -1;

		std::vector<sort_sym> Heap;
		Heap.reserve(SymCount);

		for (u32 i = 0; i < SymCount; i++)
		{
			if (FromFreq[i] == 0) continue;
			Assert(NormFreq[i] != 0);

			if (NormFreq[i] > 1 || CorrectionSign == 1)
			{
				f32 Change = (f32)log2(1.0 + CorrectionSign * (1.0 / NormFreq[i])) * FromFreq[i];
				Heap.push_back(sort_sym(i, Change));
			}
		}

		std::make_heap(Heap.begin(), Heap.end());

		while (CorrectionCount != 0)
		{
			Assert(!Heap.empty());
			std::pop_heap(Heap.begin(), Heap.end());
			sort_sym ss = Heap.back();
			Heap.pop_back();

			u32 i = ss.sym;
			Assert(FromFreq[i] != 0);

			NormFreq[i] += CorrectionSign;
			CorrectionCount -= CorrectionSign;
			Assert(NormFreq[i] != 0);

			if (NormFreq[i] > 1 || CorrectionSign == 1)
			{
				f32 Change = (f32)log(1.0 + CorrectionSign * (1.0 / NormFreq[i])) * FromFreq[i];

				Heap.push_back(sort_sym(i, Change));
				std::push_heap(Heap.begin(), Heap.end());
			}
		}
	}
}