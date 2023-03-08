
#include "ac/ac.cpp"
#include "ac_models/basic_ac.cpp"

void
CompressFileStatic(const BasicACByteModel& Model, const file_data& InputFile, ByteVec& OutBuffer)
{
	ArithEncoder Encoder(OutBuffer);

	AccumTime Accum;
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		f64 StartTime = timer();
		u64 StartClock = __rdtsc();

		for (u32 i = 0; i < InputFile.Size; ++i)
		{
			prob SymbolProb = Model.getProb(InputFile.Data[i]);
			Encoder.encode(SymbolProb);
		}

		prob SymbolProb = Model.getEndStreamProb();
		Encoder.encode(SymbolProb);
		Encoder.flush();

		u64 Clocks = __rdtsc() - StartClock;
		f64 Time = timer() - StartTime;
		Accum.update(Clocks, Time);

		if ((Run + 1) < RUNS_COUNT)
		{
			Encoder.reset();
		}
	}

	PrintAvgPerSymbolPerfStats(Accum, RUNS_COUNT, InputFile.Size);
}

void
DecompressFileStatic(const BasicACByteModel& Model, const file_data& OutputFile, ByteVec& InputBuffer, const file_data& InputFile)
{
	ArithDecoder Decoder(InputBuffer);
	u32 TotalFreqCount = Model.getTotal();

	AccumTime Accum;
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		f64 StartTime = timer();
		u64 StartClock = __rdtsc();

		u64 ByteIndex = 0;
		for (;;)
		{
			u32 DecodedFreq = Decoder.getCurrFreq(TotalFreqCount);

			u32 DecodedSymbol;
			prob Prob = Model.getSymbolFromFreq(DecodedFreq, &DecodedSymbol);

			if (DecodedSymbol == BasicACByteModel::EndOfStreamSymbolIndex) break;

			Assert(DecodedSymbol <= 255);
			Assert(ByteIndex <= OutputFile.Size);
			Assert(InputFile.Data[ByteIndex] == DecodedSymbol);

			Decoder.updateDecodeRange(Prob);

			OutputFile.Data[ByteIndex++] = static_cast<u8>(DecodedSymbol);
		}

		u64 EncClocks = __rdtsc() - StartClock;
		f64 EncTime = timer() - StartTime;
		Accum.update(EncClocks, EncTime);

		Decoder.initVal();
	}

	PrintAvgPerSymbolPerfStats(Accum, RUNS_COUNT, InputFile.Size);
}

void
TestStaticAC(const file_data& InputFile)
{
	printf("--TestStaticAC\n");
	BasicACByteModel Model;

	for (size_t i = 0; i < InputFile.Size; i++)
	{
		Model.update(InputFile.Data[i]);
	}
	
	ByteVec CompressBuffer;
	CompressFileStatic(Model, InputFile, CompressBuffer);

	u64 CompressedSize = CompressBuffer.size();
	PrintCompressionSize(InputFile.Size, CompressedSize);

	file_data OutputFile;
	OutputFile.Size = InputFile.Size;
	OutputFile.Data = new u8[OutputFile.Size];

	DecompressFileStatic(Model, OutputFile, CompressBuffer, InputFile);

	delete[] OutputFile.Data;
}