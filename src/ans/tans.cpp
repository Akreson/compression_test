
struct TansEnc
{
	struct entry
	{
		s32 deltaState;
		u32 deltaNumBits;
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

		u16 NextState[256] = {};
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

			StateTable[CumFreq[Sym] + FromState - SymNormFreq] = ToState;
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

	void encode(BitWriter& Writer, u8 Sym)
	{
		const entry& Entry = Entries[Sym];

		u8 NumBits = (State + Entry.deltaNumBits) >> 16;
		Writer.writeMSB(State & ((1 << NumBits) - 1), NumBits);

		State = StateTable[(State >> NumBits) + Entry.deltaState];
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

			Table[i].Sym = Sym;
			Table[i].Bits = TableLog - FindMostSignificantSetBit32(FromState);
			Table[i].NextState = (FromState << Table[i].Bits) - L;
		}
	}

	u8 decode(BitReaderReverseMSB& Reader)
	{
		u64 CurrState = State;

		const entry& Entry = Table[CurrState];
		CurrState = Entry.NextState;

		u64 ReadBits = Reader.peek(Entry.Bits);
		Reader.consume(Entry.Bits);

		CurrState += ReadBits;
		State = CurrState;

		return Entry.Sym;
	}
};