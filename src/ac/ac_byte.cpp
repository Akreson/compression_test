#include "ac_params.h"

struct ArithByteEncoder
{
	u32 lo, range;
	ByteVec& Bytes;

	ArithByteEncoder() = delete;
	ArithByteEncoder(ByteVec& OutBuffer) : Bytes(OutBuffer)
	{
		initVal();
	}

	~ArithByteEncoder()
	{
		if (range) flush();
	}

	inline void reset()
	{
		Bytes.clear();
		initVal();
	}

	void flush()
	{
		for (u32 i = 0; i < 4; i++)
		{
			u8 Byte = lo >> 24;
			lo <<= 8;
			Bytes.push_back(Byte);
		}
	}
	
	void encode(prob Prob)
	{
		range /= Prob.scale;
		lo += Prob.lo * range;
		range *= Prob.hi - Prob.lo;

		normalize();
	}

	void encodeShift(prob Prob)
	{
		Assert(Prob.scale <= FREQ_MAX_BITS);
		Assert(IsPowerOf2(1 << Prob.scale)); // prob.scale should be _n_ from 2^n

		range >>= Prob.scale;
		lo += Prob.lo * range;
		range *= Prob.hi - Prob.lo;

		normalize();
	}

private:
	inline void normalize()
	{
		for (;;)
		{
			if ((lo ^ (lo + range)) < CODE_MAX_VALUE)
			{
			}
			else if (range < PROB_MAX_VALUE)
			{
				range = (-((s32)lo)) & PROB_MAX_VALUE - 1;
			}
			else break;

			u8 Byte = lo >> 24;
			lo <<= 8;
			range <<= 8;
			Bytes.push_back(Byte);
		}
	}

	inline void initVal()
	{
		lo = 0;
		range = MaxUInt32;
	}
};

struct ArithByteDecoder
{
	u32 lo, range, code;
	ByteVec& BytesIn;

	u64 InSize;
	u32 ReadBytesPos;

	ArithByteDecoder() = delete;
	~ArithByteDecoder() = default;

	ArithByteDecoder(ByteVec& InputBuffer) : BytesIn(InputBuffer)
	{
		InSize = InputBuffer.size();
		reset();
	}

	inline void reset()
	{
		lo = 0;
		range = MaxUInt32;
		code = 0;
		ReadBytesPos = 0;

		for (u32 i = 0; i < 4; ++i)
		{
			code = (code << 8) | getByte();
		}
	}

	u32 getCurrFreq(u32 Scale)
	{
		range /= Scale;
		u32 Result = (code - lo) / (range);
		return Result;
	}

	u32 getCurrFreqShift(u32 ScaleShift)
	{
		Assert(IsPowerOf2(1 << ScaleShift));

		range >>= ScaleShift;
		u32 Result = (code - lo) / (range);
		return Result;
	}

	void updateDecodeRange(prob Prob)
	{
		lo += range * Prob.lo;
		range *= Prob.hi - Prob.lo;

		for (;;)
		{
			if ((lo ^ (lo + range)) < CODE_MAX_VALUE)
			{
			}
			else if (range < PROB_MAX_VALUE)
			{
				range = (-((s32)lo)) & PROB_MAX_VALUE - 1;
			}
			else break;

			u32 Byte = static_cast<u32>(getByte());
			code = (code << 8) | Byte;
			range <<= 8;
			lo <<= 8;
		}
	}

private:
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