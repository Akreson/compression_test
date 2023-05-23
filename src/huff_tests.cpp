#include "huff/huff.cpp"

void
TestHuff1(file_data& InputFile)
{
	PRINT_TEST_FUNC();

	const u32 HuffTableLog = 12;

	if (InputFile.Size > MaxUInt32)
	{
		printf("File to big for _TestHuff1_");
		return;
	}

	u32 ByteCount[256] = {};
	CountByte(ByteCount, InputFile.Data, InputFile.Size);

	BitWriter Writer;
	HuffEncoder HEnc;
	HuffDefaultBuild HuffBuild;

	b32 Success = HuffBuild.buildTable(HEnc, ByteCount, HuffTableLog);
	Assert(Success);

	u32 SymEncSize = HuffBuild.countSize(ByteCount);
	u8* EncBuff = new u8[InputFile.Size];
	
	printf("huff encode\n");
	AccumTime EncAccum;
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Writer.init(EncBuff, InputFile.Size);
		HuffBuild.writeTable(Writer);

		f64 EncStartTime = timer();
		u64 EncStartClock = __rdtsc();

		for (u32 i = 0; i < InputFile.Size; ++i)
		{
			u8 Sym = InputFile.Data[i];
			HEnc.encode(Writer, Sym);
			Assert(Writer.Stream.Pos < Writer.Stream.End);
		}

		Writer.finish();

		u64 EncClocks = __rdtsc() - EncStartClock;
		f64 EncTime = timer() - EncStartTime;
		EncAccum.update(EncClocks, EncTime);
	}
	Assert(Writer.Stream.Pos <= Writer.Stream.End);

	u32 TotalEncSize = Writer.Stream.Pos - Writer.Stream.Start;
	u32 HeaderSize = TotalEncSize - SymEncSize;
	
	PrintAvgPerSymbolPerfStats(EncAccum, RUNS_COUNT, InputFile.Size);

#if 1

	u32 HuffDecTableSizeByte = (1 << HuffTableLog) * sizeof(huff_dec_entry);
	u8* DecTableMem = new u8[HuffDecTableSizeByte];

	const u32 DecSymInOneLoop = 2;
	u8* DecBuff = new u8[InputFile.Size + (DecSymInOneLoop - 1)];
	u8* EndDecData = DecBuff + InputFile.Size;

	BitReader Reader;
	HuffDecoder HDec(DecTableMem);
	HuffDecTableInfo HuffDecInfo;

	printf("huff decode\n");
	AccumTime DecAccum;
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Reader.init(EncBuff, TotalEncSize);
		HuffDecInfo.readTable(Reader);
		HuffDecInfo.assignCodesMSB(HDec);

		Assert(HuffDecInfo.DecTableReqSizeByte == HuffDecTableSizeByte);

		u8* OrigData = InputFile.Data;
		u8* DecData = DecBuff;

		f64 EncStartTime = timer();
		u64 EncStartClock = __rdtsc();

		while (DecData < EndDecData)
		{
#if 0
			Reader.refillTo(HuffTableLog);
			u8 Byte = HDec.decode(Reader, HuffTableLog);

			Assert(*OrigData++ == Byte);
			*DecData++ = Byte;
#else
			Reader.refillTo(HuffTableLog * DecSymInOneLoop);
			*DecData++ = HDec.decode(Reader, HuffTableLog);
			*DecData++ = HDec.decode(Reader, HuffTableLog);
#endif
		}

		u64 EncClocks = __rdtsc() - EncStartClock;
		f64 EncTime = timer() - EncStartTime;
		DecAccum.update(EncClocks, EncTime);
	}

	PrintAvgPerSymbolPerfStats(DecAccum, RUNS_COUNT, InputFile.Size);
#endif
}
