#define _CRT_SECURE_NO_WARNINGS

#include <cmath>
#include "common.h"
#include "suballoc.cpp"

#include "ac/ac.cpp"
#include "ac/static_ac.cpp"
#include "ac/ppm_ac.cpp"
#include "ac_tests.cpp"

#include "ans/rans_byte.cpp"
#include "ans/static_basic_stats.cpp"

void
TestBasicRansByte(file_data& InputFile)
{
	u64 BuffSize = InputFile.Size;
	u8* OutBuff = new u8[BuffSize];
	u8* DecBuff = new u8[BuffSize];

	SymbolStats Stats;
	Stats.countSymbol(InputFile.Data, InputFile.Size);
	Stats.normalize(ProbScale);

	u8 Cum2Sym[ProbScale];
	for (u32 SymbolIndex = 0; SymbolIndex < 256; SymbolIndex++)
	{
		for (u32 j = Stats.CumFreq[SymbolIndex]; j < Stats.CumFreq[SymbolIndex + 1]; j++)
		{
			Cum2Sym[j] = SymbolIndex;
		}
	}

	RansByteEncoder Encoder;
	Encoder.init();

	u8* Out = OutBuff + BuffSize;
	for (u64 i = InputFile.Size; i > 0; i--)
	{
		u8 Byte = InputFile.Data[i - 1];
		Encoder.encode(&Out, Stats.CumFreq[Byte], Stats.Freq[Byte], ProbBit);
	}
	Encoder.flush(&Out);

	u8* DecodeBegin = Out;

	u64 CompressedSize = OutBuff + BuffSize - DecodeBegin;
	printf("compression ratio %.3f\n", (f64)InputFile.Size / (f64)CompressedSize);

	RansByteDecoder Decoder;
	Decoder.init(&DecodeBegin);

	for (u64 i = 0; i < BuffSize; i++)
	{
		u32 CumFreq = Decoder.decodeGet(ProbBit);
		u32 Symbol = Cum2Sym[CumFreq];

		Assert(InputFile.Data[i] == Symbol)
		DecBuff[i] = Symbol;
		Decoder.decodeAdvance(&DecodeBegin, Stats.CumFreq[Symbol], Stats.Freq[Symbol], ProbBit);
	}
	
	delete[] OutBuff;
	delete[] DecBuff;
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
	//TestPPMModel(InputFile);
	TestBasicRansByte(InputFile);

	return 0;
}