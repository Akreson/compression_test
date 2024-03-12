inline u32
TansRadixSort8(u32* RadixHisto, const u32 RadixNumber, const u16* NormFreq, u32 AlphSize = 256)
{
	for (u32 SymIndex = 0; SymIndex < AlphSize; ++SymIndex)
	{
		u32 Freq = NormFreq[SymIndex];
		if (Freq == 0) continue;

		u32 Invp = RadixNumber / Freq;
		Assert(Invp > 0);

		u32 Rank = Invp;
		for (u32 i = 0; i < Freq; ++i)
		{
			RadixHisto[Rank >> 8]++;
			Rank += Invp;
		}
	}

	u32 RadixCum = 0;
	for (u32 i = 0; i < 256; ++i)
	{
		u32 Temp = RadixHisto[i];
		RadixHisto[i] = RadixCum;
		RadixCum += Temp;
	}

	return RadixCum;
}

struct TansEncTable
{
	struct entry
	{
		s32 deltaState;
		u32 deltaNumBits;
	};

	TansEncTable::entry* Entries;
	u16* States;
	u32 StateBits;
	u32 L;

	TansEncTable() = default;

	void initRadix(TansEncTable::entry* EntriesMem, u32 TableLog, u16* TableMem, const u16* NormFreq, u32 AlphSymCount = 256)
	{
		Assert(TableLog <= 14);
		StateBits = TableLog;
		L = 1 << TableLog;

		Entries = EntriesMem;
		States = TableMem;

		u16 CumFreq[256] = {};
		for (u32 i = 1; i < AlphSymCount; i++)
		{
			CumFreq[i] = CumFreq[i - 1] + NormFreq[i - 1];
		}
#if 1
		const u32 RadixNumber = 255 << 8;
		//const u32 RadixNumber = (256 << 8) - 1;

		u32 RadixHisto[256] = {};
		u32 RadixSum = TansRadixSort8(RadixHisto, RadixNumber, NormFreq, AlphSymCount);
		Assert(RadixSum == L);
#endif
		u32 Total = 0;
		for (u32 SymIndex = 0; SymIndex < AlphSymCount; SymIndex++)
		{
			u16 Freq = NormFreq[SymIndex];
			if (!Freq) continue;

			u8 NumBits = TableLog - FindMostSignificantSetBit32(Freq - 1);
			u16 MinStatePlus = Freq << NumBits;
			Entries[SymIndex].deltaNumBits = (NumBits << 16) - MinStatePlus;
			Entries[SymIndex].deltaState = Total - Freq;
			Total += Freq;

			u32 Invp = RadixNumber / Freq;
			u32 Rank = Invp;

			u16* SymStates = States + CumFreq[SymIndex];
			for (u32 i = 0; i < Freq; ++i)
			{
				u32 Index = Rank >> 8;
				Rank += Invp;

				u32 To = RadixHisto[Index]++;
				u16 ToState = (L + To);
				*SymStates++ = ToState;
			}
		}
	}

	void init(TansEncTable::entry* EntriesMem, u32 TableLog, u16* TableMem, const u8* SortedSym, const u16* NormFreq, u32 AlphSymCount = 256)
	{
		Assert(TableLog <= 14);
		StateBits = TableLog;
		L = 1 << TableLog;

		Entries = EntriesMem;
		States = TableMem;

		u16 NextState[256];
		for (u32 i = 0; i < AlphSymCount; i++)
		{
			NextState[i] = NormFreq[i];
		}

		u16 CumFreq[256] = {};
		for (u32 i = 1; i < AlphSymCount; i++)
		{
			CumFreq[i] = CumFreq[i - 1] + NormFreq[i - 1];
		}

		for (u32 i = 0; i < L; i++)
		{
			u32 ToState = L + i;

			u8 Sym = SortedSym[i];
			u16 FromState = NextState[Sym]++;
			u16 SymNormFreq = NormFreq[Sym];

			States[CumFreq[Sym] + FromState - SymNormFreq] = ToState;
		}

		u32 Total = 0;
		for (u32 i = 0; i < AlphSymCount; i++)
		{
			u16 Freq = NormFreq[i];
			if (!Freq) continue;

			u8 NumBits = TableLog - FindMostSignificantSetBit32(Freq - 1);
			u16 MinStatePlus = Freq << NumBits;
			Entries[i].deltaNumBits = (NumBits << 16) - MinStatePlus;
			Entries[i].deltaState = Total - Freq;
			Total += Freq;
		}
	}
};

struct TansDecTable
{
	struct entry
	{
		u16 NextState;
		u8 Bits;
		u8 Sym;
	};

	entry* Entry;
	u32 StateBits;
	u32 L;

	TansDecTable() = default;

	void initRadix(TansDecTable::entry* EntriesMem, u32 TableLog, const u16* NormFreq, u32 AlphSymCount = 256)
	{
		Assert(TableLog <= 14);

		StateBits = TableLog;
		L = 1 << TableLog;

		Entry = EntriesMem;

#if 1
		const u32 RadixNumber = 255 << 8;
		//const u32 RadixNumber = (256 << 8) - 1;

		u32 RadixHisto[256] = {};
		u32 RadixSum = TansRadixSort8(RadixHisto, RadixNumber, NormFreq, AlphSymCount);
		Assert(RadixSum == L);
#endif

		u32 Total = 0;
		for (u32 SymIndex = 0; SymIndex < AlphSymCount; SymIndex++)
		{
			u16 Freq = NormFreq[SymIndex];
			if (!Freq) continue;

			u32 Invp = RadixNumber / Freq;
			u32 Rank = Invp;

			for (u32 i = 0; i < Freq; ++i)
			{
				u32 Index = Rank >> 8;
				Rank += Invp;

				u32 To = RadixHisto[Index]++;
				u32 FromState = Freq + i;

				Entry[To].Sym = SymIndex;
				Entry[To].Bits = TableLog - FindMostSignificantSetBit32(FromState);
				Entry[To].NextState = (FromState << Entry[To].Bits) - L;
			}
		}
	}

	void init(TansDecTable::entry* EntriesMem, u32 TableLog, const u8* SortedSym, const u16* NormFreq, u32 AlphSymCount = 256)
	{
		Assert(TableLog <= 14);

		StateBits = TableLog;
		L = 1 << TableLog;

		Entry = EntriesMem;

		u16 NextState[256];
		for (u32 i = 0; i < AlphSymCount; i++)
		{
			NextState[i] = NormFreq[i];
		}

		for (u32 i = 0; i < L; i++)
		{
			u8 Sym = SortedSym[i];
			u32 FromState = NextState[Sym]++;

			Entry[i].Sym = Sym;
			Entry[i].Bits = TableLog - FindMostSignificantSetBit32(FromState);
			Entry[i].NextState = (FromState << Entry[i].Bits) - L;
		}
	}
};

struct TansState
{
	u64 State;

	void encode(BitWriter& Writer, const TansEncTable& Table, u8 Symbol)
	{
		const TansEncTable::entry& Entry = Table.Entries[Symbol];

		u8 NumBits = (State + Entry.deltaNumBits) >> 16;
		Writer.writeMaskMSB(State, NumBits);

		State = Table.States[(State >> NumBits) + Entry.deltaState];
	}

	u8 decode(BitReaderReverseMSB& Reader, const TansDecTable& Table)
	{
		u64 CurrState = State;

		const TansDecTable::entry& Entry = Table.Entry[CurrState];
		CurrState = Entry.NextState;

		u64 ReadBits = Reader.peek(Entry.Bits);
		Reader.consume(Entry.Bits);

		CurrState += ReadBits;
		State = CurrState;

		return Entry.Sym;
	}
};
