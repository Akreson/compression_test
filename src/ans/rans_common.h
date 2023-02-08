#if !defined(RANS_COMMON_H)
#define RANS_COMMON_H

static constexpr u32 ProbBit = 14;
static constexpr u32 ProbScale = 1 << ProbBit;

struct rans_enc_sym32
{
    u32 Max;
    u32 RcpFreq;
    u32 Bias;
    u16 CmplFreq;
    u16 RcpShift;
};

struct rans_dec_sym32
{
    u16 CumStart;
    u16 Freq;
};

struct rans_enc_sym64
{
	u64 RcpFreq;
	u32 Freq;
	u32 Bias;
	u32 CmplFreq;
	u32 RcpShift;
};

struct rans_dec_sym64
{
	u32 CumStart;
	u32 Freq;
};

static inline void
RansEncSymInit(rans_enc_sym32* Sym, u32 CumStart, u32 Freq, u32 ScaleBit, u32 L, u32 NormStep)
{
	Assert(IsPowerOf2(NormStep));
	Assert(ScaleBit <= 16);
	Assert(CumStart <= (1 << 16));
	Assert(Freq <= (1 << 16) - CumStart);

	Sym->Max = ((L >> ScaleBit) << NormStep) * Freq;
	Sym->CmplFreq = (u16)((1 << ScaleBit) - Freq);
	if (Freq < 2)
	{
		Sym->RcpFreq = ~0u;
		Sym->RcpShift = 0;
		Sym->Bias = CumStart + (1 << ScaleBit) - 1;
	}
	else
	{
		// NOTE: edge case, with _while_ this will be handled when (Freq == 1 << Shift) 
		u32 Adder = static_cast<u32>(!IsPowerOf2(Freq));
		bit_scan_result Result = FindMostSignificantSetBit(Freq);
		Assert(Result.Succes);

		u32 Shift = Result.Index + Adder;
		Sym->RcpFreq = (u32)(((1ull << (Shift + 31)) + Freq - 1) / Freq);
		Sym->RcpShift = Shift - 1;
		Sym->Bias = CumStart;
	}
}

static inline void
RansDecSymInit(rans_dec_sym32* DecSym, u32 CumStart, u32 Freq)
{
	Assert(CumStart <= (1 << 16));
	Assert(Freq <= (1 << 16) - CumStart);

	DecSym->CumStart = (u16)CumStart;
	DecSym->Freq = (u16)Freq;
}

static inline void
RansEncSymInit(rans_enc_sym64* Sym, u32 CumStart, u32 Freq, u32 ScaleBit)
{
	Assert(ScaleBit <= 31);
	Assert(CumStart <= (1 << ScaleBit));
	Assert(Freq <= (1 << ScaleBit) - CumStart);

	Sym->Freq = Freq;
	Sym->CmplFreq = ((1 << ScaleBit) - Freq);
	if (Freq < 2)
	{
		Sym->RcpFreq = ~0ull;
		Sym->RcpShift = 0;
		Sym->Bias = CumStart + (1 << ScaleBit) - 1;
	}
	else
	{
		// NOTE: edge case, with _while_ this will be handled when (Freq == 1 << Shift) 
		u32 Adder = static_cast<u32>(!IsPowerOf2(Freq));
		bit_scan_result Result = FindMostSignificantSetBit(Freq);
		Assert(Result.Succes);

		u32 Shift = Result.Index + Adder;

		u64 x0 = Freq - 1;
		u64 x1 = 1ull << (Shift + 31);

		u64 t1 = x1 / Freq;
		x0 += (x1 % Freq) << 32;
		u64 t0 = x0 / Freq;

		Sym->RcpFreq = t0 + (t1 << 32);
		Sym->RcpShift = Shift - 1;
		Sym->Bias = CumStart;
	}
}

static inline void
RansDecSymInit(rans_dec_sym64* DecSym, u32 CumStart, u32 Freq)
{
	Assert(CumStart <= (1 << 16));
	Assert(Freq <= (1 << 16) - CumStart);

	DecSym->CumStart = CumStart;
	DecSym->Freq = Freq;
}

#endif