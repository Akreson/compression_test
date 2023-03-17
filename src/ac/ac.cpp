#include "ac_params.h"

class ArithEncoder
{
	u32 lo;
	u32 hi;
	u32 PendingBits;

	ByteVec& Bytes;
	u8 BitBuff;
	u8 BitAccumCount;

public:
	ArithEncoder() = delete;
	~ArithEncoder() = default;

	ArithEncoder(ByteVec& OutBuffer) :
		Bytes(OutBuffer)
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

	inline void initVal()
	{
		lo = 0;
		hi = CODE_MAX_VALUE;
		PendingBits = 0;
		BitBuff = 0;
		BitAccumCount = 8;
	}

	void encode(prob Prob)
	{
		u32 step = ((hi - lo) + 1) / Prob.scale;
		hi = lo + (step * Prob.hi) - 1;
		lo = lo + (step * Prob.lo);

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
			else if ((lo >= ONE_FOURTH) && (hi < TREE_FOURTHS))
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

		u32 ReverseBit = !Bit & 1;
		while (PendingBits)
		{
			fillBitBuff(ReverseBit);
			--PendingBits;
		}
	}
};

class ArithDecoder
{
	u32 lo;
	u32 hi;
	u32 code;

	ByteVec& BytesIn;

	u64 InSize;

	u32 ReadBytesPos;
	u32 ReadBitPos;

public:
	ArithDecoder() = delete;
	~ArithDecoder() = default;

	ArithDecoder(ByteVec& InputBuffer) :
		BytesIn(InputBuffer), lo(0), hi(CODE_MAX_VALUE), code(0), ReadBytesPos(0), ReadBitPos(8)
	{
		InSize = InputBuffer.size();
		initVal();
	}

	inline void initVal()
	{
		lo = 0;
		hi = CODE_MAX_VALUE;
		code = 0;
		ReadBytesPos = 0;
		ReadBitPos = 8;

		for (u32 i = 0; i < 3; ++i)
		{
			code = (code << 8) | getByte();
		}
	}

	u32 getCurrFreq(u32 Scale)
	{
		u32 step = ((hi - lo) + 1) / Scale;
		u32 ScaledValue = (code - lo) / step;
		return ScaledValue;
	}

	void updateDecodeRange(prob Prob)
	{
		u32 step = ((hi - lo) + 1) / Prob.scale;

		hi = lo + (step * Prob.hi) - 1;
		lo = lo + (step * Prob.lo);

		for (;;)
		{
			if (hi < ONE_HALF)
			{
			}
			else if (lo >= ONE_HALF)
			{
			}
			else if ((lo >= ONE_FOURTH) && (hi < TREE_FOURTHS))
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