#include "huff/huff.cpp"

static constexpr u32 HUFF_TABLE_LOG = 12;

void
CompressFileStaticHuff1(const file_data& InputFile, BitWriter& Writer, u32* Freqs, u8* EncBuff)
{
	HuffEncoder HEnc;
	HuffDefaultBuild HuffBuild;

	b32 Success = HuffBuild.buildTable(HEnc, Freqs, HUFF_TABLE_LOG);
	Assert(Success);

	u32 SymEncSize = HuffBuild.countSize(Freqs);

	AccumTime EncAccum;
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Writer.init(EncBuff, InputFile.Size);
		HuffBuild.writeTable(Writer);

		f64 StartTime = timer();
		u64 StartClock = __rdtsc();

		for (u32 i = 0; i < InputFile.Size; ++i)
		{
			u8 Sym = InputFile.Data[i];
			HEnc.encode(Writer, Sym);
			Assert(Writer.Stream.Pos < Writer.Stream.End);
		}

		Writer.finish();

		u64 EncClocks = __rdtsc() - StartClock;
		f64 EncTime = timer() - StartTime;
		EncAccum.update(EncClocks, EncTime);
	}

	Assert(Writer.Stream.Pos <= Writer.Stream.End);

	u32 TotalEncSize = Writer.Stream.Pos - Writer.Stream.Start;
	u32 HeaderSize = TotalEncSize - SymEncSize;

	PrintAvgPerSymbolPerfStats(EncAccum, RUNS_COUNT, InputFile.Size);
	printf(" header:%lu data:%lu bytes | %.3f ratio\n", HeaderSize, TotalEncSize, (f64)InputFile.Size / (f64)TotalEncSize);
}

void DecompressFileStaticHuff1(const file_data& InputFile, u8* EncBuff, u32 BuffEnd)
{
	const u32 DecSymInOneLoop = 2;
	u8* DecBuff = new u8[InputFile.Size + (DecSymInOneLoop - 1)];
	u8* EndDecData = DecBuff + InputFile.Size;

	BitReader Reader;
	HuffDecTableInfo HuffDecInfo;

	AccumTime DecAccum;
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Reader.init(EncBuff, BuffEnd);
		HuffDecInfo.readTable(Reader);
		
		u8* DecTableMem = new u8[HuffDecInfo.DecTableReqSizeByte];
		HuffDecoder HDec(DecTableMem);
		HuffDecInfo.assignCodesMSB(HDec);

		u8* OrigData = InputFile.Data;
		u8* DecData = DecBuff;

		u32 MaxCodeLen = HuffDecInfo.MaxCodeLen;

		f64 StartTime = timer();
		u64 StartClock = __rdtsc();

		while (DecData < EndDecData)
		{
#if 0
			Reader.refillTo(MaxCodeLen);
			u8 Byte = HDec.decode(Reader, MaxCodeLen);

			Assert(*OrigData++ == Byte);
			*DecData++ = Byte;
#else
			Reader.refillTo(MaxCodeLen * DecSymInOneLoop);
			*DecData++ = HDec.decode(Reader, MaxCodeLen);
			*DecData++ = HDec.decode(Reader, MaxCodeLen);
#endif
		}

		u64 EncClocks = __rdtsc() - StartClock;
		f64 EncTime = timer() - StartTime;
		DecAccum.update(EncClocks, EncTime);
	}

	PrintAvgPerSymbolPerfStats(DecAccum, RUNS_COUNT, InputFile.Size);
	delete[] DecBuff;
}

void
TestHuff1(const file_data& InputFile)
{
	printf("--TestHuff1\n");

	if (InputFile.Size > MaxUInt32)
	{
		printf("File to big for _TestHuff1_");
		return;
	}

	u32 ByteCount[256] = {};
	CountByte(ByteCount, InputFile.Data, InputFile.Size);

	BitWriter Writer;
	u8* EncBuff = new u8[InputFile.Size];

	CompressFileStaticHuff1(InputFile, Writer, ByteCount, EncBuff);

	u32 CompressedSize = Writer.Stream.Pos - Writer.Stream.Start;
	DecompressFileStaticHuff1(InputFile, EncBuff, CompressedSize);

	delete[] EncBuff;
}