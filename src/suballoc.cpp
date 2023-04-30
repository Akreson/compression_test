
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

template<u32 ReqMinAlloc>
class StaticSubAlloc
{
	static constexpr u32 MinAlloc = AlignSizeForward(ReqMinAlloc, 2);
	static constexpr u32 MemBlockSize = AlignSizeForward(sizeof(mem_block));
	static constexpr u32 FreeMemBlockSize = AlignSizeForward(sizeof(free_mem_block));
	static constexpr u32 MaxBlockFreeSize = std::numeric_limits<u32>::max() >> 2;

	u8* Memory;
	union
	{
		mem_block* MemBlock;
		free_mem_block* FreeMemBlock;
	} EndOf;
	free_mem_block FreeSentinel;
	u64 TotalSize;
	//u32 MinAlloc;
	u32 MinUse;

public:
#if DEBUG_SUB_ALLOC
	u32 FreeMem;
	u32 FreeListCount;
#endif

	StaticSubAlloc() {};
	StaticSubAlloc(u64 SizeToReserve) : Memory(nullptr)
	{
		init(SizeToReserve);
	}

	~StaticSubAlloc()
	{
		delete[] Memory;
		Memory = nullptr;
	}

	void init(u64 SizeToReserve)
	{
		//MinAlloc = AlignSizeForward(MinAlloc, 2);
		MinUse = AlignSizeForward(MinAlloc + MemBlockSize);

		if (MinUse < FreeMemBlockSize)
		{
			MinUse = FreeMemBlockSize;
		}

		if (SizeToReserve < MinUse)
		{
			SizeToReserve = MinUse;
		}
		
		TotalSize = SizeToReserve;
		Memory = new u8[TotalSize];
		EndOf.MemBlock = reinterpret_cast<mem_block*>(Memory + TotalSize);

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
				while (FreeBlock != EndOf.FreeMemBlock);
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
		Assert(ReqSize < MaxBlockFreeSize);

		u8* Result = nullptr;
		u32 AllocSize = alignSizeWithMinAllocForward(ReqSize);

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
					free_mem_block* NewFreeBlock = splitBlock(reinterpret_cast<mem_block*>(FreeBlock), AllocSize, LeftSize);
					
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
		}

#if DEBUG_SUB_ALLOC
		if (!Result)
		{
			logFreeMemInfo(AllocSize);
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

		if (EndOf.MemBlock != NextBlock)
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
				Block = nullptr;
#if DEBUG_SUB_ALLOC
				FreeMem += MemBlockSize;
#endif
			}
		}

		if (Block)
		{
			Block->Mem.IsFree = true;
			insertBlockNext(&FreeSentinel, Block);

#if DEBUG_SUB_ALLOC
			FreeListCount++;
#endif
		}
	}

	template<typename T>
	inline void shrink(T* Ptr, u32 NewCount, u32 FreeThreshInMinAlloc = 1)
	{
		shrink(reinterpret_cast<u8*>(Ptr), sizeof(T) * NewCount, FreeThreshInMinAlloc);
	}

	void shrink(u8* Ptr, u32 NewSize, u32 FreeThreshInMinAlloc = 1)
	{
		Assert(Ptr);
		mem_block* Block = reinterpret_cast<mem_block*>(Ptr - MemBlockSize);

		u32 NewSizeAlign = alignSizeWithMinAllocForward(NewSize);
		u32 FreeSize = Block->Size - NewSizeAlign;
		u32 DeallocSize = MemBlockSize + (FreeThreshInMinAlloc * MinAlloc);

		if (FreeSize >= DeallocSize)
		{
			free_mem_block* NewFreeBlock = splitBlock(Block, NewSizeAlign, FreeSize);
			u8* PtrToDealloc = reinterpret_cast<u8*>(NewFreeBlock) + MemBlockSize;
			dealloc(PtrToDealloc);
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
				MemCopy(Block->Size, Result, Ptr);
				dealloc(Ptr);
			}
		}

		return Result;
	}

private:
	//NOTE: for non power of 2 align
	inline u32 alignSizeWithMinAllocForward(u32 Size)
	{
		u32 Rem = Size % MinAlloc;
		u32 Result = (Size - Rem) + MinAlloc;
		if (Rem == 0) Result = Size;

		return Result;
	}

	inline free_mem_block* splitBlock(mem_block* SplitBlock, u32 SplitLeftSize, u32 NewTotalSize)
	{
		free_mem_block* NewFreeBlock = getNextBlockPtr<free_mem_block*>(SplitBlock, SplitLeftSize);

		NewFreeBlock->Mem.Size = NewTotalSize - MemBlockSize;
		NewFreeBlock->Mem.PrevSize = SplitLeftSize;
		NewFreeBlock->Mem.IsFree = true;

		SplitBlock->Size = SplitLeftSize;
		patchNextPrevSize(NewFreeBlock, NewFreeBlock->Mem.Size);

		return NewFreeBlock;
	}

	template<typename T>
	inline void patchNextPrevSize(T* NewAfter, u32 NewPrevSize)
	{
		Assert(NewPrevSize <= MaxBlockFreeSize);

		mem_block* MemBlock = reinterpret_cast<mem_block*>(NewAfter);
		mem_block* CurrNext = getNextBlockPtr(MemBlock, MemBlock->Size);

		if (EndOf.MemBlock != CurrNext)
		{
			CurrNext->PrevSize = NewPrevSize;
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
		memset(DebugBlocksCount, 0, DebugBlockMaxSize*4);

		u32 FreeListTotalMem = 0;
		u32 Largest = 0;
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
		}
#endif
	}

	void logFreeMemInfo(u32 LastReqSize)
	{
#if DEBUG_SUB_ALLOC
		memset(DebugBlocksCount, 0, DebugBlockMaxSize*4);

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
		LOG("(alloc) mem in free in:%u with:%u largest:%u free-list:%d (last-req: %u)\n", FreeListTotalMem, TotalFreeMem, Largest, FreeListCount, LastReqSize);

		u32 mi = 0;
		f64 max = 0.0;
		u32 counter = 0;

		for (u32 i = 0; i < (1 << 13); i++)
		{
			if (DebugBlocksCount[i] > 0)
			{
				u32 TotalCount = (DebugBlocksCount[i] * i) + (MemBlockSize * DebugBlocksCount[i]);
				f64 per = (f64)TotalCount / (f64)TotalFreeMem;

				LOG(" (%u| bc:%u free:%u tmem:%u oft:%.3f) ", i, DebugBlocksCount[i], DebugBlocksCount[i] * i, TotalCount, per);

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
		if (counter == 1) LOG("\n");
		LOG("\n");

		u32 TotalCount = (DebugBlocksCount[mi] * mi) + (MemBlockSize * DebugBlocksCount[mi]);
		f64 per = (f64)TotalCount / (f64)TotalFreeMem;

		LOG(" max-used-space (%d| bc:%d free:%d tmem:%d oft:%.3f)\n\n", mi, DebugBlocksCount[mi], DebugBlocksCount[mi] * mi, TotalCount, per);
#endif
	}
};