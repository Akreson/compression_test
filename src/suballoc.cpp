
struct mem_block
{
	mem_block* Next;
	mem_block* Prev;
	u32 Size;
	u32 Used; // delete?
};

class StaticSubAlloc
{
	static constexpr u32 MemBlockSize = sizeof(mem_block);
	static constexpr u32 PtrAlign = sizeof(void*);

	void* Memory;
	mem_block FreeSentinel;
	u32 TotalSize;

public:
	StaticSubAlloc() = delete;
	StaticSubAlloc(u32 SizeToReserve)
	{
		TotalSize = SizeToReserve;
		Memory = new u8[SizeToReserve];
		memset(Memory, 0, SizeToReserve);

		FreeSentinel.Next = &FreeSentinel;
		FreeSentinel.Prev = &FreeSentinel;

		mem_block* FreeBlock = reinterpret_cast<mem_block*>(Memory);
		FreeBlock->Used = FreeBlock->Size = SizeToReserve - MemBlockSize;

		insertBlockNext(&FreeSentinel, FreeBlock);
	}

	~StaticSubAlloc()
	{
		delete[] Memory;
	}

	template<typename T>
	inline T* alloc(u32 ReqSize)
	{
		return reinterpret_cast<T*>(alloc(ReqSize));
	}

	u8* alloc(u32 ReqSize)
	{
		u8* Result = nullptr;
		u32 AllocSize = alignSizeForwad(ReqSize);

		for (mem_block* FreeBlock = FreeSentinel.Next;
			FreeBlock != &FreeSentinel;
			FreeBlock = FreeBlock->Next)
		{
			if (FreeBlock->Size >= AllocSize)
			{
				u32 LeftSize = FreeBlock->Size - AllocSize;
				u32 MinimalAlloc = alignSizeForwad(MemBlockSize + 1);

				if (LeftSize >= MinimalAlloc)
				{
					mem_block* NewFreeBlock = getNewBlockPtr(FreeBlock, AllocSize);

					NewFreeBlock->Used = 0;
					NewFreeBlock->Size = LeftSize - MemBlockSize;
					
					FreeBlock->Size = AllocSize;
					
					insertBlockNext(FreeBlock, NewFreeBlock);
				}
				else
				{
					AllocSize = FreeBlock->Size;
				}

				FreeBlock->Used = AllocSize - (AllocSize - ReqSize);

				Result = reinterpret_cast<u8*>(FreeBlock) + MemBlockSize;
				removeBlock(FreeBlock);

				break;
			}
		}

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

		mem_block* Block = reinterpret_cast<mem_block*>(Ptr - MemBlockSize);
		mem_block* AfterBlock = getNewBlockPtr(Block, Block->Size);

		Block->Used = 0;
		b32 IsInserted = false;

		for (mem_block* FreeBlock = FreeSentinel.Next;
			FreeBlock != &FreeSentinel;
			FreeBlock = FreeBlock->Next)
		{
			if (FreeBlock > Block)
			{
				mem_block* Prev = FreeBlock->Prev;
				mem_block* AfterPrev = (Prev == &FreeSentinel) ? 0 : getNewBlockPtr(Prev, Prev->Size);

				if (FreeBlock == AfterBlock)
				{
					Block->Size += FreeBlock->Size + MemBlockSize;
					removeBlock(FreeBlock);
				}
			
				if (AfterPrev && (AfterPrev == Block))
				{
					Prev->Size += Block->Size + MemBlockSize;
				}
				else
				{
					insertBlockNext(Prev, Block);
				}

				break;
			}
		}
	}

	template<typename T>
	inline T* realloc(T* Ptr, u32 NewSize)
	{
		return reinterpret_cast<T*>(realloc(reinterpret_cast<u8*>(Ptr), NewSize));
	}

	u8* realloc(u8* Ptr, u32 NewSize)
	{
		u8* Result = nullptr;
		mem_block* Block = reinterpret_cast<mem_block*>(Ptr - MemBlockSize);

		if (NewSize <= Block->Size)
		{
			if (NewSize > Block->Used)
			{
				Block->Used = NewSize;
				Result = Ptr;
			}
			else
			{
				u32 MinimalAlloc = alignSizeForwad(MemBlockSize + 1);
				u32 Diff = Block->Size - NewSize;
			
				if (Diff < MinimalAlloc)
				{
					Block->Used = NewSize;
					Result = Ptr;
				}
			}
		}

		if (!Result)
		{
			dealloc(Ptr);
			Result = alloc(NewSize);
		}

		return Result;
	}

private:
	inline u32 alignSizeForwad(u32 Size, u32 Alignment = PtrAlign)
	{
		Assert(!(Alignment & (Alignment - 1)));

		u32 Result = Size;
		u32 AlignMask = Alignment - 1;
		u32 OffsetFromMask = (Size & AlignMask);
		u32 AlignOffset = OffsetFromMask ? (Alignment - OffsetFromMask) : 0;

		Result += AlignOffset;
		return Result;
	}

	inline mem_block* getNewBlockPtr(mem_block* Block, u32 Size)
	{
		Assert(Block);
		return reinterpret_cast<mem_block*>(reinterpret_cast<u8*>(Block) + MemBlockSize + Size);
	}

	inline void insertBlockNext(mem_block* Target, mem_block* ToInsert)
	{
		Assert(Target && ToInsert);

		ToInsert->Next = Target->Next;
		ToInsert->Prev = Target;
		ToInsert->Next->Prev = ToInsert;
		ToInsert->Prev->Next = ToInsert;
	}
	
	inline void insertBlockPrev(mem_block* Target, mem_block* ToInsert)
	{
		Assert(Target && ToInsert);

		ToInsert->Next = Target;
		ToInsert->Prev = Target->Prev;
		ToInsert->Next->Prev = ToInsert;
		ToInsert->Prev->Next = ToInsert;
	}

	inline void removeBlock(mem_block* Block)
	{
		Assert(Block);

		Block->Next->Prev = Block->Prev;
		Block->Prev->Next = Block->Next;
		Block->Next = nullptr;
		Block->Prev = nullptr;
	}
};