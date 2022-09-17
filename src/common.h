#pragma once

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cassert>
#include <vector>
#include <limits>
#include <stdint.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef size_t memory_index;

typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;

typedef float f32;
typedef double f64;

typedef u32 b32;
typedef u16 b16;
typedef u8 b8;

typedef uintptr_t umm;

using ByteVec = std::vector<u8>;

#define Assert(Expression) if (!(Expression)) *((int *)0) = 0;
#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

static constexpr u32 MaxUInt32 = std::numeric_limits<u32>::max();

static constexpr u32 CodeBit = 16;
static constexpr u32 FreqBit = 14;
static constexpr u32 CodeMaxValue = (1 << CodeBit) - 1;
static constexpr u32 FreqMaxValue = (1 << FreqBit) - 1;
static constexpr u32 OneFourth = (1 << (CodeBit - 2));
static constexpr u32 OneHalf = OneFourth * 2;
static constexpr u32 ThreeFourths = OneFourth * 3;

static_assert((CodeBit + FreqBit) <= 32, "not ok");

struct file_data
{
	u8* Data;
	u64 Size;
};

struct prob
{
	u32 lo;
	u32 hi;
	u32 count;
};

inline void
MemSet(u8* Dest, u64 Size, u8 Value)
{
	while (Size--)
	{
		*Dest++ = Value;
	}
}

inline void
MemSet(u32* Dest, u64 Size, u32 Value)
{
	while (Size--)
	{
		*Dest++ = Value;
	}
}

inline void
ZeroSize(void* Ptr, u64 Size)
{
	MemSet(static_cast<u8*>(Ptr), Size, 0);
}

file_data
ReadFile(const char* Name)
{
	file_data Result = {};

	FILE* f = fopen(Name, "rb");
	if (!f)
	{
		printf("can't open file: %s!\n", Name);
		exit(1);
	}

	fseek(f, 0, SEEK_END);
	Result.Size = ftell(f);
	fseek(f, 0, SEEK_SET);

	Result.Data = new uint8_t[Result.Size];

	if (fread(Result.Data, 1, Result.Size, f) != Result.Size)
	{
		printf("error during file reading!\n");
		exit(1);
	}

	fclose(f);
	return Result;
}
