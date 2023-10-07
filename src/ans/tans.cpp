
struct TansEnc
{
	struct entry
	{
		s16 Offset;
		s16 MoreBitsThreshold;
		s16 MaxState;
		u8 MinBits;
	};

	TansEnc::entry* Entries;
	u16* StateTable;
	u64 State;

#if _DEBUG
	u64 MaxL;
#endif

	TansEnc() = default;

	void init(TansEnc::entry* EntriesMem, u32 TableLog, u16* TableMem, const u8* SortedSym, const u16* NormFreq, u32 AlphSymCount = 256)
	{
		Assert(TableLog <= 14);
		u32 L = 1 << TableLog;
		State = L;

#if _DEBUG
		MaxL = L << 1;
#endif

		Entries = EntriesMem;
		StateTable = TableMem;

		u32 CumFreq = 0;
		for (u32 i = 0; i < AlphSymCount; i++)
		{
			u16 Freq = NormFreq[i];
			if (!Freq) continue;

			Entries[i].MaxState = Freq;
			Entries[i].Offset = CumFreq - Freq;

			u32 MaxFMState = (2*Freq) - 1;
			u8 NumBits = TableLog - FindMostSignificantSetBit32(MaxFMState);

			Assert(((L >> NumBits) <= MaxFMState));
			Assert((L >> NumBits) >= Freq);

			Entries[i].MinBits = NumBits;
			Entries[i].MoreBitsThreshold = (MaxFMState + 1) << NumBits;

			Assert(((2*L - 1) >> (NumBits + 1)) <= MaxFMState);
			CumFreq += Freq;
		}
		
		Assert(CumFreq == L);

		for (u32 i = 0; i < L; i++)
		{
			u8 Sym = SortedSym[i];
			u32 ToState = L + i;
			u32 FromState = Entries[Sym].MaxState++;
			
			StateTable[FromState + Entries[Sym].Offset] = ToState;
		}
	}

	void encode(BitWriter& Writer, u8 Sym)
	{
		const entry& Entry = Entries[Sym];

		u8 NumBits = Entry.MinBits;
		if (State >= Entry.MoreBitsThreshold) NumBits++;

		Writer.writeMSB(State & ((1 << NumBits) - 1), NumBits);
		State >>= NumBits;

		State = StateTable[State + Entry.Offset];
	}
};

struct TansDec
{
	struct entry
	{
		u16 NextState;
		u8 Bits;
		u8 Sym;
	};

	entry* Table;
	u64 State;
	u16 L;

	TansDec() = default;

	void init(u64 InitState, TansDec::entry* EntriesMem, u32 TableLog, const u8* SortedSym, const u16* NormFreq, u32 AlphSymCount = 256)
	{
		Assert(TableLog <= 14);
		
		L = 1 << TableLog;
		Table = EntriesMem;
		State = InitState;

		u16 NextState[256];
		for (u32 i = 0; i < AlphSymCount; i++)
		{
			NextState[i] = NormFreq[i];
		}

		for (u32 i = 0; i < L; i++)
		{
			u8 Sym = SortedSym[i];
			u32 FromState = NextState[Sym]++;

			Table[i].Bits = TableLog - FindMostSignificantSetBit32(FromState);
			Table[i].NextState = FromState;
			Table[i].Sym = Sym;
		}

	}

	u8 decode(BitReaderReverseMSB& Reader)
	{
		u16 CurrState = State;
		Assert((CurrState >= L) && (L < 2*CurrState));

		const entry& Entry = Table[CurrState - L];
		CurrState = Entry.NextState;

		CurrState <<= Entry.Bits;
		CurrState |= Reader.peek(Entry.Bits);
		Reader.consume(Entry.Bits);

		State = CurrState;

		return Entry.Sym;
	}
};