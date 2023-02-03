#if !defined(AC_PARAMS_H)
#define AC_PARAMS_H

static constexpr u32 CodeBit = 24;
static constexpr u32 FreqBit = 15;
static constexpr u32 FreqMaxBit = 14;
static constexpr u32 CodeMaxValue = (1 << CodeBit) - 1;
static constexpr u32 ProbMaxValue = (1 << FreqBit) - 1;
static constexpr u32 FreqMaxValue = (1 << FreqMaxBit);
static constexpr u32 OneFourth = (1 << (CodeBit - 2));
static constexpr u32 OneHalf = OneFourth * 2;
static constexpr u32 ThreeFourths = OneFourth * 3;

struct prob
{
	u32 lo;
	u32 hi;
	u32 scale;
};

#endif