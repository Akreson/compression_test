#include "huff/huff.cpp"

void
TestHuffDefault1(file_data& InputFile)
{
	PRINT_TEST_FUNC();

	const u32 HuffTableLog = 11;

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

	Timer Timer;

	AccumTime BuildAccum;
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Timer.start();

		b32 Success = HuffBuild.buildTable(ByteCount, HuffTableLog);
		Assert(Success);

		Timer.end();
		BuildAccum.update(Timer);
	}

	BuildAccum.avg(RUNS_COUNT);
	printf("Table build - %lu clocks, %0.6f ms \n", BuildAccum.Clock, BuildAccum.Time * 1000.0);

	HuffBuild.buildCodes(HEnc);
	
	u32 SymEncSize = HuffBuild.countSize(ByteCount);
	u8* EncBuff = new u8[InputFile.Size];
	
	printf(" huff encode\n");
	AccumTime EncAccum;
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Writer.init(EncBuff, InputFile.Size);
		HuffBuild.writeTable(Writer);

		Timer.start();

		for (u32 i = 0; i < InputFile.Size; ++i)
		{
			u8 Sym = InputFile.Data[i];
			HEnc.encode(Writer, Sym);
			Assert(Writer.Stream.Pos < Writer.Stream.End);
		}

		Writer.finish();

		Timer.end();
		EncAccum.update(Timer);
	}
	Assert(Writer.Stream.Pos <= Writer.Stream.End);

	u32 TotalEncSize = Writer.Stream.Pos - Writer.Stream.Start;
	u32 HeaderSize = TotalEncSize - SymEncSize;
	
	PrintAvgPerSymbolPerfStats(EncAccum, RUNS_COUNT, InputFile.Size);
	printf(" header:%lu data:%lu bytes | %.3f ratio\n", HeaderSize, TotalEncSize, (f64)InputFile.Size / (f64)TotalEncSize);

#if 1
	printf(" huff decode\n");

	const u32 DecSymInOneLoop = 2;
	u8* DecBuff = new u8[InputFile.Size + (DecSymInOneLoop - 1)];
	u8* EndDecData = DecBuff + InputFile.Size;

	BitReaderMSB Reader;
	HuffDecTableInfo HuffDecInfo;

	AccumTime DecAccum;
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Reader.init(EncBuff, TotalEncSize);
		HuffDecInfo.readTable(Reader);

		u8* DecTableMem = new u8[HuffDecInfo.DecTableReqSizeByte];

		HuffDecoder HDec(DecTableMem);
		HuffDecInfo.assignCodesMSB(HDec);

		u8* OrigData = InputFile.Data;
		u8* DecData = DecBuff;

		u32 MaxCodeLen = HuffDecInfo.MaxCodeLen;

		Timer.start();

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

		Timer.end();
		DecAccum.update(Timer);
		delete[] DecTableMem;
	}

	PrintAvgPerSymbolPerfStats(DecAccum, RUNS_COUNT, InputFile.Size);
	
	delete[] EncBuff;
	delete[] DecBuff;
#endif
}
