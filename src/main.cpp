#define _CRT_SECURE_NO_WARNINGS

#include <cmath>
#include "common.h"
#include "mem.cpp"
#include "suballoc.cpp"

#include "huff_tests.cpp"
#include "ac_tests.cpp"

int
main(int argc, char** argv)
{
	if (argc < 2)
	{
		printf("file name missed\n");
		exit(0);
	}

	std::vector<file_data> InputArr;
	ReadTestFiles(InputArr, argv[1]);

	size_t ByteCount[256] = {};

	for (const auto& InputFile : InputArr)
	{
		CountByte(ByteCount, InputFile.Data, InputFile.Size);
		f64 FileByteH = Entropy(ByteCount, 256);
		printf("---------- %s %lu H:%.3f\n", InputFile.Name.c_str(), InputFile.Size, FileByteH);
		
		//TestHuff1(InputFile);
		//TestStaticAC(InputFile);
		//TestOrder0AC(InputFile);
		//TestSimpleOrder1AC(InputFile);
		TestPPMModel(InputFile);

		ZeroSize(ByteCount, sizeof(ByteCount));
		printf("\n");
	}
}