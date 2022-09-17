
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
	ArithEncoder(ByteVec& OutBuffer) :
		Bytes(OutBuffer), lo(0), hi(CodeMaxValue), PendingBits(0), BitBuff(0), BitAccumCount(8) {}

	~ArithEncoder()
	{
		PendingBits++;
		if (lo < OneFourth) writeBit(0);
		else writeBit(1);

		if (BitBuff) Bytes.push_back(BitBuff);
	}

	void encode(prob Prob)
	{
		u32 range = (hi - lo) + 1;
		hi = lo + ((range * Prob.hi) / Prob.count) - 1;
		lo = lo + ((range * Prob.lo) / Prob.count);

		for (;;)
		{
			if (hi < OneHalf)
			{
				writeBit(0);
			}
			else if (lo >= OneHalf)
			{
				writeBit(1);
			}
			else if ((lo >= OneFourth) && (hi < ThreeFourths))
			{
				++PendingBits;
				lo -= OneFourth;
				hi -= OneFourth;
			}
			else break;

			hi <<= 1;
			hi++;
			lo <<= 1;
			hi &= CodeMaxValue;
			lo &= CodeMaxValue;
		}
	}

private:
	inline void fillBitBuff(u32 Bit)
	{
		--BitAccumCount;
		BitBuff |= Bit << BitAccumCount;

		if (!BitAccumCount)
		{
			BitAccumCount = 8;
			Bytes.push_back(BitBuff);
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
	ArithDecoder(ByteVec& InputBuffer) :
		BytesIn(InputBuffer), lo(0), hi(CodeMaxValue), code(0), ReadBytesPos(0), ReadBitPos(8)
	{
		InSize = InputBuffer.size();

		for (u32 i = 0; i < 2; ++i)
		{
			code = (code << 8) | getByte();
		}
	}

	~ArithDecoder() {}

	u32 getCurrFreq(u32 Scale)
	{
		u32 range = (hi - lo) + 1;
		u32 scaledValue = ((code - lo + 1) * Scale - 1) / range;
		return scaledValue;
	}

	void updateDecodeRange(prob Prob)
	{
		u32 range = (hi - lo) + 1;

		hi = lo + ((range * Prob.hi) / Prob.count) - 1;
		lo = lo + ((range * Prob.lo) / Prob.count);

		for (;;)
		{
			if (hi < OneHalf)
			{
			}
			else if (lo >= OneHalf)
			{
				code -= OneHalf;
				hi -= OneHalf;
				lo -= OneHalf;
			}
			else if ((lo >= OneFourth) && (hi < ThreeFourths))
			{
				code -= OneFourth;
				hi -= OneFourth;
				lo -= OneFourth;
			}
			else break;

			hi <<= 1;
			hi++;
			lo <<= 1;

			code = shiftBitToCode();
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