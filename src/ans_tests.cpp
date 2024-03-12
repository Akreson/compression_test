#include <vector>

#include "ans/rans_common.h"
#include "ans/rans8.cpp"
#include "ans/rans16.cpp"
#include "ans/rans32.cpp"
#include "ans/tans.cpp"
#include "ans/static_basic_stats.cpp"

static constexpr u32 RANS_PROB_BIT = 12;
static constexpr u32 RANS_PROB_SCALE = 1 << RANS_PROB_BIT;

static constexpr u32 TANS_PROB_BITS = 12;
static constexpr u32 TANS_PROB_SCALE = 1 << TANS_PROB_BITS;

void
TestBasicRans8(file_data& InputFile)
{
	PRINT_TEST_FUNC();
	Timer Timer;

	u64 BuffSize = InputFile.Size;
	std::vector<u8> OutBuff(BuffSize);
	std::vector<u8> DecBuff(BuffSize);

	SymbolStats Stats;
	Stats.countSymbol(InputFile.Data, InputFile.Size);
	//Stats.normalize(RANS_PROB_SCALE);
	Stats.optimalNormalize(RANS_PROB_SCALE);

	u8 Cum2Sym[RANS_PROB_SCALE];
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
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Timer.start();

		Rans8Enc Encoder;
		Encoder.init();

		u8* Out = OutBuff.data() + BuffSize;
		for (u64 i = InputFile.Size; i > 0; i--)
		{
			u8 Symbol = InputFile.Data[i - 1];
			Encoder.encode(&Out, Stats.CumFreq[Symbol], Stats.Freq[Symbol], RANS_PROB_BIT);
		}
		Encoder.flush(&Out);
		DecodeBegin = Out;

		Timer.end();
		Accum.update(Timer);
		//PrintSymbolEncPerfStats(EncClocks, EncTime, InputFile.Size);
	}

	PrintAvgPerSymbolPerfStats(Accum, RUNS_COUNT, InputFile.Size);
	Accum.reset();

	u64 CompressedSize = (OutBuff.data() + BuffSize) - DecodeBegin;
	PrintCompressionSize(InputFile.Size, CompressedSize);

	printf(" rANS decode\n");
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Timer.start();

		u8* In = DecodeBegin;

		Rans8Dec Decoder;
		Decoder.init(&In);

		for (u64 i = 0; i < InputFile.Size; i++)
		{
			u32 CumFreq = Decoder.decodeGet(RANS_PROB_BIT);
			u32 Symbol = Cum2Sym[CumFreq];

			Assert(InputFile.Data[i] == Symbol);
			DecBuff[i] = Symbol;
			Decoder.decodeAdvance(&In, Stats.CumFreq[Symbol], Stats.Freq[Symbol], RANS_PROB_BIT);
		}

		Timer.end();
		Accum.update(Timer);
		//PrintSymbolEncPerfStats(DecClocks, DecTime, InputFile.Size);
	}

	PrintAvgPerSymbolPerfStats(Accum, RUNS_COUNT, InputFile.Size);
}

void
TestBasicRans32(file_data& InputFile)
{
	PRINT_TEST_FUNC();
	Timer Timer;

	u64 BuffSize = AlignSizeForward(InputFile.Size);
	std::vector<u8> OutBuff(BuffSize);
	std::vector<u8> DecBuff(BuffSize);

	SymbolStats Stats;
	Stats.countSymbol(InputFile.Data, InputFile.Size);
	Stats.optimalNormalize(RANS_PROB_SCALE);

	u8 Cum2Sym[RANS_PROB_SCALE];
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
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Timer.start();

		Rans32Enc Encoder;
		Encoder.init();

		u32* Out = reinterpret_cast<u32*>(OutBuff.data() + BuffSize);
		for (u64 i = InputFile.Size; i > 0; i--)
		{
			u8 Symbol = InputFile.Data[i - 1];
			Encoder.encode(&Out, Stats.CumFreq[Symbol], Stats.Freq[Symbol], RANS_PROB_BIT);
		}
		Encoder.flush(&Out);
		DecodeBegin = Out;

		Timer.end();
		Accum.update(Timer);
		//PrintSymbolEncPerfStats(EncClocks, EncTime, InputFile.Size);
	}

	PrintAvgPerSymbolPerfStats(Accum, RUNS_COUNT, InputFile.Size);
	Accum.reset();

	u64 CompressedSize = (OutBuff.data() + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
	PrintCompressionSize(InputFile.Size, CompressedSize);

	printf(" rANS decode\n");
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Timer.start();

		u32* In = DecodeBegin;
		Rans32Dec Decoder;
		Decoder.init(&In);

		for (u64 i = 0; i < InputFile.Size; i++)
		{
			u32 CumFreq = Decoder.decodeGet(RANS_PROB_BIT);
			u32 Symbol = Cum2Sym[CumFreq];

			Assert(InputFile.Data[i] == Symbol);
			DecBuff[i] = Symbol;
			Decoder.decodeAdvance(&In, Stats.CumFreq[Symbol], Stats.Freq[Symbol], RANS_PROB_BIT);
		}

		Timer.end();
		Accum.update(Timer);
		//PrintSymbolEncPerfStats(DecClocks, DecTime, InputFile.Size);
	}

	PrintAvgPerSymbolPerfStats(Accum, RUNS_COUNT, InputFile.Size);
}

void
TestFastEncodeRans8(file_data& InputFile)
{
	PRINT_TEST_FUNC();
	Timer Timer;

	u64 BuffSize = InputFile.Size;
	std::vector<u8> OutBuff(BuffSize);
	std::vector<u8> DecBuff(BuffSize);

	SymbolStats Stats;
	Stats.countSymbol(InputFile.Data, InputFile.Size);
	//Stats.fastNormalize(InputFile.Size, RANS_PROB_BIT);
	Stats.normalize(RANS_PROB_SCALE);
	//Stats.optimalNormalize(RANS_PROB_SCALE);

	u8 Cum2Sym[RANS_PROB_SCALE];
	for (u32 SymbolIndex = 0; SymbolIndex < 256; SymbolIndex++)
	{
		for (u32 j = Stats.CumFreq[SymbolIndex]; j < Stats.CumFreq[SymbolIndex + 1]; j++)
		{
			Cum2Sym[j] = SymbolIndex;
		}
	}

	rans_enc_sym32 EncSymArr[256];
	rans_dec_sym32 DecSymArr[256];

	for (int i = 0; i < 256; i++)
	{
		RansEncSymInit(&EncSymArr[i], Stats.CumFreq[i], Stats.Freq[i], RANS_PROB_BIT, Rans8L, 8);
		RansDecSymInit(&DecSymArr[i], Stats.CumFreq[i], Stats.Freq[i]);
	}

	u8* DecodeBegin = nullptr;

	AccumTime Accum;
	printf(" rANS encode\n");
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Timer.start();

		Rans8Enc Encoder;
		Encoder.init();

		u8* Out = OutBuff.data() + BuffSize;
		for (u64 i = InputFile.Size; i > 0; i--)
		{
			u8 Symbol = InputFile.Data[i - 1];
			Encoder.encode(&Out, &EncSymArr[Symbol]);
		}
		Encoder.flush(&Out);
		DecodeBegin = Out;

		Timer.end();
		Accum.update(Timer);
		//PrintSymbolEncPerfStats(EncClocks, EncTime, InputFile.Size);
	}

	PrintAvgPerSymbolPerfStats(Accum, RUNS_COUNT, InputFile.Size);
	Accum.reset();

	u64 CompressedSize = (OutBuff.data() + BuffSize) - DecodeBegin;
	PrintCompressionSize(InputFile.Size, CompressedSize);

	printf(" rANS decode\n");
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Timer.start();

		u8* In = DecodeBegin;
		Rans8Dec Decoder;
		Decoder.init(&In);

		for (u64 i = 0; i < InputFile.Size; i++)
		{
			u32 CumFreq = Decoder.decodeGet(RANS_PROB_BIT);
			u32 Symbol = Cum2Sym[CumFreq];

			Assert(InputFile.Data[i] == Symbol);
			DecBuff[i] = Symbol;

			Decoder.decodeAdvance(&In, &DecSymArr[Symbol], RANS_PROB_BIT);
		}

		Timer.end();
		Accum.update(Timer);
		//PrintSymbolEncPerfStats(DecClocks, DecTime, InputFile.Size);
	}

	PrintAvgPerSymbolPerfStats(Accum, RUNS_COUNT, InputFile.Size);
}

void
TestFastEncodeRans32(file_data& InputFile)
{
	PRINT_TEST_FUNC();
	Timer Timer;

	u64 BuffSize = InputFile.Size;
	std::vector<u8> OutBuff(BuffSize);
	std::vector<u8> DecBuff(BuffSize);

	SymbolStats Stats;
	Stats.countSymbol(InputFile.Data, InputFile.Size);
	Stats.optimalNormalize(RANS_PROB_SCALE);

	u8 Cum2Sym[RANS_PROB_SCALE];
	for (u32 SymbolIndex = 0; SymbolIndex < 256; SymbolIndex++)
	{
		for (u32 j = Stats.CumFreq[SymbolIndex]; j < Stats.CumFreq[SymbolIndex + 1]; j++)
		{
			Cum2Sym[j] = SymbolIndex;
		}
	}

	rans_enc_sym64 EncSymArr[256];
	rans_dec_sym64 DecSymArr[256];

	for (int i = 0; i < 256; i++)
	{
		RansEncSymInit(&EncSymArr[i], Stats.CumFreq[i], Stats.Freq[i], RANS_PROB_BIT);
		RansDecSymInit(&DecSymArr[i], Stats.CumFreq[i], Stats.Freq[i]);
	}

	u32* DecodeBegin = nullptr;

	AccumTime Accum;
	printf(" rANS encode\n");
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Timer.start();

		Rans32Enc Encoder;
		Encoder.init();

		u32* Out = reinterpret_cast<u32*>(OutBuff.data() + BuffSize);
		for (u64 i = InputFile.Size; i > 0; i--)
		{
			u8 Symbol = InputFile.Data[i - 1];
			Encoder.encode(&Out, &EncSymArr[Symbol], RANS_PROB_BIT);
		}
		Encoder.flush(&Out);
		DecodeBegin = Out;

		Timer.end();
		Accum.update(Timer);
		//PrintSymbolEncPerfStats(EncClocks, EncTime, InputFile.Size);
	}

	PrintAvgPerSymbolPerfStats(Accum, RUNS_COUNT, InputFile.Size);
	Accum.reset();

	u64 CompressedSize = (OutBuff.data() + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
	PrintCompressionSize(InputFile.Size, CompressedSize);

	printf(" rANS decode\n");
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Timer.start();

		u32* In = DecodeBegin;

		Rans32Dec Decoder;
		Decoder.init(&In);

		for (u64 i = 0; i < InputFile.Size; i++)
		{
			u32 CumFreq = Decoder.decodeGet(RANS_PROB_BIT);
			u32 Symbol = Cum2Sym[CumFreq];

			Assert(InputFile.Data[i] == Symbol);
			DecBuff[i] = Symbol;

			Decoder.decodeAdvance(&In, &DecSymArr[Symbol], RANS_PROB_BIT);
		}

		Timer.end();
		Accum.update(Timer);
		//PrintSymbolEncPerfStats(DecClocks, DecTime, InputFile.Size);
	}

	PrintAvgPerSymbolPerfStats(Accum, RUNS_COUNT, InputFile.Size);
}

void
TestTableDecodeRans16(file_data& InputFile)
{
	PRINT_TEST_FUNC();
	Timer Timer;

	u64 BuffSize = AlignSizeForward(InputFile.Size);
	std::vector<u8> OutBuff(BuffSize);
	std::vector<u8> DecBuff(BuffSize);

	SymbolStats Stats;
	Stats.countSymbol(InputFile.Data, InputFile.Size);
	Stats.optimalNormalize(RANS_PROB_SCALE);

	rans_sym_table<RANS_PROB_SCALE> Tab;

	for (u32 i = 0; i < 256; i++)
	{
		RansTableInitSym(Tab, i, Stats.CumFreq[i], Stats.Freq[i]);
	}

	u16* DecodeBegin = nullptr;

	AccumTime Accum;
	printf(" rANS encode\n");
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Timer.start();

		Rans16Enc Encoder;
		Encoder.init();

		u16* Out = reinterpret_cast<u16*>(OutBuff.data() + BuffSize);
		for (u64 i = InputFile.Size; i > 0; i--)
		{
			u8 Symbol = InputFile.Data[i - 1];
			Encoder.encode(&Out, Stats.CumFreq[Symbol], Stats.Freq[Symbol], RANS_PROB_BIT);
		}
		Encoder.flush(&Out);
		DecodeBegin = Out;

		Timer.end();
		Accum.update(Timer);
		//PrintSymbolEncPerfStats(EncClocks, EncTime, InputFile.Size);
	}

	PrintAvgPerSymbolPerfStats(Accum, RUNS_COUNT, InputFile.Size);
	Accum.reset();

	u64 CompressedSize = (OutBuff.data() + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
	PrintCompressionSize(InputFile.Size, CompressedSize);

	printf(" rANS decode\n");
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Timer.start();

		u16* In = DecodeBegin;

		Rans16Dec Decoder;
		Decoder.init(&In);

		for (u64 i = 0; i < InputFile.Size; i++)
		{
			u8 Symbol = Decoder.decodeSym(Tab, RANS_PROB_SCALE, RANS_PROB_BIT);

			Assert(InputFile.Data[i] == Symbol);

			DecBuff[i] = Symbol;
			Decoder.decodeRenorm(&In);
		}

		Timer.end();
		Accum.update(Timer);
		//PrintSymbolEncPerfStats(DecClocks, DecTime, InputFile.Size);
	}

	PrintAvgPerSymbolPerfStats(Accum, RUNS_COUNT, InputFile.Size);
}

void
TestTableInterleavedRans16(file_data& InputFile)
{
	PRINT_TEST_FUNC();
	Timer Timer;

	u64 BuffSize = AlignSizeForward(InputFile.Size);
	std::vector<u8> OutBuff(BuffSize);
	std::vector<u8> DecBuff(BuffSize);

	SymbolStats Stats;
	Stats.countSymbol(InputFile.Data, InputFile.Size);
	Stats.optimalNormalize(RANS_PROB_SCALE);

	rans_sym_table<RANS_PROB_SCALE> Tab;

	for (u32 i = 0; i < 256; i++)
	{
		RansTableInitSym(Tab, i, Stats.CumFreq[i], Stats.Freq[i]);
	}

	u16* DecodeBegin = nullptr;

	AccumTime Accum;
	printf(" rANS encode\n");
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Timer.start();

		u16* Out = reinterpret_cast<u16*>(OutBuff.data() + BuffSize);

		Rans16Enc Enc0, Enc1;
		Enc0.init();
		Enc1.init();

		if (InputFile.Size & 1)
		{
			u8 Symbol = InputFile.Data[InputFile.Size - 1];
			Enc0.encode(&Out, Stats.CumFreq[Symbol], Stats.Freq[Symbol], RANS_PROB_BIT);
		}

		for (u64 i = (InputFile.Size & ~1); i > 0; i -= 2)
		{
			u8 Symbol1 = InputFile.Data[i - 1];
			u8 Symbol0 = InputFile.Data[i - 2];
			Enc1.encode(&Out, Stats.CumFreq[Symbol1], Stats.Freq[Symbol1], RANS_PROB_BIT);
			Enc0.encode(&Out, Stats.CumFreq[Symbol0], Stats.Freq[Symbol0], RANS_PROB_BIT);
		}
		Enc1.flush(&Out);
		Enc0.flush(&Out);
		DecodeBegin = Out;

		Timer.end();
		Accum.update(Timer);
		//PrintSymbolEncPerfStats(EncClocks, EncTime, InputFile.Size);
	}

	PrintAvgPerSymbolPerfStats(Accum, RUNS_COUNT, InputFile.Size);
	Accum.reset();

	u64 CompressedSize = (OutBuff.data() + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
	PrintCompressionSize(InputFile.Size, CompressedSize);

	printf(" rANS decode\n");
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Timer.start();

		u16* In = DecodeBegin;

		Rans16Dec Dec0, Dec1;
		Dec0.init(&In);
		Dec1.init(&In);

		for (u64 i = 0; i < (InputFile.Size & ~1); i += 2)
		{
			u8 Symbol0 = Dec0.decodeSym(Tab, RANS_PROB_SCALE, RANS_PROB_BIT);
			u8 Symbol1 = Dec1.decodeSym(Tab, RANS_PROB_SCALE, RANS_PROB_BIT);

			Assert(InputFile.Data[i] == Symbol0);
			Assert(InputFile.Data[i + 1] == Symbol1);

			DecBuff[i] = Symbol0;
			DecBuff[i + 1] = Symbol1;

			Dec0.decodeRenorm(&In);
			Dec1.decodeRenorm(&In);
		}

		if (InputFile.Size & 1)
		{
			u8 Symbol = Dec0.decodeSym(Tab, RANS_PROB_SCALE, RANS_PROB_BIT);
			InputFile.Data[InputFile.Size - 1] = Symbol;
		}

		Timer.end();
		Accum.update(Timer);
		//PrintSymbolEncPerfStats(DecClocks, DecTime, InputFile.Size);
	}

	PrintAvgPerSymbolPerfStats(Accum, RUNS_COUNT, InputFile.Size);
}

void
TestTableInterleavedRans32(file_data& InputFile)
{
	PRINT_TEST_FUNC();

	Timer Timer;

	u64 BuffSize = AlignSizeForward(InputFile.Size);
	std::vector<u8> OutBuff(BuffSize);
	std::vector<u8> DecBuff(BuffSize);

	SymbolStats Stats;
	Stats.countSymbol(InputFile.Data, InputFile.Size);
	Stats.optimalNormalize(RANS_PROB_SCALE);

	rans_sym_table<RANS_PROB_SCALE> Tab;

	for (u32 i = 0; i < 256; i++)
	{
		RansTableInitSym(Tab, i, Stats.CumFreq[i], Stats.Freq[i]);
	}

	u32* DecodeBegin = nullptr;

	AccumTime Accum;
	printf(" rANS encode\n");
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Timer.start();

		u32* Out = reinterpret_cast<u32*>(OutBuff.data() + BuffSize);

		Rans32Enc Enc0, Enc1;
		Enc0.init();
		Enc1.init();

		if (InputFile.Size & 1)
		{
			u8 Symbol = InputFile.Data[InputFile.Size - 1];
			Enc0.encode(&Out, Stats.CumFreq[Symbol], Stats.Freq[Symbol], RANS_PROB_BIT);
		}

		for (u64 i = (InputFile.Size & ~1); i > 0; i -= 2)
		{
			u8 Symbol1 = InputFile.Data[i - 1];
			u8 Symbol0 = InputFile.Data[i - 2];
			Enc1.encode(&Out, Stats.CumFreq[Symbol1], Stats.Freq[Symbol1], RANS_PROB_BIT);
			Enc0.encode(&Out, Stats.CumFreq[Symbol0], Stats.Freq[Symbol0], RANS_PROB_BIT);
		}
		Enc1.flush(&Out);
		Enc0.flush(&Out);
		DecodeBegin = Out;

		Timer.end();
		Accum.update(Timer);
		//PrintSymbolEncPerfStats(EncClocks, EncTime, InputFile.Size);
	}

	PrintAvgPerSymbolPerfStats(Accum, RUNS_COUNT, InputFile.Size);
	Accum.reset();

	u64 CompressedSize = (OutBuff.data() + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
	PrintCompressionSize(InputFile.Size, CompressedSize);

	printf(" rANS decode\n");
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Timer.start();

		u32* In = DecodeBegin;

		Rans32Dec Dec0, Dec1;
		Dec0.init(&In);
		Dec1.init(&In);

		for (u64 i = 0; i < (InputFile.Size & ~1); i += 2)
		{
			u8 Symbol0 = Dec0.decodeSym(Tab, RANS_PROB_SCALE, RANS_PROB_BIT);
			u8 Symbol1 = Dec1.decodeSym(Tab, RANS_PROB_SCALE, RANS_PROB_BIT);

			Assert(InputFile.Data[i] == Symbol0);
			Assert(InputFile.Data[i + 1] == Symbol1);

			DecBuff[i] = Symbol0;
			DecBuff[i + 1] = Symbol1;

			Dec0.decodeRenorm(&In);
			Dec1.decodeRenorm(&In);
		}

		if (InputFile.Size & 1)
		{
			u8 Symbol = Dec0.decodeSym(Tab, RANS_PROB_SCALE, RANS_PROB_BIT);
			InputFile.Data[InputFile.Size - 1] = Symbol;
		}

		Timer.end();
		Accum.update(Timer);
		//PrintSymbolEncPerfStats(DecClocks, DecTime, InputFile.Size);
	}

	PrintAvgPerSymbolPerfStats(Accum, RUNS_COUNT, InputFile.Size);
}

#define Check4SymDecBuff(Buff, i) ((u32)(Buff[(i)] | (Buff[(i) + 1] << 8) | (Buff[(i) + 2] << 16) | (Buff[(i) + 3] << 24)))

void
TestSIMDDecodeRans16(file_data& InputFile)
{
	PRINT_TEST_FUNC();

	Timer Timer;

	// align buffer size
	u64 BuffSize = AlignSizeForward(InputFile.Size, 16);
	std::vector<u8> OutBuff(BuffSize);
	std::vector<u8> DecBuff(BuffSize);

	SymbolStats Stats;
	Stats.countSymbol(InputFile.Data, InputFile.Size);
	Stats.optimalNormalize(RANS_PROB_SCALE);

	rans_sym_table<RANS_PROB_SCALE> Tab;

	for (u32 i = 0; i < 256; i++)
	{
		RansTableInitSym(Tab, i, Stats.CumFreq[i], Stats.Freq[i]);
	}

	u16* DecodeBegin = nullptr;

	AccumTime Accum;
	printf(" rANS encode\n");
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Timer.start();

		Rans16Enc Enc[8];
		for (u32 i = 0; i < 8; i++) Enc[i].init();

		u16* Out = reinterpret_cast<u16*>(OutBuff.data() + BuffSize);
		for (u64 i = InputFile.Size; i > 0; i--)
		{
			u8 Symbol = InputFile.Data[i - 1];
			u32 Index = (i - 1) & 7;
			Enc[Index].encode(&Out, Stats.CumFreq[Symbol], Stats.Freq[Symbol], RANS_PROB_BIT);
		}

		for (u32 i = 8; i > 0; i--) Enc[i - 1].flush(&Out);
		DecodeBegin = Out;

		Timer.end();
		Accum.update(Timer);
		//PrintSymbolEncPerfStats(EncClocks, EncTime, InputFile.Size);
	}

	PrintAvgPerSymbolPerfStats(Accum, RUNS_COUNT, InputFile.Size);
	Accum.reset();

	u64 CompressedSize = (OutBuff.data() + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
	PrintCompressionSize(InputFile.Size, CompressedSize);

	printf(" rANS decode\n");
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Timer.start();

		u16* In = DecodeBegin;

		Rans16DecSIMD Dec0, Dec1;
		Dec0.init(&In);
		Dec1.init(&In);

		u8* DecBuffPtr = DecBuff.data();
		for (u64 i = 0; i < (InputFile.Size & ~7); i += 8)
		{
			u32 Symbol03 = Dec0.decodeSym(Tab, RANS_PROB_SCALE, RANS_PROB_BIT);
			u32 Symbol47 = Dec1.decodeSym(Tab, RANS_PROB_SCALE, RANS_PROB_BIT);

			Assert(Symbol03 == Check4SymDecBuff(InputFile.Data, i));
			Assert(Symbol47 == Check4SymDecBuff(InputFile.Data, i + 4));
			*reinterpret_cast<u32*>(DecBuffPtr + i) = Symbol03;
			*reinterpret_cast<u32*>(DecBuffPtr + i + 4) = Symbol47;

			Dec0.decodeRenorm(&In);
			Dec1.decodeRenorm(&In);
		}

		for (u64 i = InputFile.Size & ~7; i < InputFile.Size; i++)
		{
			Rans16DecSIMD* DecSIMD = (i & 4) != 0 ? &Dec1 : &Dec0;
			Rans16Dec* Dec = DecSIMD->State.lane + (i & 3);
			u8 Symbol = Dec->decodeSym(Tab, RANS_PROB_SCALE, RANS_PROB_BIT);
			DecBuff[i] = Symbol;
		}

		Timer.end();
		Accum.update(Timer);
		//PrintSymbolEncPerfStats(DecClocks, DecTime, InputFile.Size);
	}

	PrintAvgPerSymbolPerfStats(Accum, RUNS_COUNT, InputFile.Size);
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
	std::vector<u8> Cum2Sym(ProbScale);

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
	PRINT_TEST_FUNC();

	u64 BuffSize = AlignSizeForward(InputFile.Size);
	std::vector<u8> OutBuff(BuffSize);
	std::vector<u8> DecBuff(BuffSize);

	u32* Out = reinterpret_cast<u32*>(OutBuff.data() + BuffSize);

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

	u32 TableLogSize[] = { 14, 13, 12, 11, 10 };
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

		u64 CompressedSizeNorm = (OutBuff.data() + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
		DecodeRans32(InputFile.Data, InputFile.Size, DecBuff.data(), DecodeBegin, NormFreq, CumFreq, ProbBit); // decode ok?

		printf("(norm) ");
		PrintCompressionSize(InputFile.Size, CompressedSizeNorm);

		//fast normalize
		ZeroSize(NormFreq, sizeof(NormFreq));
		ZeroSize(CumFreq, sizeof(CumFreq));

		FastNormalize(RawFreq, NormFreq, InputFile.Size, 256, ProbBit);
		CalcCumFreq(NormFreq, CumFreq, 256);

		DecodeBegin = EncodeRans32(Out, InputFile.Data, InputFile.Size, NormFreq, CumFreq, ProbBit);

		u64 CompressedSizeFast = (OutBuff.data() + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
		DecodeRans32(InputFile.Data, InputFile.Size, DecBuff.data(), DecodeBegin, NormFreq, CumFreq, ProbBit); // decode ok?

		printf("(fast) ");
		PrintCompressionSize(InputFile.Size, CompressedSizeFast);

		// optimal normalize
		ZeroSize(NormFreq, sizeof(NormFreq));
		ZeroSize(CumFreq, sizeof(CumFreq));

		OptimalNormalize(RawFreq, NormFreq, TotalSum, 256, ProbScale);
		CalcCumFreq(NormFreq, CumFreq, 256);

		DecodeBegin = EncodeRans32(Out, InputFile.Data, InputFile.Size, NormFreq, CumFreq, ProbBit);

		u64 CompressedSizeOpt = (OutBuff.data() + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
		DecodeRans32(InputFile.Data, InputFile.Size, DecBuff.data(), DecodeBegin, NormFreq, CumFreq, ProbBit); // decode ok?

		printf("(optm) ");
		PrintCompressionSize(InputFile.Size, CompressedSizeOpt);

		printf("\n");
	}
}

using freq_val_o1 = array2d<u32, 256, 256>;
using cdf_val_o1 = array2d<u16, 256, 257>;

void
PrecomputCDFFromEntireData(u8* Data, u64 Size, cdf_val_o1* MixCDF, u32 TargetTotalLog, u32 SymCount)
{
	std::vector<u16> TempNormFreq(SymCount, 0);
	std::vector<u32> Total(SymCount, SymCount);
	freq_val_o1* Order1Freq = new freq_val_o1;

	for (u32 i = 0; i < freq_val_o1::d0; i++)
	{
		u32* Arr = &Order1Freq->E[i][0];
		MemSet<u32>(Arr, SymCount, 1);
	}

	u32 Inc = 16;
	for (u32 i = 1; i < Size; i++)
	{
		Order1Freq->E[Data[i - 1]][Data[i]] += Inc;
		Total[Data[i - 1]] += Inc;
	}

	for (u32 i = 0; i < SymCount; i++)
	{
		FastNormalize(Order1Freq->E[i], TempNormFreq.data(), Total[i], SymCount, TargetTotalLog);
		CalcCumFreq(TempNormFreq.data(), MixCDF->E[i], SymCount);
		ZeroSize(TempNormFreq.data(), sizeof(u16) * SymCount);
	}

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
InitEqDistCDF(u16* CDF, u32 SymCount, u32 ProbScale)
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
	PRINT_TEST_FUNC();
	
	const u32 AdaptRate = 1;
	const u32 SymCount = 256;
	const u32 ProbBit = 14;
	const u32 ProbScale = 1 << ProbBit;
	const u32 CDFSize = sizeof(u16) * (SymCount + 1);

	Assert(ProbBit <= 15); // for ease u16 -> s16 interpretation

	// init data
	std::vector<u16> CDF(SymCount + 1);
	std::vector<u16> InitCDF(SymCount + 1);
	cdf_val_o1* MixCDF = new cdf_val_o1;

	InitEqDistCDF(InitCDF.data(), SymCount, ProbScale);
	PrecomputCDFFromEntireData(InputFile.Data, InputFile.Size, MixCDF, ProbBit, SymCount);

	// collect adapted rans sym
	const u32 SplitSize = 1 << 16;
	Assert(IsPowerOf2(SplitSize));

	u64 Start = 0;
	u64 End = SplitSize < InputFile.Size ? SplitSize : InputFile.Size;

	std::vector<rans_sym> BuffRans;
	for (;;)
	{
		CDF = InitCDF;

		for (u64 i = Start; i < End; i++)
		{
			rans_sym R;

			u8 Sym = InputFile.Data[i];
			R.Start = CDF[Sym];
			R.Freq = CDF[Sym + 1] - R.Start;
			Assert(R.Freq);

			BuffRans.push_back(R);

			AdaptFromMixCDF(CDF.data(), MixCDF->E[Sym], AdaptRate, SymCount);
			Assert(CheckCDF(CDF.data(), SymCount, ProbScale));
		}

		if (End == InputFile.Size) break;

		Start += SplitSize;
		End += SplitSize;
		End = End < InputFile.Size ? End : InputFile.Size;
	}

	//encoding
	u64 BuffSize = AlignSizeForward(InputFile.Size);
	std::vector<u8> OutBuff(BuffSize);
	std::vector<u8> DecBuff(BuffSize);
	u32* Out = reinterpret_cast<u32*>(OutBuff.data() + BuffSize);

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

	u64 CompressedSize = (OutBuff.data() + BuffSize) - reinterpret_cast<u8*>(DecodeBegin);
	PrintCompressionSize(InputFile.Size, CompressedSize);

	//decoding
	CDF = InitCDF;
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

		AdaptFromMixCDF(CDF.data(), MixCDF->E[Sym], AdaptRate, SymCount);
		Assert(CheckCDF(CDF.data(), SymCount, ProbScale));

		if ((ByteIndex & (SplitSize - 1)) == 0)
		{
			for (u32 i = 0; i < (SymCount + 1); i++) CDF[i] = InitCDF[i];
			Decoder.init(&In);
		}
	}

	delete MixCDF;
}

void
TansSortSymBitReverse(u8* SortedSym, u32 SortCount, const u16* NormFreq, u32 AlphSymCount = 256)
{
	u32 SymbolCounter = 0;
	for (u32 Sym = 0; Sym < AlphSymCount; Sym++)
	{
		u16 Freq = NormFreq[Sym];
		for (u32 Count = 0; Count < Freq; Count++)
		{
			u32 ReverseCounter = BitReverseSlow(SymbolCounter++, TANS_PROB_BITS);
			SortedSym[ReverseCounter] = Sym;
		}
	}

	Assert(SymbolCounter == SortCount);
}

void
TansRadix8SortToBuffer(u8* SortedSym, u32 SortCount, const u16* NormFreq, u32 AlphSymCount = 256)
{
	const u32 RadixNumber = 255 << 8;
	//const u32 RadixNumber = (256 << 8) - 1;
	u32 RadixHisto[256] = {};
	u32 RadixSum = TansRadixSort8(RadixHisto, RadixNumber, NormFreq, AlphSymCount);

	for (u32 SymIndex = 0; SymIndex < AlphSymCount; SymIndex++)
	{
		u16 Freq = NormFreq[SymIndex];
		if (!Freq) continue;

		u32 Invp = RadixNumber / Freq;
		u32 Rank = Invp;

		for (u32 i = 0; i < Freq; ++i)
		{
			u32 Index = Rank >> 8;
			Rank += Invp;

			SortedSym[RadixHisto[Index]++] = SymIndex;
		}
	}
}

template <b32 IsRadixSort = true> void
TestBasicTans(file_data& InputFile)
{
	PRINT_TEST_FUNC();

	std::vector<u8> OutBuff(InputFile.Size);
	std::vector<u8> DecBuff(InputFile.Size);

	u32 Freq[256] = {};
	u16 NormFreq[256] = {};

	CountByte(Freq, InputFile.Data, InputFile.Size);
	OptimalNormalize(Freq, NormFreq, InputFile.Size, 256, TANS_PROB_SCALE);

	std::vector<TansEncTable::entry> EncEntriesMem(256);
	std::vector<TansDecTable::entry> DecEntriesMem(TANS_PROB_SCALE);

	std::vector<u16> TableMem(TANS_PROB_SCALE);
	std::vector<u8> SortedSym(TANS_PROB_SCALE);
	
	TansEncTable EncTable;
	TansDecTable DecTable;

	TansState State;

	Timer Timer;
	AccumTime SymbolSortAccum;
	AccumTime EncodeInitAccum, EncAccum;
	AccumTime DecodeInitAccum, DecAccum;

	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Timer.start();
		if constexpr (IsRadixSort)
		{
			TansRadix8SortToBuffer(SortedSym.data(), TANS_PROB_SCALE, NormFreq, 256);
		}
		else
		{
			TansSortSymBitReverse(SortedSym.data(), TANS_PROB_SCALE, NormFreq, 256);
		}
		Timer.end();
		SymbolSortAccum.update(Timer);
	}

	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Timer.start();

		EncTable.init(EncEntriesMem.data(), TANS_PROB_BITS, TableMem.data(), SortedSym.data(), NormFreq);
		State.State = EncTable.L;

		Timer.end();
		EncodeInitAccum.update(Timer);
	}

	BitWriter Writer;
	u64 TotalEncSize = 0;
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Writer.init(OutBuff.data(), InputFile.Size);

		Timer.start();
		for (u64 i = InputFile.Size; i > 0; --i)
		{
			State.encode(Writer, EncTable, InputFile.Data[i - 1]);
		}

		Writer.writeMaskMSB(State.State, EncTable.StateBits);
		TotalEncSize = Writer.finishReverse();

		Timer.end();
		EncAccum.update(Timer);
	}

	BitReaderReverseMSB Reader;
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Timer.start();

		Reader.init(OutBuff.data(), TotalEncSize);
		DecTable.init(DecEntriesMem.data(), TANS_PROB_BITS, SortedSym.data(), NormFreq);

		Reader.refillTo(DecTable.StateBits);
		State.State = Reader.getBits(DecTable.StateBits);

		Timer.end();
		DecodeInitAccum.update(Timer);

		u8* DecOut = DecBuff.data();
		Timer.start();

		Reader.refillTo(DecTable.StateBits);
		for (u64 i = 0; i < InputFile.Size; i++)
		{
			u8 Sym = State.decode(Reader, DecTable);
			Assert(Sym == InputFile.Data[i]);
			*DecOut++ = Sym;
			Reader.refillTo(DecTable.StateBits);
		}

		Timer.end();
		DecAccum.update(Timer);
	}

	SymbolSortAccum.avg(RUNS_COUNT);
	EncodeInitAccum.avg(RUNS_COUNT);

	printf(" tANS symbol sort - %lu clocks, %0.6f ms \n", SymbolSortAccum.Clock, SymbolSortAccum.Time * 1000.0);
	printf(" tANS table build - %lu clocks, %0.6f ms \n\n", EncodeInitAccum.Clock, EncodeInitAccum.Time * 1000.0);

	printf(" tANS encode\n");
	PrintAvgPerSymbolPerfStats(EncAccum, RUNS_COUNT, InputFile.Size);
	printf(" tANS decode\n");
	PrintAvgPerSymbolPerfStats(DecAccum, RUNS_COUNT, InputFile.Size);
	PrintCompressionSize(InputFile.Size, TotalEncSize);
}

template<b32 IsRadixInit = true> void
TestInterleavedTans(file_data& InputFile)
{
	PRINT_TEST_FUNC();

	std::vector<u8> OutBuff(InputFile.Size);
	std::vector<u8> DecBuff(InputFile.Size);

	u32 Freq[256] = {};
	u16 NormFreq[256] = {};

	CountByte(Freq, InputFile.Data, InputFile.Size);
	OptimalNormalize(Freq, NormFreq, InputFile.Size, 256, TANS_PROB_SCALE);

	std::vector<TansEncTable::entry> EncEntriesMem(256);
	std::vector<TansDecTable::entry> DecEntriesMem(TANS_PROB_SCALE);
	std::vector<u16> TableMem(TANS_PROB_SCALE);
	std::vector<u8> SortedSym(TANS_PROB_SCALE);

	BitWriter Writer;

	TansEncTable EncTable;
	TansDecTable DecTable;

	TansState State1;
	TansState State2;

	Timer Timer;
	AccumTime SymbolSortAccum;
	AccumTime EncodeInitAccum, EncAccum;
	AccumTime DecodeInitAccum, DecAccum;

	u64 TotalEncSize = 0;
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		if constexpr (IsRadixInit)
		{
			Timer.start();

			EncTable.initRadix(EncEntriesMem.data(), TANS_PROB_BITS, TableMem.data(), NormFreq);
			State1.State = EncTable.L;
			State2.State = EncTable.L;

			Timer.end();
		}
		else 
		{
			Timer.start();
			TansSortSymBitReverse(SortedSym.data(), TANS_PROB_SCALE, NormFreq, 256);
			Timer.end();

			SymbolSortAccum.update(Timer);

			Timer.start();

			EncTable.init(EncEntriesMem.data(), TANS_PROB_BITS, TableMem.data(), SortedSym.data(), NormFreq);
			State1.State = EncTable.L;
			State2.State = EncTable.L;

			Timer.end();
		}
		EncodeInitAccum.update(Timer);
	}

	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Writer.init(OutBuff.data(), InputFile.Size);

		Timer.start();
		u64 i = InputFile.Size;
		for (; i > (InputFile.Size - (InputFile.Size % 4)); i--)
		{
			u8 Symbol = InputFile.Data[i - 1];
			State1.encode(Writer, EncTable, Symbol);
		}

		for (; i > 0; i -= 4)
		{
			State1.encode(Writer, EncTable, InputFile.Data[i - 1]);
			State2.encode(Writer, EncTable, InputFile.Data[i - 2]);
			State1.encode(Writer, EncTable, InputFile.Data[i - 3]);
			State2.encode(Writer, EncTable, InputFile.Data[i - 4]);
		}

		Writer.writeMaskMSB(State1.State, EncTable.StateBits);
		Writer.writeMaskMSB(State2.State, EncTable.StateBits);
		TotalEncSize = Writer.finishReverse();

		Timer.end();
		EncAccum.update(Timer);
	}

	BitReaderReverseMSB Reader;
	for (u32 Run = 0; Run < RUNS_COUNT; Run++)
	{
		Reader.init(OutBuff.data(), TotalEncSize);

		Timer.start();
		if constexpr (IsRadixInit)
		{
			DecTable.initRadix(DecEntriesMem.data(), TANS_PROB_BITS, NormFreq);
		}
		else
		{
			DecTable.init(DecEntriesMem.data(), TANS_PROB_BITS, SortedSym, NormFreq);
		}

		Reader.refillTo(DecTable.StateBits);
		State2.State = Reader.getBits(DecTable.StateBits);
		State1.State = Reader.getBits(DecTable.StateBits);
		Timer.end();

		DecodeInitAccum.update(Timer);

		u8* DecOut = DecBuff.data();
		Timer.start();
		Reader.refillTo(DecTable.StateBits * 4);

		for (u64 i = 0; i < (InputFile.Size / 4); i++)
		{
			*DecOut++ = State2.decode(Reader, DecTable);
			*DecOut++ = State1.decode(Reader, DecTable);
			*DecOut++ = State2.decode(Reader, DecTable);
			*DecOut++ = State1.decode(Reader, DecTable);
			Reader.refillTo(DecTable.StateBits * 4);
		}

		for (u64 i = 0; i < (InputFile.Size % 4); i++)
		{
			*DecOut++ = State1.decode(Reader, DecTable);
			Reader.refillTo(DecTable.StateBits);
		}
		Timer.end();
		DecAccum.update(Timer);

		for (u64 i = 0; i < InputFile.Size; i++)
		{
			Assert(DecBuff[i] == InputFile.Data[i]);
		}
	}

	SymbolSortAccum.avg(RUNS_COUNT);
	EncodeInitAccum.avg(RUNS_COUNT);

	printf(" tANS symbol sort - %lu clocks, %0.6f ms \n", SymbolSortAccum.Clock, SymbolSortAccum.Time * 1000.0);
	printf(" tANS table build - %lu clocks, %0.6f ms \n\n", EncodeInitAccum.Clock, EncodeInitAccum.Time * 1000.0);

	printf(" tANS encode\n");
	PrintAvgPerSymbolPerfStats(EncAccum, RUNS_COUNT, InputFile.Size);
	printf(" tANS decode\n");
	PrintAvgPerSymbolPerfStats(DecAccum, RUNS_COUNT, InputFile.Size);
	PrintCompressionSize(InputFile.Size, TotalEncSize);
}
