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
#include "ans/renorm.cpp"
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
#if 1
	printf(" min of %d runs ", RunsCount);
	PrintRansPerfStats(Accum.MinClock, Accum.MinTime, DataSize);
	printf(" max of %d runs ", RunsCount);
	PrintRansPerfStats(Accum.MaxClock, Accum.MaxTime, DataSize);
#endif
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

		Rans8Enc Encoder;
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
		Accum.update(EncClocks, EncTime);
		//PrintRansPerfStats(EncClocks, EncTime, InputFile.Size);
	}

	PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);
	Accum.reset();

	u64 CompressedSize = (OutBuff + BuffSize) - DecodeBegin;
	PrintCompressionSize(InputFile.Size, CompressedSize);

	printf(" rANS decode\n");
	for (u32 Run = 0; Run < RunsCount; Run++)
	{
		f64 DecStartTime = timer();
		u64 DecStartClock = __rdtsc();

		u8* In = DecodeBegin;

		Rans8Dec Decoder;
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
		Accum.update(DecClocks, DecTime);
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

		Rans32Enc Encoder;
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
		Accum.update(EncClocks, EncTime);
		//PrintRansPerfStats(EncClocks, EncTime, InputFile.Size);
	}

	PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);
	Accum.reset();

	u64 CompressedSize = (OutBuff + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
	PrintCompressionSize(InputFile.Size, CompressedSize);

	printf(" rANS decode\n");
	for (u32 Run = 0; Run < RunsCount; Run++)
	{
		f64 DecStartTime = timer();
		u64 DecStartClock = __rdtsc();

		u32* In = DecodeBegin;
		Rans32Dec Decoder;
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
		Accum.update(DecClocks, DecTime);
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

		Rans8Enc Encoder;
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
		Accum.update(EncClocks, EncTime);
		//PrintRansPerfStats(EncClocks, EncTime, InputFile.Size);
	}

	PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);
	Accum.reset();

	u64 CompressedSize = (OutBuff + BuffSize) - DecodeBegin;
	PrintCompressionSize(InputFile.Size, CompressedSize);

	printf(" rANS decode\n");
	for (u32 Run = 0; Run < RunsCount; Run++)
	{
		f64 DecStartTime = timer();
		u64 DecStartClock = __rdtsc();

		u8* In = DecodeBegin;
		Rans8Dec Decoder;
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
		Accum.update(DecClocks, DecTime);
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

		Rans32Enc Encoder;
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
		Accum.update(EncClocks, EncTime);
		//PrintRansPerfStats(EncClocks, EncTime, InputFile.Size);
	}

	PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);
	Accum.reset();

	u64 CompressedSize = (OutBuff + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
	PrintCompressionSize(InputFile.Size, CompressedSize);

	printf(" rANS decode\n");
	for (u32 Run = 0; Run < RunsCount; Run++)
	{
		f64 DecStartTime = timer();
		u64 DecStartClock = __rdtsc();

		u32* In = DecodeBegin;

		Rans32Dec Decoder;
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
		Accum.update(DecClocks, DecTime);
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

		Rans16Enc Encoder;
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
		Accum.update(EncClocks, EncTime);
		//PrintRansPerfStats(EncClocks, EncTime, InputFile.Size);
	}

	PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);
	Accum.reset();

	u64 CompressedSize = (OutBuff + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
	PrintCompressionSize(InputFile.Size, CompressedSize);

	printf(" rANS decode\n");
	for (u32 Run = 0; Run < RunsCount; Run++)
	{
		f64 DecStartTime = timer();
		u64 DecStartClock = __rdtsc();

		u16* In = DecodeBegin;

		Rans16Dec Decoder;
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
		Accum.update(DecClocks, DecTime);
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
	printf("TestTableInterleavedRans16\n");

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

		Rans16Enc Enc0, Enc1;
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
		Accum.update(EncClocks, EncTime);
		//PrintRansPerfStats(EncClocks, EncTime, InputFile.Size);
	}

	PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);
	Accum.reset();

	u64 CompressedSize = (OutBuff + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
	PrintCompressionSize(InputFile.Size, CompressedSize);

	printf(" rANS decode\n");
	for (u32 Run = 0; Run < RunsCount; Run++)
	{
		f64 DecStartTime = timer();
		u64 DecStartClock = __rdtsc();

		u16* In = DecodeBegin;

		Rans16Dec Dec0, Dec1;
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
		Accum.update(DecClocks, DecTime);
		//PrintRansPerfStats(DecClocks, DecTime, InputFile.Size);
	}

	PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);

	delete[] OutBuff;
	delete[] DecBuff;
}

void
TestTableInterleavedRans32(file_data& InputFile)
{
	u32 RunsCount = 10;
	printf("TestTableInterleavedRans32\n");

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

	u32* DecodeBegin = nullptr;

	AccumTime Accum;
	printf(" rANS encode\n");
	for (u32 Run = 0; Run < RunsCount; Run++)
	{
		f64 EncStartTime = timer();
		u64 EncStartClock = __rdtsc();

		u32* Out = reinterpret_cast<u32*>(OutBuff + BuffSize);

		Rans32Enc Enc0, Enc1;
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
		Accum.update(EncClocks, EncTime);
		//PrintRansPerfStats(EncClocks, EncTime, InputFile.Size);
	}

	PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);
	Accum.reset();

	u64 CompressedSize = (OutBuff + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
	PrintCompressionSize(InputFile.Size, CompressedSize);

	printf(" rANS decode\n");
	for (u32 Run = 0; Run < RunsCount; Run++)
	{
		f64 DecStartTime = timer();
		u64 DecStartClock = __rdtsc();

		u32* In = DecodeBegin;

		Rans32Dec Dec0, Dec1;
		Dec0.init(&In);
		Dec1.init(&In);

		for (u64 i = 0; i < (InputFile.Size & ~1); i += 2)
		{
			u8 Symbol0 = Dec0.decodeSym(Tab, ProbScale, ProbBit);
			u8 Symbol1 = Dec1.decodeSym(Tab, ProbScale, ProbBit);

			Assert(InputFile.Data[i] == Symbol0);
			Assert(InputFile.Data[i + 1] == Symbol1);

			DecBuff[i] = Symbol0;
			DecBuff[i + 1] = Symbol1;

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
		Accum.update(DecClocks, DecTime);
		//PrintRansPerfStats(DecClocks, DecTime, InputFile.Size);
	}

	PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);

	delete[] OutBuff;
	delete[] DecBuff;
}

#define Check4SymDecBuff(Buff, i) ((u32)(Buff[(i)] | (Buff[(i) + 1] << 8) | (Buff[(i) + 2] << 16) | (Buff[(i) + 3] << 24)))

void
TestSIMDDecodeRans16(file_data& InputFile)
{
	u32 RunsCount = 10;
	printf("TestSIMDDecodeRans16\n");

	static constexpr u32 ProbBit = 12;
	static constexpr u32 ProbScale = 1 << ProbBit;

	// align buffer size
	u64 BuffSize = AlignSizeForward(InputFile.Size, 16);
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

		Rans16Enc Enc[8];
		for (u32 i = 0; i < 8; i++) Enc[i].init();

		u16* Out = reinterpret_cast<u16*>(OutBuff + BuffSize);
		for (u64 i = InputFile.Size; i > 0; i--)
		{
			u8 Symbol = InputFile.Data[i - 1];
			u32 Index = (i - 1) & 7;
			Enc[Index].encode(&Out, Stats.CumFreq[Symbol], Stats.Freq[Symbol], ProbBit);
		}

		for (u32 i = 8; i > 0; i--) Enc[i - 1].flush(&Out);
		DecodeBegin = Out;

		u64 EncClocks = __rdtsc() - EncStartClock;
		f64 EncTime = timer() - EncStartTime;
		Accum.update(EncClocks, EncTime);
		//PrintRansPerfStats(EncClocks, EncTime, InputFile.Size);
	}

	PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);
	Accum.reset();

	u64 CompressedSize = (OutBuff + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
	PrintCompressionSize(InputFile.Size, CompressedSize);

	printf(" rANS decode\n");
	for (u32 Run = 0; Run < RunsCount; Run++)
	{
		f64 DecStartTime = timer();
		u64 DecStartClock = __rdtsc();

		u16* In = DecodeBegin;

		Rans16DecSIMD Dec0, Dec1;
		Dec0.init(&In);
		Dec1.init(&In);

		for (u64 i = 0; i < (InputFile.Size & ~7); i += 8)
		{
			u32 Symbol03 = Dec0.decodeSym(Tab, ProbScale, ProbBit);
			u32 Symbol47 = Dec1.decodeSym(Tab, ProbScale, ProbBit);
			
			Assert(Symbol03 == Check4SymDecBuff(InputFile.Data, i));
			Assert(Symbol47 == Check4SymDecBuff(InputFile.Data, i + 4));
			*reinterpret_cast<u32*>(DecBuff + i) = Symbol03;
			*reinterpret_cast<u32*>(DecBuff + i + 4) = Symbol47;

			Dec0.decodeRenorm(&In);
			Dec1.decodeRenorm(&In);
		}

		for (u64 i = InputFile.Size & ~7; i < InputFile.Size; i++)
		{
			Rans16DecSIMD* DecSIMD = (i & 4) != 0 ? &Dec1 : &Dec0;
			Rans16Dec* Dec = DecSIMD->State.lane + (i & 3);
			u8 Symbol = Dec->decodeSym(Tab, ProbScale, ProbBit);
			DecBuff[i] = Symbol;
		}

		u64 DecClocks = __rdtsc() - DecStartClock;
		f64 DecTime = timer() - DecStartTime;
		Accum.update(DecClocks, DecTime);
		//PrintRansPerfStats(DecClocks, DecTime, InputFile.Size);
	}

	PrintAvgRansPerfStats(Accum, RunsCount, InputFile.Size);

	delete[] OutBuff;
	delete[] DecBuff;
}

static inline u32*
EncodeRans32(u32* Out, u8* Data, u64 Size, u16* Freq, u16* CumFreq, u32 ProbBit)
{
	Rans32Enc Encoder;
	Encoder.init();

	for (u64 i = Size; i > 0; i--)
	{
		u8 Symbol = Data[i - 1];
		Encoder.encode(&Out, (u32)CumFreq[Symbol], (u32)Freq[Symbol], ProbBit);
	}
	Encoder.flush(&Out);

	return Out;
}

static inline void
DecodeRans32(u8* RefData, u32 RefSize, u8* DecBuff, u32* DecodeBegin, u16* Freq, u16* CumFreq, u32 ProbBit)
{
	u32 ProbScale = 1 << ProbBit;
	u8* Cum2Sym = new u8[ProbScale];
	for (u32 SymbolIndex = 0; SymbolIndex < 256; SymbolIndex++)
	{
		for (u32 j = CumFreq[SymbolIndex]; j < CumFreq[SymbolIndex + 1]; j++)
		{
			Cum2Sym[j] = SymbolIndex;
		}
	}

	u32* In = DecodeBegin;
	Rans32Dec Decoder;
	Decoder.init(&In);

	u64 ByteIndex = 0;
	while (ByteIndex < RefSize)
	{
		u32 CumFreqVal = Decoder.decodeGet(ProbBit);
		u8 Symbol = Cum2Sym[CumFreqVal];

		Assert(RefData[ByteIndex] == Symbol);

		DecBuff[ByteIndex++] = Symbol;
		Decoder.decodeAdvance(&In, CumFreq[Symbol], Freq[Symbol], ProbBit);
	}
}

void
TestNormalizationRans32(file_data& InputFile)
{
	u64 BuffSize = AlignSizeForward(InputFile.Size);
	u8* OutBuff = new u8[BuffSize];
	u8* DecBuff = new u8[BuffSize];
	u32* Out = reinterpret_cast<u32*>(OutBuff + BuffSize);

	u32 RawFreq[256] = {};
	u32 CumFreq32[257] = {};
	CountByte(RawFreq, InputFile.Data, InputFile.Size);
	CalcCumFreq(RawFreq, CumFreq32, 256);

	u32 TotalSum = 0;
	for (u32 i = 0; i < 256; i++)
	{
		TotalSum += RawFreq[i];
	}

	u16 NormFreq[256];
	u16 CumFreq[257];

	u32 TableLogSize[] = {14, 13, 12, 11, 10};
	for (u32 i = 0; i < ArrayCount(TableLogSize); i++)
	{
		printf("tableLog %d\n", TableLogSize[i]);
		u32 ProbBit = TableLogSize[i];
		u32 ProbScale = 1 << ProbBit;

		//tutorial normalize
		ZeroSize(NormFreq, sizeof(NormFreq));
		ZeroSize(CumFreq, sizeof(CumFreq));

		Normalize(RawFreq, CumFreq32, NormFreq, CumFreq, 256, ProbScale);

		u32* DecodeBegin = EncodeRans32(Out, InputFile.Data, InputFile.Size, NormFreq, CumFreq, ProbBit);

		u64 CompressedSizeNorm = (OutBuff + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
		DecodeRans32(InputFile.Data, InputFile.Size, DecBuff, DecodeBegin, NormFreq, CumFreq, ProbBit); // decode ok?

		printf("(norm) ");
		PrintCompressionSize(InputFile.Size, CompressedSizeNorm);

		//fast normalize
		ZeroSize(NormFreq, sizeof(NormFreq));
		ZeroSize(CumFreq, sizeof(CumFreq));

		FastNormalize(RawFreq, NormFreq, InputFile.Size, 256, ProbBit);
		CalcCumFreq(NormFreq, CumFreq, 256);

		DecodeBegin = EncodeRans32(Out, InputFile.Data, InputFile.Size, NormFreq, CumFreq, ProbBit);

		u64 CompressedSizeFast = (OutBuff + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
		DecodeRans32(InputFile.Data, InputFile.Size, DecBuff, DecodeBegin, NormFreq, CumFreq, ProbBit); // decode ok?

		printf("(fast) ");
		PrintCompressionSize(InputFile.Size, CompressedSizeFast);

		// optimal normalize
		ZeroSize(NormFreq, sizeof(NormFreq));
		ZeroSize(CumFreq, sizeof(CumFreq));

		OptimalNormalize(RawFreq, NormFreq, TotalSum, 256, ProbScale);
		CalcCumFreq(NormFreq, CumFreq, 256);

		DecodeBegin = EncodeRans32(Out, InputFile.Data, InputFile.Size, NormFreq, CumFreq, ProbBit);

		u64 CompressedSizeOpt = (OutBuff + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
		DecodeRans32(InputFile.Data, InputFile.Size, DecBuff, DecodeBegin, NormFreq, CumFreq, ProbBit); // decode ok?

		printf("(optm) ");
		PrintCompressionSize(InputFile.Size, CompressedSizeOpt);

		printf("\n");
	}
}

typedef array2d<u32, 256, 256> freq_val_o1;
typedef array2d<u16, 256, 257> cdf_val_o1;

void
PrecomputCDFFromEntireData(u8* Data, u64 Size, cdf_val_o1* MixCDF, u32 TargetTotalLog, u32 SymCount)
{
	u16* TempNormFreq = new u16[SymCount];
	u32* Total = new u32[SymCount];
	freq_val_o1* Order1Freq = new freq_val_o1;

	ZeroSize(TempNormFreq, sizeof(u16) * SymCount);

	for (u32 i = 0; i < freq_val_o1::d0; i++)
	{
		u32* Arr = &Order1Freq->E[i][0];
		MemSet<u32>(Arr, SymCount, 1);
	}
	MemSet<u32>(Total, SymCount, SymCount);

	u32 Inc = 16;
	for (u32 i = 1; i < Size; i++)
	{
		Order1Freq->E[Data[i - 1]][Data[i]] += Inc;
		Total[Data[i - 1]] += Inc;
	}

	for (u32 i = 0; i < SymCount; i++)
	{
		FastNormalize(Order1Freq->E[i], TempNormFreq, Total[i], SymCount, TargetTotalLog);
		CalcCumFreq(TempNormFreq, MixCDF->E[i], SymCount);
		ZeroSize(TempNormFreq, sizeof(u16)*SymCount);
	}

	delete TempNormFreq;
	delete Order1Freq;
}

void
AdaptFromMixCDF(u16* CDF, u16* AdaptCDF, u32 AdaptRate, u32 SymCount)
{
	for (u32 i = 1; i < SymCount; i++)
	{
		s16 NewCDF = ((s16)AdaptCDF[i] - (s16)CDF[i]) >> AdaptRate;
		CDF[i] = ((s16)CDF[i] + NewCDF);
		Assert(CDF[i] >= CDF[i - 1]);
	}
}

b32
CheckCDF(u16* CDF, u32 SymCount, u32 TargetTotal)
{
	u32 Total = 0;
	for (u32 i = 1; i < (SymCount + 1); i++)
	{
		Total += CDF[i] - CDF[i - 1];
	}

	return Total == TargetTotal;
}

void
InitEqDistCDF(u16 * CDF, u32 SymCount, u32 ProbScale)
{
	const u16 InitCDFValue = ProbScale / SymCount;
	
	CDF[0] = 0;
	for (u32 i = 1; i < (SymCount + 1); i++)
	{
		CDF[i] = CDF[i - 1] + InitCDFValue;
	}
	Assert(CDF[SymCount] == ProbScale);
}

// mixing CDF test (this test was setup for fun, require tuning parameters to beat static model,
// this parameters was used for book1 to compress it to 2.039 ratio)
void
TestPrecomputeAdaptiveOrder1Rans32(file_data& InputFile)
{
	const u32 AdaptRate = 1;
	const u32 SymCount = 256;
	const u32 ProbBit = 14;
	const u32 ProbScale = 1 << ProbBit;
	const u32 CDFSize = sizeof(u16) * (SymCount+1);

	Assert(ProbBit <= 15); // for ease u16 -> s16 interpretation

	// init data
	u16* CDF = new u16[SymCount + 1];
	u16* InitCDF = new u16[SymCount + 1];
	cdf_val_o1* MixCDF = new cdf_val_o1;

	InitEqDistCDF(InitCDF, SymCount, ProbScale);
	PrecomputCDFFromEntireData(InputFile.Data, InputFile.Size, MixCDF, ProbBit, SymCount);

	// collect adapted rans sym
	const u32 SplitSize = 1 << 16;
	Assert(IsPowerOf2(SplitSize));

	u64 Start = 0;
	u64 End = SplitSize < InputFile.Size ? SplitSize : InputFile.Size;

	std::vector<rans_sym> BuffRans;
	for (;;)
	{
		Copy(CDFSize, CDF, InitCDF);
		
		for (u64 i = Start; i < End; i++)
		{
			rans_sym R;
		
			u8 Sym = InputFile.Data[i];
			R.Start = CDF[Sym];
			R.Freq = CDF[Sym + 1] - R.Start;
			Assert(R.Freq);

			BuffRans.push_back(R);

			AdaptFromMixCDF(CDF, MixCDF->E[Sym], AdaptRate, SymCount);
			Assert(CheckCDF(CDF, SymCount, ProbScale));
		}

		if (End == InputFile.Size) break;

		Start += SplitSize;
		End += SplitSize;
		End = End < InputFile.Size ? End : InputFile.Size;
	}

	//encoding
	u64 BuffSize = AlignSizeForward(InputFile.Size);
	u8* OutBuff = new u8[BuffSize];
	u8* DecBuff = new u8[BuffSize];
	u32* Out = reinterpret_cast<u32*>(OutBuff + BuffSize);

	u32* DecodeBegin = nullptr;
	Rans32Enc Encoder;
	Encoder.init();

	u32 ToFlush = InputFile.Size / SplitSize;
	ToFlush = InputFile.Size - (ToFlush * SplitSize);

	for (u64 i = BuffRans.size(); i > 0; i--)
	{
		rans_sym& R = BuffRans.back();
		Encoder.encode(&Out, (u32)R.Start, (u32)R.Freq, ProbBit);
		BuffRans.pop_back();

		if (--ToFlush == 0)
		{
			Encoder.flush(&Out);
			Encoder.init();
			ToFlush = SplitSize;
		}
	}
	DecodeBegin = Out;

	u64 CompressedSize = (OutBuff + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
	PrintCompressionSize(InputFile.Size, CompressedSize);

	//decoding
	Copy(CDFSize, CDF, InitCDF);

	u32* In = DecodeBegin;
	Rans32Dec Decoder;
	Decoder.init(&In);

	u64 ByteIndex = 0;
	while (ByteIndex < InputFile.Size)
	{
		u8 Sym;
		u16 CumStart;
		u32 CumFreqVal = Decoder.decodeGet(ProbBit);

		for (u32 i = 0; i < (SymCount + 1); i++)
		{
			if (CDF[i] > CumFreqVal)
			{
				Sym = i - 1;
				CumStart = CDF[Sym];
				break;
			}
		}

		Assert(InputFile.Data[ByteIndex] == Sym);
		DecBuff[ByteIndex++] = Sym;

		u32 Freq = CDF[Sym + 1] - CumStart;
		Decoder.decodeAdvance(&In, CumStart, Freq, ProbBit);

		AdaptFromMixCDF(CDF, MixCDF->E[Sym], AdaptRate, SymCount);
		Assert(CheckCDF(CDF, SymCount, ProbScale));

		if ((ByteIndex & (SplitSize - 1)) == 0)
		{
			for (u32 i = 0; i < (SymCount + 1); i++) CDF[i] = InitCDF[i];
			Decoder.init(&In);
		}
	}
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
	//TestTableInterleavedRans16(InputFile);
	//TestTableInterleavedRans32(InputFile);
	//TestSIMDDecodeRans16(InputFile);
	//TestNormalizationRans32(InputFile);
	TestPrecomputeAdaptiveOrder1Rans32(InputFile);

	return 0;
}