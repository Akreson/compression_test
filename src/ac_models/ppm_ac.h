#if !defined(PPM_AC_H)
#define PPM_AC_H

struct context_data_excl
{
	u8 Data[256];

	static constexpr u8 Mask = MaxUInt8;
	static constexpr u8 ClearMask = 0;
};

struct context;

// TODO: fix for gcc and clang
#pragma pack(push, 1)
struct context_data
{
	context* Next;
	u8 Freq;
	u8 Symbol;
};
#pragma pack(pop)

struct context
{
	context_data* Data;
	context* Prev;
	u16 TotalFreq;
	u16 SymbolCount;

	static constexpr u32 MaxSymbol = 255;
};

struct decode_symbol_result
{
	prob Prob;
	u32 Symbol;
};

static const u8 ExpEscape[16] = {25, 14, 9, 7, 5, 5, 4, 4, 4, 3, 3, 3, 2, 2, 2, 2};

static constexpr u32 CTX_MAX_BITS = 7;
static constexpr u32 INTERVAL = 1 << CTX_MAX_BITS;
static constexpr u32 MAX_FREQ = 124;

#endif