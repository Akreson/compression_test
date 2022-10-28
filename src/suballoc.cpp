
#define DEBUG_SUB_ALLOC 0
#define PRINT_LOG_SUB_ALLOC 0

#if PRINT_LOG_SUB_ALLOC
#define LOG(str, ...) printf(str, ##__VA_ARGS__);
#else
#define LOG()
#endif

#if DEBUG_SUB_ALLOC
static constexpr u32 DebugBlockMaxSize = 1 << 13;
static u32 DebugBlocksCount[DebugBlockMaxSize];
#endif

struct mem_block
{
	u64 Size : 31, PrevSize : 30, IsFree : 1;
};

struct free_mem_block
{
	mem_block Mem;
	free_mem_block* Next;
	free_mem_block* Prev;
};

class StaticSubAlloc
{
	static constexpr u32 MemBlockSize = AlignSizeForwad(sizeof(mem_block));
	static constexpr u32 FreeMemBlockSize = AlignSizeForwad(sizeof(free_mem_block));
	static constexpr u32 MaxBlockFreeSize = std::numeric_limits<u32>::max() >> 2;

	void* Memory;
	mem_block* EndOfMemBlock;
	free_mem_block FreeSentinel;
	u64 TotalSize;
	u32 MinAlloc;
	u32 MinUse;

	//u32 RcpMinUse;
	//u32 RcpShift;

public:
#if DEBUG_SUB_ALLOC
	u32 FreeMem;
	u32 FreeListCount;
#endif

	StaticSubAlloc() {};
	StaticSubAlloc(u64 SizeToReserve, u32 MinAllocSize = 1) : Memory(0)
	{
		init(SizeToReserve, MinAllocSize);
	}

	~StaticSubAlloc()
	{
		delete[] Memory;
		Memory = nullptr;
	}

	void init(u64 SizeToReserve, u32 MinAllocSize)
	{
		MinAlloc = AlignSizeForwad(MinAllocSize, 2);
		MinUse = AlignSizeForwad(MinAlloc + MemBlockSize);

		if (MinUse < FreeMemBlockSize)
		{
			MinUse = FreeMemBlockSize;
		}

		if (SizeToReserve < MinUse)
		{
			SizeToReserve = MinUse;
		}
		
		setAlignForMinUse();
		TotalSize = SizeToReserve;

		Memory = new u8[TotalSize];
		EndOfMemBlock = reinterpret_cast<mem_block*>(static_cast<u8*>(Memory) + TotalSize);

		reset();
	}

	void reset()
	{
		if (Memory)
		{
#if DEBUG_SUB_ALLOC
			FreeListCount = 0;
			FreeMem = TotalSize;
#endif
			FreeSentinel = {};
			FreeSentinel.Next = &FreeSentinel;
			FreeSentinel.Prev = &FreeSentinel;

			free_mem_block* FreeBlock = reinterpret_cast<free_mem_block*>(Memory);

			if (TotalSize > MaxBlockFreeSize)
			{
				u64 LeftTotal = TotalSize;
				free_mem_block* Prev = &FreeSentinel;

				do
				{
					u64 BlockSize = LeftTotal > MaxBlockFreeSize ? MaxBlockFreeSize : LeftTotal;
					LeftTotal -= MaxBlockFreeSize;

					if (LeftTotal < MinUse)
					{
						BlockSize += LeftTotal;
					}
					
					FreeBlock->Mem.IsFree = true;
					FreeBlock->Mem.PrevSize = Prev->Mem.Size;
					FreeBlock->Mem.Size = BlockSize - MemBlockSize;

					insertBlockPrev(&FreeSentinel, FreeBlock);
					Prev = FreeBlock;
					FreeBlock = getNextBlockPtr<free_mem_block*>(FreeBlock, FreeBlock->Mem.Size);

#if DEBUG_SUB_ALLOC
					FreeMem -= MemBlockSize;
#endif
				}
				while (FreeBlock != reinterpret_cast<free_mem_block*>(EndOfMemBlock));
			}
			else
			{
				FreeBlock->Mem.IsFree = true;
				FreeBlock->Mem.PrevSize = 0;
				FreeBlock->Mem.Size = TotalSize - MemBlockSize;

#if DEBUG_SUB_ALLOC
				FreeMem -= MemBlockSize;
#endif
				insertBlockNext(&FreeSentinel, FreeBlock);
			}
		}
	}

	template<typename T>
	inline T* alloc(u32 CountOfElement)
	{
		return reinterpret_cast<T*>(alloc(sizeof(T) * CountOfElement));
	}

	u8* alloc(u32 ReqSize)
	{
		u8* Result = nullptr;

		u32 AllocSize = alignSizeWithMinAllocForward(ReqSize);
		u32 MinLeftSize = AllocSize + MemBlockSize;

		free_mem_block* Prev = &FreeSentinel;
		for (free_mem_block* FreeBlock = FreeSentinel.Next;
			FreeBlock != &FreeSentinel;
			FreeBlock = FreeBlock->Next)
		{
			Assert(FreeBlock->Mem.Size);

			if (FreeBlock->Mem.Size >= AllocSize)
			{
				FreeBlock->Mem.IsFree = false;

				u32 LeftSize = FreeBlock->Mem.Size - AllocSize;
				if (LeftSize >= MinUse)
				{
					free_mem_block* NewFreeBlock = getNextBlockPtr<free_mem_block*>(FreeBlock, AllocSize);

					NewFreeBlock->Mem.Size = LeftSize - MemBlockSize;
					NewFreeBlock->Mem.PrevSize = AllocSize;
					NewFreeBlock->Mem.IsFree = true;

					FreeBlock->Mem.Size = AllocSize;

					patchNextPrevSize(NewFreeBlock, NewFreeBlock->Mem.Size);
					
					Assert(NewFreeBlock->Mem.PrevSize == FreeBlock->Mem.Size);
					insertBlockNext(&FreeSentinel, NewFreeBlock);

#if DEBUG_SUB_ALLOC
					FreeListCount++;
					FreeMem -= MemBlockSize + AllocSize;
#endif
				}

				Result = reinterpret_cast<u8*>(FreeBlock) + MemBlockSize;

				Assert(FreeBlock->Mem.Size);				
				removeBlock(FreeBlock);

#if DEBUG_SUB_ALLOC
				FreeMem -= AllocSize;
				FreeListCount--;
#endif
				break;
			}

			Prev = FreeBlock;
		}

#if DEBUG_SUB_ALLOC
		if (!Result)
		{
			logFreeMemInfo();
		}
#endif
		return Result;
	}

	template<typename T>
	inline void dealloc(T* Ptr)
	{
		dealloc(reinterpret_cast<u8*>(Ptr));
	}

	void dealloc(u8* Ptr)
	{
		Assert(Ptr);

		free_mem_block* Block = reinterpret_cast<free_mem_block*>(Ptr - MemBlockSize);
		Assert(Block->Mem.Size);

		mem_block* NextBlock = getNextBlockPtr(Block, Block->Mem.Size);

#if DEBUG_SUB_ALLOC
		FreeMem += Block->Mem.Size;
#endif

		if (EndOfMemBlock != NextBlock)
		{
			Assert(NextBlock->Size);
			Assert(Block->Mem.Size == NextBlock->PrevSize);

			if (NextBlock->IsFree)
			{
				Block->Mem.Size += NextBlock->Size + MemBlockSize;

				patchNextPrevSize(NextBlock, Block->Mem.Size);
				removeBlock(reinterpret_cast<free_mem_block*>(NextBlock));

#if DEBUG_SUB_ALLOC
				FreeListCount--;
				FreeMem += MemBlockSize;
#endif
			}
		}

		free_mem_block* FirstBlock = reinterpret_cast<free_mem_block*>(Memory);
		if (FirstBlock != Block)
		{ 
			mem_block* PrevBlock = getPrevBlockPtr(Block, Block->Mem.PrevSize);

			Assert(PrevBlock->Size);
			Assert(PrevBlock->Size == Block->Mem.PrevSize);

			if (PrevBlock->IsFree)
			{
				PrevBlock->Size += Block->Mem.Size + MemBlockSize;
				patchNextPrevSize(Block, PrevBlock->Size);

#if DEBUG_SUB_ALLOC
				FreeMem += MemBlockSize;
#endif
			}
			else
			{
				Block->Mem.IsFree = true;
				insertBlockNext(&FreeSentinel, Block);

#if DEBUG_SUB_ALLOC
				FreeListCount++;
#endif
			}
		}
	}

	template<typename T>
	inline T* realloc(T* Ptr, u32 NewCount, u32 PreallocCount = 0)
	{
		return reinterpret_cast<T*>(realloc(reinterpret_cast<u8*>(Ptr), sizeof(T) * NewCount, sizeof(T) * PreallocCount));
	}

	u8* realloc(u8* Ptr, u32 NewSize, u32 PreallocSize = 0)
	{
		Assert(Ptr);

		PreallocSize = PreallocSize > NewSize ? PreallocSize : NewSize;

		u8* Result = nullptr;
		mem_block* Block = reinterpret_cast<mem_block*>(Ptr - MemBlockSize);

		if (NewSize <= Block->Size)
		{
			Result = Ptr;
		}

		if (!Result)
		{
			Result = alloc(PreallocSize);
			if (Result)
			{
				Copy(Block->Size, Result, Ptr);
			}

			dealloc(Ptr);
		}

		return Result;
	}

private:
	inline void setAlignForMinUse()
	{
		/*
		bit_scan_result Result = FindMostSignificantSetBit(MinUse);
		Assert(Result.Succes);

		u32 Shift = Result.Index + 1;
		RcpMinUse = (u32)(((1ull << (Shift + 31)) + MinUse - 1) / MinUse);
		RcpShift = Shift - 1;
		*/
	}

	//NOTE: for non power of 2 align
	inline u32 alignSizeWithMinAllocForward(u32 Size)
	{
		/*
		u32 q = (u32)(((u64)Size * RcpMinUse) >> 32) >> RcpShift;
		u32 Rem = Size - q*MinUse;
		*/

		u32 Rem = Size % MinUse;
		u32 Result = (Size - Rem) + MinUse;

		return Result;
	}

	template<typename T>
	inline void patchNextPrevSize(T* NextAfter, u32 NewPrevSize)
	{
		Assert(NewPrevSize <= MaxBlockFreeSize);

		mem_block* MemBlock = reinterpret_cast<mem_block*>(NextAfter);
		mem_block* OldNext = getNextBlockPtr(MemBlock, MemBlock->Size);

		if (EndOfMemBlock != OldNext)
		{
			OldNext->PrevSize = NewPrevSize;
		}
	}

	template<typename RetType = mem_block*, typename T>
	inline RetType getNextBlockPtr(T* Block, u32 Size)
	{
		Assert(Block);
		return reinterpret_cast<RetType>(reinterpret_cast<u8*>(Block) + MemBlockSize + Size);
	}

	template<typename RetType = mem_block*, typename T>
	inline RetType getPrevBlockPtr(T* Block, u32 PrevSize)
	{
		Assert(Block);
		return reinterpret_cast<RetType>(reinterpret_cast<u8*>(Block) - PrevSize - MemBlockSize);
	}

	inline void insertBlockNext(free_mem_block* Target, free_mem_block* Insert)
	{
		Assert(Target && Insert);

		Insert->Next = Target->Next;
		Insert->Prev = Target;
		Insert->Next->Prev = Insert;
		Insert->Prev->Next = Insert;
	}
	
	inline void insertBlockPrev(free_mem_block* Target, free_mem_block* Insert)
	{
		Assert(Target && Insert);

		Insert->Next = Target;
		Insert->Prev = Target->Prev;
		Insert->Next->Prev = Insert;
		Insert->Prev->Next = Insert;
	}

	inline void removeBlock(free_mem_block* Block)
	{
		Assert(Block);

		Block->Next->Prev = Block->Prev;
		Block->Prev->Next = Block->Next;
		Block->Next = nullptr;
		Block->Prev = nullptr;
	}

public:
	void memcheck()
	{
#if DEBUG_SUB_ALLOC
		memset(DebugBlocksCount, 0, DebugBlockMaxSize);

		u32 FreeListTotalMem = 0;
		u32 Largest = 0;
		u32 i = 0;
		for (free_mem_block* FreeBlock = FreeSentinel.Next;
			FreeBlock != &FreeSentinel;
			FreeBlock = FreeBlock->Next)
		{
			Assert(FreeBlock->Mem.Size);
			FreeListTotalMem += FreeBlock->Mem.Size;

			Largest = FreeBlock->Mem.Size > Largest ? FreeBlock->Mem.Size : Largest;

			if (FreeBlock->Mem.Size < (1 << 13))
			{
				DebugBlocksCount[FreeBlock->Mem.Size]++;
			}
			i++;
		}
#endif
	}

	void logFreeMemInfo()
	{
#if DEBUG_SUB_ALLOC
		memset(DebugBlocksCount, 0, DebugBlockMaxSize);

		u32 FreeListTotalMem = 0;
		u32 Largest = 0;
		u32 i = 0;
		for (free_mem_block* FreeBlock = FreeSentinel.Next;
			FreeBlock != &FreeSentinel;
			FreeBlock = FreeBlock->Next)
		{
			Assert(FreeBlock->Mem.Size);
			FreeListTotalMem += FreeBlock->Mem.Size;

			Largest = FreeBlock->Mem.Size > Largest ? FreeBlock->Mem.Size : Largest;

			if (FreeBlock->Mem.Size < DebugBlockMaxSize)
			{
				DebugBlocksCount[FreeBlock->Mem.Size]++;
			}

			i++;
		}

		u32 TotalFreeMem = (i * MemBlockSize) + FreeListTotalMem;
		LOG("(alloc) mem in free in:%d with:%d largest:%d free-list:%d\n", FreeListTotalMem, TotalFreeMem, Largest, FreeListCount);

		u32 mi = 0;
		double max = 0.0;
		u32 counter = 0;

		for (u32 i = 0; i < (1 << 13); i++)
		{
			if (DebugBlocksCount[i] > 0)
			{
				u32 TotalCount = DebugBlocksCount[i] * i + MemBlockSize * DebugBlocksCount[i];
				double per = (double)TotalCount / (double)TotalFreeMem;

				LOG(" (%d| bc:%d free:%d tmem:%d oft:%.3f) ", i, DebugBlocksCount[i], DebugBlocksCount[i] * i, TotalCount, per);

				if (++counter == 2)
				{
					counter = 0;
					LOG("\n");
				}
				if (max < per)
				{
					max = per;
					mi = i;
				}
			}
		}
		LOG("\n");

		u32 TotalCount = (DebugBlocksCount[mi] * mi) + (MemBlockSize * mi);
		double per = (double)TotalCount / (double)TotalFreeMem;

		LOG("max-used-space (%d| bc:%d free:%d tmem:%d oft:%.3f) ", mi, DebugBlocksCount[mi], DebugBlocksCount[mi] * mi, TotalCount, per);
		LOG("\n\n");
#endif
	}
};