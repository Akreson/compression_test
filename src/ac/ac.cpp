#include "ac_params.h"

struct ArithBitEncoder
{
public:
	u32 lo;
	u32 hi;
	u32 PendingBits;

	ByteVec& Bytes;
	u8 BitBuff;
	u8 BitAccumCount;

	ArithBitEncoder() = delete;
	~ArithBitEncoder() = default;
	ArithBitEncoder(ByteVec& OutBuffer) : Bytes(OutBuffer)
	{
		initVal();
	}

	inline void flush()
	{
		PendingBits++;
		if (lo < ONE_FOURTH) writeBit(0);
		else writeBit(1);

		if (BitBuff) Bytes.push_back(BitBuff);
	}

	inline void reset()
	{
		Bytes.clear();
		initVal();
	}

	void encode(prob Prob)
	{
		Assert(Prob.scale <= PROB_MAX_VALUE);

		u32 step = ((hi - lo) + 1) / Prob.scale;
		hi = lo + (Prob.hi * step) - 1;
		lo = lo + (Prob.lo * step);

		for (;;)
		{
			if (hi < ONE_HALF)
			{
				writeBit(0);
			}
			else if (lo >= ONE_HALF)
			{
				writeBit(1);
			}
			else if ((lo >= ONE_FOURTH) && (hi < THREE_FOURTHS))
			{
				++PendingBits;
				lo -= ONE_FOURTH;
				hi -= ONE_FOURTH;
			}
			else break;

			hi <<= 1;
			hi++;
			lo <<= 1;
			hi &= CODE_MAX_VALUE;
			lo &= CODE_MAX_VALUE;
		}
	}

private:
	
	inline void initVal()
	{
		lo = 0;
		hi = CODE_MAX_VALUE;
		PendingBits = 0;
		BitBuff = 0;
		BitAccumCount = 8;
	}

	inline void fillBitBuff(u32 Bit)
	{
		--BitAccumCount;
		BitBuff |= Bit << BitAccumCount;

		if (!BitAccumCount)
		{
			Bytes.push_back(BitBuff);
			BitAccumCount = 8;
			BitBuff = 0;
		}
	}

	void writeBit(u32 Bit)
	{
		fillBitBuff(Bit);

		u32 ReverseBit = (!Bit) & 1;
		while (PendingBits)
		{
			fillBitBuff(ReverseBit);
			--PendingBits;
		}
	}
};

struct ArithBitDecoder
{
public:
	u32 lo;
	u32 hi;
	u32 code;

	ByteVec& BytesIn;

	u64 InSize;

	u32 ReadBytesPos;
	u32 ReadBitPos;

	ArithBitDecoder() = delete;
	~ArithBitDecoder() = default;

	ArithBitDecoder(ByteVec& InputBuffer) : BytesIn(InputBuffer)
	{
		InSize = InputBuffer.size();
		reset();
	}

	inline void reset()
	{
		lo = 0;
		hi = CODE_MAX_VALUE;
		code = 0;
		ReadBytesPos = 0;
		ReadBitPos = 8;

		Assert((CODE_BITS % 8) == 0);
		for (u32 i = 0; i < (CODE_BITS >> 3); ++i)
		{
			code = (code << 8) | getByte();
		}
	}

	u32 getCurrFreq(u32 Scale)
	{
		u32 step = ((hi - lo) + 1) / Scale;
		u32 scaledValue = (code - lo) / step;
		return scaledValue;
	}

	void updateDecodeRange(prob Prob)
	{
		u32 step = ((hi - lo) + 1) / Prob.scale;
		hi = lo + (Prob.hi * step) - 1;
		lo = lo + (Prob.lo * step);

		for (;;)
		{
			if (hi < ONE_HALF)
			{
			}
			else if (lo >= ONE_HALF)
			{
			}
			else if ((lo >= ONE_FOURTH) && (hi < THREE_FOURTHS))
			{
				code -= ONE_FOURTH;
				hi -= ONE_FOURTH;
				lo -= ONE_FOURTH;
			}
			else break;

			hi <<= 1;
			hi++;
			lo <<= 1;
			hi &= CODE_MAX_VALUE;
			lo &= CODE_MAX_VALUE;

			code = shiftBitToCode();
			code &= CODE_MAX_VALUE;
		}
	}

private:
	inline u32 shiftBitToCode()
	{
		u32 Result = code << 1;

		if (ReadBytesPos < InSize)
		{
			u8 byte = BytesIn[ReadBytesPos];

			--ReadBitPos;
			Result |= (byte >> ReadBitPos) & 1;

			if (!ReadBitPos)
			{
				++ReadBytesPos;
				ReadBitPos = 8;
			}
		}

		return Result;
	}

	inline u8 getByte()
	{
		u8 Result = 0;

		if (ReadBytesPos < InSize)
		{
			Result = BytesIn[ReadBytesPos++];
		}

		return Result;
	}
};