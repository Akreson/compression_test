
struct see_context
{
	u16 Sum;
	u8 Shift;
	u8 Count;
};

struct see_bin_context
{
	u16 Scale;
};

class SEEState
{
public:
	see_context* LastUsed;
	u8 PrevSuccess;
	u8 NToIndex[256];
	u8 DiffToIndex[256];
	see_context Context[44][8];
	see_bin_context BinContext[128][16];

public:
	~SEEState() {}
	SEEState()
	{
		LastUsed = &Context[43][0];
		PrevSuccess = 0;

		u32 i;
		for (i = 0; i < 6; i++)
			NToIndex[i] = 2 * i;

		for (; i < 50; i++)
			NToIndex[i] = 12;

		for (; i < 256; i++)
			NToIndex[i] = 14;
		//
		for (i = 0; i < 4; i++)
			DiffToIndex[i] = i;

		for (; i < 4 + 8; i++)
			DiffToIndex[i] = 4 + ((i - 4) >> 1);

		for (; i < 4 + 8 + 32; i++)
			DiffToIndex[i] = 4 + 4 + ((i - 4 - 8) >> 2);

		for (; i < 256; i++)
			DiffToIndex[i] = 4 + 4 + 8 + ((i - 4 - 8 - 32) >> 3);
	}

	inline u8 getBinMean(u16 Scale)
	{
		u32 Shift = 7;
		u32 Round = 2;
		u8 Result = (Scale + (1 << (Shift - Round))) >> Shift;
		return Result;
	}

	inline see_bin_context* getBinContext(context* PPMCont)
	{
		u32 CountIndex = PrevSuccess + NToIndex[PPMCont->Prev->SymbolCount - 1];
		see_bin_context* Result = &BinContext[PPMCont->Data[0].Freq - 1][CountIndex];
		return Result;
	}

	inline u16 getMean(see_context* Context)
	{
		u32 Result = Context->Sum >> Context->Shift;
		Context->Sum -= Result;
		Result = Result ? Result : 1;
		return Result;
	}

	inline void updateLastUsed(void)
	{
		if ((LastUsed->Shift < CTX_MAX_BITS) && (--LastUsed->Count == 0)) {
			LastUsed->Sum += LastUsed->Sum;
			LastUsed->Count = 2 << ++LastUsed->Shift;
		}
	}

	inline u16 getContextMean(context* PPMCont, u32 Diff, u32 MaskedCount)
	{
		u16 Result;

		if (PPMCont->SymbolCount != 256)
		{
			u32 Index = 0;
			Index += (Diff < (PPMCont->Prev->SymbolCount - PPMCont->SymbolCount)) ? 4 : 0;
			Index += (PPMCont->TotalFreq < (11 * PPMCont->SymbolCount)) ? 2 : 0;
			Index += (MaskedCount > Diff) ? 1 : 0;

			LastUsed = &Context[DiffToIndex[Diff - 1]][Index];
			Result = getMean(LastUsed);
		}
		else
		{
			LastUsed = &Context[43][0];
			Result = 1;
		}

		return Result;
	}

	static inline see_context initContext(u32 Init)
	{
		see_context Result = {};

		Result.Shift = CTX_MAX_BITS - 3;
		Result.Sum = Init << Result.Shift;
		Result.Count = 16;

		return Result;
	}

	void init()
	{
		PrevSuccess = 0;

		static const u16 InitBinEsc[16] = {
			0x3CDD, 0x1F3F, 0x59BF, 0x48F3, 0x5FFB, 0x5545, 0x63D1, 0x5D9D,
			0x64A1, 0x5ABC, 0x6632, 0x6051, 0x68F6, 0x549B, 0x6BCA, 0x3AB0};

		for (u32 i = 0; i < 128; i++)
		{
			for (u32 j = 0; j < 16; j++)
			{
				BinContext[i][j].Scale = FREQ_MAX_VALUE - (InitBinEsc[j] / (i + 2));
			}
		}

		for (u32 i = 0; i < 44; i++)
		{
			for (u32 j = 0; j < 8; j++)
			{
				Context[i][j] = SEEState::initContext(4 * i + 8);
			}
		}
	}
};