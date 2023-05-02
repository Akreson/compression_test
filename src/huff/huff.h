#if !defined(HUFF_H)
#define HUFF_H

static constexpr u32 HUFF_MAX_CODELEN = 16;

struct huff_node
{
	u32 Freq;
	u16 Parent;
	u8 Len;
	u8 Sym;
};

struct huff_def_build_iter
{
	u32 NodeAt;
	u32 BuildAt;
	u32 InsertAt;
};

struct huff_enc_entry
{
	u16 Len;
	u16 Code;
};

static_assert(sizeof(huff_enc_entry) == sizeof(u32));

struct huff_dec_entry
{
	union
	{
		u16 Val;

		struct
		{
			u8 Sym;
			u8 Len;
		};
	};
};

static_assert(sizeof(huff_dec_entry) == sizeof(u16));

#endif
