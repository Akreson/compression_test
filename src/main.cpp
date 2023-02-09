#define _CRT_SECURE_NO_WARNINGS

#include <cmath>
#include "common.h"
#include "suballoc.cpp"

#include "ac/ac.cpp"
#include "ac/static_ac.cpp"
#include "ac/ppm_ac.cpp"
#include "ac_tests.cpp"

#include "ans/rans_common.h"
#include "ans/rans8.cpp"
#include "ans/rans16.cpp"
#include "ans/rans32.cpp"
#include "ans/static_basic_stats.cpp"

inline void
PrintRansPerfStats(u64 Clocks, f64 Time, u64 DataSize)
{
	printf(" %llu clocks, %.1f clocks/symbol (%5.1f MiB/s)\n", Clocks, 1.0 * Clocks / DataSize, 1.0 * DataSize / (Time * 1048576.0));
}

inline void
PrintAvgRansPerfStats(AccumTime Accum, u32 RunsCount, u64 DataSize)
{
	Accum.Clock /= RunsCount;
	Accum.Time /= (f64)RunsCount;
	printf(" avg of %d runs ", RunsCount);
	PrintRansPerfStats(Accum.Clock, Accum.Time, DataSize);
	printf("\n");
}

void
TestBasicRans8(file_data& InputFile)
{
	u32 RunsCount = 5;
	printf("TestBasicRans8\n");

	static constexpr u32 ProbBit = 14;
	static constexpr u32 ProbScale = 1 << ProbBit;

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

	u8* DecodeBegin = nullptr;

	AccumTime Accum;
	printf(" rANS encode\n");
	for (u32 Run = 0; Run < RunsCount; Run++)
	{
		f64 EncStartTime = timer();
		u64 EncStartClock = __rdtsc();

		Rans8Encoder Encoder;
		Encoder.init();

		u8* Out = OutBuff + BuffSize;
		for (u64 i = InputFile.Size; i > 0; i--)
		{
			u8 Symbol = InputFile.Data[i - 1];
			Encoder.encode(&Out, Stats.CumFreq[Symbol], Stats.Freq[Symbol], ProbBit);
		}
		Encoder.flush(&Out);
		DecodeBegin = Out;

		u64 EncClocks = __rdtsc() - EncStartClock;
		f64 EncTime = timer() - EncStartTime;
		Accum.Clock += EncClocks;
		Accum.Time += EncTime;
		//PrintRansPerfStats(EncClocks, EncTime, InputFile.Size);
	}

	PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);
	Accum.reset();

	u64 CompressedSize = (OutBuff + BuffSize) - DecodeBegin;
	printf(" %d bytes | %.3f ratio\n", CompressedSize, (f64)InputFile.Size / (f64)CompressedSize);

	printf(" rANS decode\n");
	for (u32 Run = 0; Run < RunsCount; Run++)
	{
		f64 DecStartTime = timer();
		u64 DecStartClock = __rdtsc();

		u8* In = DecodeBegin;

		Rans8Decoder Decoder;
		Decoder.init(&In);

		for (u64 i = 0; i < InputFile.Size; i++)
		{
			u32 CumFreq = Decoder.decodeGet(ProbBit);
			u32 Symbol = Cum2Sym[CumFreq];

			Assert(InputFile.Data[i] == Symbol);
			DecBuff[i] = Symbol;
			Decoder.decodeAdvance(&In, Stats.CumFreq[Symbol], Stats.Freq[Symbol], ProbBit);
		}

		u64 DecClocks = __rdtsc() - DecStartClock;
		f64 DecTime = timer() - DecStartTime;
		Accum.Clock += DecClocks;
		Accum.Time += DecTime;
		//PrintRansPerfStats(DecClocks, DecTime, InputFile.Size);
	}

	PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);

	delete[] OutBuff;
	delete[] DecBuff;
}

void
TestBasicRans32(file_data& InputFile)
{
	u32 RunsCount = 5;
	printf("TestBasicRans32\n");

	static constexpr u32 ProbBit = 14;
	static constexpr u32 ProbScale = 1 << ProbBit;

	u64 BuffSize = AlignSizeForward(InputFile.Size);
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

	u32* DecodeBegin = nullptr;

	AccumTime Accum;
	printf(" rANS encode\n");
	for (u32 Run = 0; Run < RunsCount; Run++)
	{
		f64 EncStartTime = timer();
		u64 EncStartClock = __rdtsc();

		Rans32Encoder Encoder;
		Encoder.init();

		u32* Out = reinterpret_cast<u32*>(OutBuff + BuffSize);
		for (u64 i = InputFile.Size; i > 0; i--)
		{
			u8 Symbol = InputFile.Data[i - 1];
			Encoder.encode(&Out, Stats.CumFreq[Symbol], Stats.Freq[Symbol], ProbBit);
		}
		Encoder.flush(&Out);
		DecodeBegin = Out;

		u64 EncClocks = __rdtsc() - EncStartClock;
		f64 EncTime = timer() - EncStartTime;
		Accum.Clock += EncClocks;
		Accum.Time += EncTime;
		//PrintRansPerfStats(EncClocks, EncTime, InputFile.Size);
	}

	PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);
	Accum.reset();

	u64 CompressedSize = (OutBuff + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
	printf(" %d bytes | %.3f ratio\n", CompressedSize, (f64)InputFile.Size / (f64)CompressedSize);

	printf(" rANS decode\n");
	for (u32 Run = 0; Run < RunsCount; Run++)
	{
		f64 DecStartTime = timer();
		u64 DecStartClock = __rdtsc();

		u32* In = DecodeBegin;
		Rans32Decoder Decoder;
		Decoder.init(&In);

		for (u64 i = 0; i < InputFile.Size; i++)
		{
			u32 CumFreq = Decoder.decodeGet(ProbBit);
			u32 Symbol = Cum2Sym[CumFreq];

			Assert(InputFile.Data[i] == Symbol);
			DecBuff[i] = Symbol;
			Decoder.decodeAdvance(&In, Stats.CumFreq[Symbol], Stats.Freq[Symbol], ProbBit);
		}

		u64 DecClocks = __rdtsc() - DecStartClock;
		f64 DecTime = timer() - DecStartTime;
		Accum.Clock += DecClocks;
		Accum.Time += DecTime;
		//PrintRansPerfStats(DecClocks, DecTime, InputFile.Size);
	}

	PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);

	delete[] OutBuff;
	delete[] DecBuff;
}

void
TestFastEncodeRans8(file_data& InputFile)
{
	u32 RunsCount = 5;
	printf("TestFastEncodeRans8\n");

	static constexpr u32 ProbBit = 14;
	static constexpr u32 ProbScale = 1 << ProbBit;

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

	rans_enc_sym32 EncSymArr[256];
	rans_dec_sym32 DecSymArr[256];

	for (int i = 0; i < 256; i++) {
		RansEncSymInit(&EncSymArr[i], Stats.CumFreq[i], Stats.Freq[i], ProbBit, Rans8L, 8);
		RansDecSymInit(&DecSymArr[i], Stats.CumFreq[i], Stats.Freq[i]);
	}

	u8* DecodeBegin = nullptr;

	AccumTime Accum;
	printf(" rANS encode\n");
	for (u32 Run = 0; Run < RunsCount; Run++)
	{
		f64 EncStartTime = timer();
		u64 EncStartClock = __rdtsc();

		Rans8Encoder Encoder;
		Encoder.init();

		u8* Out = OutBuff + BuffSize;
		for (u64 i = InputFile.Size; i > 0; i--)
		{
			u8 Symbol = InputFile.Data[i - 1];
			Encoder.encode(&Out, &EncSymArr[Symbol]);
		}
		Encoder.flush(&Out);
		DecodeBegin = Out;

		u64 EncClocks = __rdtsc() - EncStartClock;
		f64 EncTime = timer() - EncStartTime;
		Accum.Clock += EncClocks;
		Accum.Time += EncTime;
		//PrintRansPerfStats(EncClocks, EncTime, InputFile.Size);
	}

	PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);
	Accum.reset();

	u64 CompressedSize = (OutBuff + BuffSize) - DecodeBegin;
	printf(" %d bytes | %.3f ratio\n", CompressedSize, (f64)InputFile.Size / (f64)CompressedSize);

	printf(" rANS decode\n");
	for (u32 Run = 0; Run < RunsCount; Run++)
	{
		f64 DecStartTime = timer();
		u64 DecStartClock = __rdtsc();

		u8* In = DecodeBegin;
		Rans8Decoder Decoder;
		Decoder.init(&In);

		for (u64 i = 0; i < InputFile.Size; i++)
		{
			u32 CumFreq = Decoder.decodeGet(ProbBit);
			u32 Symbol = Cum2Sym[CumFreq];

			Assert(InputFile.Data[i] == Symbol);
			DecBuff[i] = Symbol;

			Decoder.decodeAdvance(&In, &DecSymArr[Symbol], ProbBit);
		}

		u64 DecClocks = __rdtsc() - DecStartClock;
		f64 DecTime = timer() - DecStartTime;
		Accum.Clock += DecClocks;
		Accum.Time += DecTime;
		//PrintRansPerfStats(DecClocks, DecTime, InputFile.Size);
	}

	PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);

	delete[] OutBuff;
	delete[] DecBuff;
}

void
TestFastEncodeRans32(file_data& InputFile)
{
	u32 RunsCount = 5;
	printf("TestFastEncodeRans32\n");

	static constexpr u32 ProbBit = 14;
	static constexpr u32 ProbScale = 1 << ProbBit;

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

	rans_enc_sym64 EncSymArr[256];
	rans_dec_sym64 DecSymArr[256];

	for (int i = 0; i < 256; i++) {
		RansEncSymInit(&EncSymArr[i], Stats.CumFreq[i], Stats.Freq[i], ProbBit);
		RansDecSymInit(&DecSymArr[i], Stats.CumFreq[i], Stats.Freq[i]);
	}

	u32* DecodeBegin = nullptr;

	AccumTime Accum;
	printf(" rANS encode\n");
	for (u32 Run = 0; Run < RunsCount; Run++)
	{
		f64 EncStartTime = timer();
		u64 EncStartClock = __rdtsc();

		Rans32Encoder Encoder;
		Encoder.init();

		u32* Out = reinterpret_cast<u32*>(OutBuff + BuffSize);
		for (u64 i = InputFile.Size; i > 0; i--)
		{
			u8 Symbol = InputFile.Data[i - 1];
			Encoder.encode(&Out, &EncSymArr[Symbol], ProbBit);
		}
		Encoder.flush(&Out);
		DecodeBegin = Out;

		u64 EncClocks = __rdtsc() - EncStartClock;
		f64 EncTime = timer() - EncStartTime;
		Accum.Clock += EncClocks;
		Accum.Time += EncTime;
		//PrintRansPerfStats(EncClocks, EncTime, InputFile.Size);
	}

	PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);
	Accum.reset();

	u64 CompressedSize = (OutBuff + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
	printf(" %d bytes | %.3f ratio\n", CompressedSize, (f64)InputFile.Size / (f64)CompressedSize);

	printf(" rANS decode\n");
	for (u32 Run = 0; Run < RunsCount; Run++)
	{
		f64 DecStartTime = timer();
		u64 DecStartClock = __rdtsc();

		u32* In = DecodeBegin;

		Rans32Decoder Decoder;
		Decoder.init(&In);

		for (u64 i = 0; i < InputFile.Size; i++)
		{
			u32 CumFreq = Decoder.decodeGet(ProbBit);
			u32 Symbol = Cum2Sym[CumFreq];

			Assert(InputFile.Data[i] == Symbol);
			DecBuff[i] = Symbol;

			Decoder.decodeAdvance(&In, &DecSymArr[Symbol], ProbBit);
		}

		u64 DecClocks = __rdtsc() - DecStartClock;
		f64 DecTime = timer() - DecStartTime;
		Accum.Clock += DecClocks;
		Accum.Time += DecTime;
		//PrintRansPerfStats(DecClocks, DecTime, InputFile.Size);
	}

	PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);

	delete[] OutBuff;
	delete[] DecBuff;
}

void 
TestTableDecodeRans16(file_data& InputFile)
{
	u32 RunsCount = 5;
	printf("TestTableDecodeRans16\n");

	static constexpr u32 ProbBit = 14;
	static constexpr u32 ProbScale = 1 << ProbBit;

	u64 BuffSize = AlignSizeForward(InputFile.Size);
	u8* OutBuff = new u8[BuffSize];
	u8* DecBuff = new u8[BuffSize];

	SymbolStats Stats;
	Stats.countSymbol(InputFile.Data, InputFile.Size);
	Stats.normalize(ProbScale);

	rans_sym_table<ProbScale> Tab;

	for (u32 i = 0; i < 256; i++)
	{
		RansTableInitSym(Tab, i, Stats.CumFreq[i], Stats.Freq[i]);
	}

	u16* DecodeBegin = nullptr;

	AccumTime Accum;
	printf(" rANS encode\n");
	for (u32 Run = 0; Run < RunsCount; Run++)
	{
		f64 EncStartTime = timer();
		u64 EncStartClock = __rdtsc();

		Rans16Encoder Encoder;
		Encoder.init();

		u16* Out = reinterpret_cast<u16*>(OutBuff + BuffSize);
		for (u64 i = InputFile.Size; i > 0; i--)
		{
			u8 Symbol = InputFile.Data[i - 1];
			Encoder.encode(&Out, Stats.CumFreq[Symbol], Stats.Freq[Symbol], ProbBit);
		}
		Encoder.flush(&Out);
		DecodeBegin = Out;

		u64 EncClocks = __rdtsc() - EncStartClock;
		f64 EncTime = timer() - EncStartTime;
		Accum.Clock += EncClocks;
		Accum.Time += EncTime;
		//PrintRansPerfStats(EncClocks, EncTime, InputFile.Size);
	}

	PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);
	Accum.reset();

	u64 CompressedSize = (OutBuff + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
	printf(" %d bytes | %.3f ratio\n", CompressedSize, (f64)InputFile.Size / (f64)CompressedSize);

	printf(" rANS decode\n");
	for (u32 Run = 0; Run < RunsCount; Run++)
	{
		f64 DecStartTime = timer();
		u64 DecStartClock = __rdtsc();

		u16* In = DecodeBegin;

		Rans16Decoder Decoder;
		Decoder.init(&In);

		for (u64 i = 0; i < InputFile.Size; i++)
		{
			u8 Symbol = Decoder.decodeSym(Tab, ProbScale, ProbBit);

			Assert(InputFile.Data[i] == Symbol);

			DecBuff[i] = Symbol;
			Decoder.decodeRenorm(&In);
		}

		u64 DecClocks = __rdtsc() - DecStartClock;
		f64 DecTime = timer() - DecStartTime;
		Accum.Clock += DecClocks;
		Accum.Time += DecTime;
		//PrintRansPerfStats(DecClocks, DecTime, InputFile.Size);
	}

	PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);

	delete[] OutBuff;
	delete[] DecBuff;
}

void
TestTableInterleavedRans16(file_data& InputFile)
{
	u32 RunsCount = 10;
	printf("TestInterleavedRans16\n");

	static constexpr u32 ProbBit = 12;
	static constexpr u32 ProbScale = 1 << ProbBit;

	u64 BuffSize = AlignSizeForward(InputFile.Size);
	u8* OutBuff = new u8[BuffSize];
	u8* DecBuff = new u8[BuffSize];

	SymbolStats Stats;
	Stats.countSymbol(InputFile.Data, InputFile.Size);
	Stats.normalize(ProbScale);

	rans_sym_table<ProbScale> Tab;

	for (u32 i = 0; i < 256; i++)
	{
		RansTableInitSym(Tab, i, Stats.CumFreq[i], Stats.Freq[i]);
	}

	u16* DecodeBegin = nullptr;

	AccumTime Accum;
	printf(" rANS encode\n");
	for (u32 Run = 0; Run < RunsCount; Run++)
	{
		f64 EncStartTime = timer();
		u64 EncStartClock = __rdtsc();

		u16* Out = reinterpret_cast<u16*>(OutBuff + BuffSize);

		Rans16Encoder Enc0, Enc1;
		Enc0.init();
		Enc1.init();

		if (InputFile.Size & 1)
		{
			u8 Symbol = InputFile.Data[InputFile.Size - 1];
			Enc0.encode(&Out, Stats.CumFreq[Symbol], Stats.Freq[Symbol], ProbBit);
		}

		for (u64 i = (InputFile.Size & ~1); i > 0; i -= 2)
		{
			u8 Symbol1 = InputFile.Data[i - 1];
			u8 Symbol0 = InputFile.Data[i - 2];
			Enc1.encode(&Out, Stats.CumFreq[Symbol1], Stats.Freq[Symbol1], ProbBit);
			Enc0.encode(&Out, Stats.CumFreq[Symbol0], Stats.Freq[Symbol0], ProbBit);
		}
		Enc1.flush(&Out);
		Enc0.flush(&Out);
		DecodeBegin = Out;

		u64 EncClocks = __rdtsc() - EncStartClock;
		f64 EncTime = timer() - EncStartTime;
		Accum.Clock += EncClocks;
		Accum.Time += EncTime;
		PrintRansPerfStats(EncClocks, EncTime, InputFile.Size);
	}

	//PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);
	Accum.reset();

	u64 CompressedSize = (OutBuff + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
	printf(" %d bytes | %.3f ratio\n", CompressedSize, (f64)InputFile.Size / (f64)CompressedSize);

	printf(" rANS decode\n");
	for (u32 Run = 0; Run < RunsCount; Run++)
	{
		f64 DecStartTime = timer();
		u64 DecStartClock = __rdtsc();

		u16* In = DecodeBegin;

		Rans16Decoder Dec0, Dec1;
		Dec0.init(&In);
		Dec1.init(&In);

		for (u64 i = 0; i < (InputFile.Size & ~1); i += 2)
		{
			u8 Symbol0 = Dec0.decodeSym(Tab, ProbScale, ProbBit);
			u8 Symbol1 = Dec1.decodeSym(Tab, ProbScale, ProbBit);

			Assert(InputFile.Data[i] == Symbol0);
			Assert(InputFile.Data[i+1] == Symbol1);

			DecBuff[i] = Symbol0;
			DecBuff[i+1] = Symbol1;

			Dec0.decodeRenorm(&In);
			Dec1.decodeRenorm(&In);
		}

		if (InputFile.Size & 1)
		{
			u8 Symbol = Dec0.decodeSym(Tab, ProbScale, ProbBit);
			InputFile.Data[InputFile.Size - 1] = Symbol;
		}

		u64 DecClocks = __rdtsc() - DecStartClock;
		f64 DecTime = timer() - DecStartTime;
		Accum.Clock += DecClocks;
		Accum.Time += DecTime;
		PrintRansPerfStats(DecClocks, DecTime, InputFile.Size);
	}

	PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);

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
	
	//TestBasicRans8(InputFile);
	//TestBasicRans32(InputFile);
	//TestFastEncodeRans8(InputFile);
	//TestFastEncodeRans32(InputFile);
	//TestTableDecodeRans16(InputFile);
	TestTableInterleavedRans16(InputFile);

	return 0;
}