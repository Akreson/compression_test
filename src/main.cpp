#define _CRT_SECURE_NO_WARNINGS

#include "common.h"
#include "ac.cpp"
#include "static_ac.cpp"
#include "ppm_ac.cpp"

void
CompressFile(StaticByteModel& Model, file_data& InputFile, ByteVec& OutBuffer)
{
	ArithEncoder Encoder(OutBuffer);

	for (u32 i = 0; i <= InputFile.Size; ++i)
	{
		prob SymbolProb = Model.getProb(InputFile.Data[i]);
		Encoder.encode(SymbolProb);
		Model.update(InputFile.Data[i]);
	}

	prob SymbolProb = Model.getEndStreamProb();
	Encoder.encode(SymbolProb);
}

void 
DecompressFile(StaticByteModel& Model, file_data& OutputFile, ByteVec& InputBuffer)
{
	ArithDecoder Decoder(InputBuffer);

	u64 ByteIndex = 0;
	for (;;)
	{
		u32 DecodedFreq = Decoder.getCurrFreq(Model.getCount());

		u32 DecodedSymbol;
		prob Prob = Model.getByteFromFreq(DecodedFreq, &DecodedSymbol);

		if (DecodedSymbol == StaticByteModel::EndOfStreamSymbolIndex) break;

		Decoder.updateDecodeRange(Prob);
		Model.update(DecodedSymbol);

		Assert(ByteIndex <= OutputFile.Size);
		OutputFile.Data[ByteIndex++] = DecodedSymbol;
	}
}

void
TestStaticModel(file_data& InputFile)
{
	StaticByteModel Model;
	ByteVec CompressBuffer;

	CompressFile(Model, InputFile, CompressBuffer);
	u64 CompressedSize = CompressBuffer.size();

	Model.reset();

	file_data OutputFile;
	OutputFile.Size = InputFile.Size;
	OutputFile.Data = new u8[OutputFile.Size];

	DecompressFile(Model, OutputFile, CompressBuffer);

	for (u32 i = 0; i <= InputFile.Size; ++i)
	{
		b32 cmp = InputFile.Data[i] == OutputFile.Data[i];
		Assert(cmp);
	}

	delete[] OutputFile.Data;
}

void
CompressFile(PPMByte& Model, file_data& InputFile, ByteVec& OutBuffer)
{
	ArithEncoder Encoder(OutBuffer);

	for (u32 i = 0; i <= InputFile.Size; ++i)
	{
		Model.encode(Encoder, InputFile.Data[i]);
	}

	Model.encodeEndOfStream(Encoder);
	printf("ctx - %d | mem %d B _ %d KiB _ %d MiB\n",
		Model.ContextCount, Model.MemUse, Model.MemUse / 1024, Model.MemUse / 1024 / 1024);
}

void
DecompressFile(PPMByte& Model, file_data& OutputFile, ByteVec& InputBuffer, file_data& InputFile)
{
	ArithDecoder Decoder(InputBuffer);

	u64 ByteIndex = 0;
	for (;;)
	{
		u32 DecodedSymbol = Model.decode(Decoder);
		if (DecodedSymbol == PPMByte::EscapeSymbol) break;

		Assert(ByteIndex <= OutputFile.Size);
		Assert(InputFile.Data[ByteIndex] == DecodedSymbol)
		OutputFile.Data[ByteIndex++] = DecodedSymbol;
	}
}

void
TestPPMModel(file_data& InputFile)
{
	PPMByte PPMModel(3);
	ByteVec CompressBuffer;

	CompressFile(PPMModel, InputFile, CompressBuffer);

	u64 CompressedSize = CompressBuffer.size();
	printf("compression ratio %.3f", (double)CompressedSize / (double)InputFile.Size);

	PPMModel.reset();

	file_data OutputFile;
	OutputFile.Size = InputFile.Size;
	OutputFile.Data = new u8[OutputFile.Size];

	DecompressFile(PPMModel, OutputFile, CompressBuffer, InputFile);

	delete[] OutputFile.Data;
}

int
main(int argc, char** argv)
{
	if (argc < 2)
	{
		printf("file name missed");
		exit(0);
	}

	file_data InputFile = ReadFile(argv[1]);

	//TestStaticModel(InputFile);
	TestPPMModel(InputFile);

	return 0;
}