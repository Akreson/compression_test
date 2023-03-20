
#include "ac/ac.cpp"
#include "ac_models/basic_ac.cpp"
#include "ac_models/simple_order1_ac.cpp"

void
CompressFileStatic(BasicACByteModel& Model, const file_data& InputFile, ByteVec& OutBuffer)
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
DecompressFileStatic(BasicACByteModel& Model, const file_data& OutputFile, ByteVec& InputBuffer, const file_data& InputFile)
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
	ByteVec CompressBuffer;

	for (size_t i = 0; i < InputFile.Size; i++)
	{
		Model.update(InputFile.Data[i]);
	}

	CompressFileStatic(Model, InputFile, CompressBuffer);

	u64 CompressedSize = CompressBuffer.size();
	PrintCompressionSize(InputFile.Size, CompressedSize);

	file_data OutputFile;
	OutputFile.Size = InputFile.Size;
	OutputFile.Data = new u8[OutputFile.Size];

	DecompressFileStatic(Model, OutputFile, CompressBuffer, InputFile);

	delete[] OutputFile.Data;
}


void
CompressFileOrder0(BasicACByteModel& Model, const file_data& InputFile, ByteVec& OutBuffer)
{
	ArithEncoder Encoder(OutBuffer);

	for (u32 i = 0; i < InputFile.Size; ++i)
	{
		prob SymbolProb = Model.getProb(InputFile.Data[i]);
		Encoder.encode(SymbolProb);
		Model.update(InputFile.Data[i]); // now update model
	}

	prob SymbolProb = Model.getEndStreamProb();
	Encoder.encode(SymbolProb);
	Encoder.flush();
}

void
DecompressFileOrder0(BasicACByteModel& Model, const file_data& OutputFile, ByteVec& InputBuffer, const file_data& InputFile)
{
	ArithDecoder Decoder(InputBuffer);

	u64 ByteIndex = 0;
	for (;;)
	{
		u32 DecodedFreq = Decoder.getCurrFreq(Model.getTotal());

		u32 DecodedSymbol;
		prob Prob = Model.getSymbolFromFreq(DecodedFreq, &DecodedSymbol);

		if (DecodedSymbol == BasicACByteModel::EndOfStreamSymbolIndex) break;

		Assert(DecodedSymbol <= 255);
		Assert(ByteIndex <= OutputFile.Size);
		Assert(InputFile.Data[ByteIndex] == DecodedSymbol);

		Decoder.updateDecodeRange(Prob);
		Model.update(DecodedSymbol); // now update model

		OutputFile.Data[ByteIndex++] = static_cast<u8>(DecodedSymbol);
	}
}

void
TestOrder0AC(const file_data & InputFile)
{
	printf("--TestOrder0AC\n");
	BasicACByteModel Model;
	ByteVec CompressBuffer;

	f64 StartTime = timer();
	CompressFileOrder0(Model, InputFile, CompressBuffer);
	f64 EndTime = timer() - StartTime;

	Model.reset();
	u64 CompressedSize = CompressBuffer.size();

	PrintCompressionSize(InputFile.Size, CompressedSize);
	printf(" EncTime %.3f\n", EndTime);

	file_data OutputFile;
	OutputFile.Size = InputFile.Size;
	OutputFile.Data = new u8[OutputFile.Size];

	StartTime = timer();
	DecompressFileOrder0(Model, OutputFile, CompressBuffer, InputFile);
	EndTime = timer() - StartTime;
	printf("DecTime %.3f\n", EndTime);

	delete[] OutputFile.Data;
}

void
CompressFileOrder1(SimpleOrder1AC& Model, const file_data& InputFile, ByteVec& OutBuffer)
{
	ArithEncoder Encoder(OutBuffer);

	for (u32 i = 0; i < InputFile.Size; ++i)
	{
		prob SymbolProb = Model.getProb(InputFile.Data[i]);
		Encoder.encode(SymbolProb);
		Model.update(InputFile.Data[i]);
	}

	prob SymbolProb = Model.getEndStreamProb();
	Encoder.encode(SymbolProb);
	Encoder.flush();
}

void
DecompressFileOrder1(SimpleOrder1AC& Model, file_data& OutputFile, ByteVec& InputBuffer, const file_data& InputFile)
{
	ArithDecoder Decoder(InputBuffer);

	u64 ByteIndex = 0;
	for (;;)
	{
		u32 DecodedFreq = Decoder.getCurrFreq(Model.getTotal());

		u32 DecodedSymbol;
		prob Prob = Model.getSymbolFromFreq(DecodedFreq, &DecodedSymbol);

		if (DecodedSymbol == SimpleOrder1AC::EndOfStreamSymbolIndex) break;

		Assert(ByteIndex <= OutputFile.Size);
		Assert(InputFile.Data[ByteIndex] == DecodedSymbol);

		Decoder.updateDecodeRange(Prob);
		Model.update(DecodedSymbol);

		OutputFile.Data[ByteIndex++] = DecodedSymbol;
	}
}

void
TestSimpleOrder1AC(const file_data& InputFile)
{
	printf("--TestSimpleOrder1AC\n");

	SimpleOrder1AC Model;
	ByteVec CompressBuffer;

	f64 StartTime = timer();
	CompressFileOrder1(Model, InputFile, CompressBuffer);
	f64 EndTime = timer() - StartTime;

	Model.reset();
	u64 CompressedSize = CompressBuffer.size();

	PrintCompressionSize(InputFile.Size, CompressedSize);
	printf(" EncTime %.3f\n", EndTime);

	file_data OutputFile;
	OutputFile.Size = InputFile.Size;
	OutputFile.Data = new u8[OutputFile.Size];

	StartTime = timer();
	DecompressFileOrder1(Model, OutputFile, CompressBuffer, InputFile);
	EndTime = timer() - StartTime;
	printf(" DecTime %.3f\n", EndTime);

	delete[] OutputFile.Data;
}