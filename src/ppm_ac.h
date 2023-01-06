#if !defined(PPM_AC_H)
#define PPM_AC_H

struct context_data_excl
{
	u16 Data[256];

	static constexpr u16 Mask = MaxUInt16;
	static constexpr u16 ClearMask = 0;
};

struct context;
struct context_data
{
	context* Next;
	u16 Freq;
	u8 Symbol;
};

struct context
{
	context_data* Data;
	context* Prev;
	u32 TotalFreq;
	u16 EscapeFreq;
	u16 SymbolCount;

	static constexpr u32 MaxSymbol = 255;
};

struct decode_symbol_result
{
	prob Prob;
	u32 SymbolIndex;
	u32 Symbol;
};

struct symbol_search_result
{
	b32 Success;
	u16 Index;
};

struct find_context_result
{
	context* Context;
	u16 Order;
	u16 ChainMissIndex;
	b16 IsNotComplete;
	b16 SymbolMiss;
};

#endif