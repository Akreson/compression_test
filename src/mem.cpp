using ByteVec = std::vector<u8>;

template<typename T> inline void
MemSet(T* Dest, u64 Size, T Value)
{
	while (Size--)
	{
		*Dest++ = Value;
	}
}

inline void
ZeroSize(void* Ptr, u64 Size)
{
	MemSet<u8>(static_cast<u8*>(Ptr), Size, 0);
}

#define ZeroStruct(Instance) ZeroSize((void *)&(Instance), sizeof(Instance))

inline void
MemCopy(size_t Size, void* DestBase, void* SourceBase)
{
	u8* Source = (u8*)SourceBase;
	u8* Dest = (u8*)DestBase;

	while (Size--)
	{
		*Dest++ = *Source++;
	}
}

struct StreamBuff
{
	u8* Start;
	u8* End;
	u8* Pos;

	StreamBuff() : Start(nullptr), End(nullptr), Pos(nullptr) {}
	StreamBuff(u8* Init, size_t Size)
	{
		init(Init, Size);
	}

	inline void init(u8* Init, size_t Size)
	{
		Pos = Start = Init;
		End = Init + Size;
	}

	inline u8 read8()
	{
		u8 Result = Pos < End ? *Pos : 0;
		return Result;
	}
};

struct BitWriter
{
	StreamBuff Stream;
	u32 BitCount;
	u32 BitBuff;

	BitWriter() : BitCount(0), BitBuff(0) {}
	BitWriter(u8* BuffStart, size_t Size) : Stream(BuffStart, Size), BitCount(0), BitBuff(0) {}

	inline void init(u8* BuffStart, size_t Size)
	{
		Stream.init(BuffStart, Size);
		BitBuff = BitCount = 0;
	}

	inline void writeMSB(u32 Val, u32 Len)
	{
		BitBuff = (BitBuff << Len) | Val;
		BitCount += Len;

		while (BitCount >= 8)
		{
			BitCount -= 8;
			*Stream.Pos = static_cast<u8>(BitBuff >> BitCount);
			Stream.Pos++;
		}
	}

	inline void finish()
	{
		u8 FlushByte = BitBuff << (8 - BitCount);
		*Stream.Pos = FlushByte;
		Stream.Pos++;
	}
};

struct BitReader
{
	StreamBuff Stream;
	u32 BitCount;
	u64 BitBuff;

	BitReader() : BitCount(0), BitBuff(0) {}
	BitReader(u8* BuffStart, size_t Size) : Stream(BuffStart, Size), BitCount(0), BitBuff(0) {}

	inline void init(u8* BuffStart, size_t Size)
	{
		Stream.init(BuffStart, Size);
		BitBuff = BitCount = 0;
	}

	inline void refillTo(u32 Count)
	{
		while (BitCount < Count)
		{
			u64 Byte = static_cast<u64>(Stream.read8());
			Stream.Pos++;

			BitBuff |= Byte << (56 - BitCount);
			BitCount += 8;
		}
	}

	inline u64 peek(u32 Count)
	{
		u64 Result = BitBuff >> (64 - Count);
		return Result;
	}

	inline void consume(u32 Count)
	{
		BitBuff <<= Count;
		BitCount -= Count;
	}
};