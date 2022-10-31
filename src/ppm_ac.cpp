struct context_data_excl
{
	u16 Data[256];

	static constexpr u16 Mask = MaxUInt16;
	static constexpr u16 ClearMask = 0;
};

struct context;
struct context_data
{
	context* Next;
	u16 Freq;
	u8 Symbol;
};

struct context
{
	context_data* Data;
	context* Prev;
	u32 TotalFreq;
	u16 EscapeFreq;
	u16 SymbolCount;

	static constexpr u32 MaxSymbol = 255;
};

struct decode_symbol_result
{
	prob Prob;
	u32 SymbolIndex;
	u32 Symbol;
};

struct symbol_search_result
{
	b32 Success;
	u16 Index;
};

struct find_context_result
{
	context* Context;
	u16 Order;
	u16 ChainMissIndex;
	b16 IsNotComplete;
	b16 SymbolMiss;
};

class PPMByte
{
	context_data_excl* Exclusion;
	context* StaticContext; // order -1
	context* ContextZero;
	context* LastUsed;

	u32* ContextSeq;
	find_context_result* ContextStack;
	
	u32 OrderCount;
	u32 CurrSetOrderCount;
	u32 SeqLookAt;

public:
	StaticSubAlloc SubAlloc;
	static constexpr u32 EscapeSymbol = context::MaxSymbol + 1;

	// For debug
	u64 ContextCount;

	PPMByte() = delete;
	PPMByte(u32 MaxOrderContext, u32 MemLimit) :
		SubAlloc(MemLimit, sizeof(context_data)*2), OrderCount(MaxOrderContext)
	{
		initModel();
	}

	~PPMByte() {}

	void encode(ArithEncoder& Encoder, u32 Symbol)
	{
		SeqLookAt = 0;
		u32 OrderLooksLeft = CurrSetOrderCount + 1;
		
		context* Prev = 0;
		while (OrderLooksLeft)
		{
			find_context_result Find = {};

			if (Prev)
			{
				Find.Context = Prev;
			}
			else
			{
				Find = findContext();
			}

			if (!Find.IsNotComplete)
			{
				b32 Success = encodeSymbol(Encoder, Find.Context, Symbol);
				if (Success)
				{
					LastUsed = Find.Context;
					break;
				}

				Prev = Find.Context->Prev;
				Find.SymbolMiss = true;

				Assert (Find.Context->TotalFreq)
				updateExclusionData(Find.Context);
			}

			ContextStack[SeqLookAt++] = Find;
			OrderLooksLeft--;
		}

		if (!OrderLooksLeft)
		{
			LastUsed = StaticContext;
			
			prob Prob = {};
			b32 Success = getProb(StaticContext, Prob, Symbol);
			Assert(Success);
			Encoder.encode(Prob);

			Assert(SeqLookAt);
		}

		if (update(Symbol))
		{
			updateOrderSeq(Symbol);
		}

		clearExclusion();
	}

	u32 decode(ArithDecoder& Decoder)
	{
		u32 ResultSymbol;
		
		SeqLookAt = 0;
		u32 OrderLooksLeft = CurrSetOrderCount + 1;
		
		context* Prev = 0;
		while (OrderLooksLeft)
		{
			find_context_result Find = {};

			if (Prev)
			{
				Find.Context = Prev;
			}
			else
			{
				Find = findContext();
			}

			if (!Find.IsNotComplete)
			{
				b32 Success = false;
				Assert(Find.Context->TotalFreq)

				Success = decodeSymbol(Decoder, Find.Context, &ResultSymbol);
				updateExclusionData(Find.Context);

				if (Success)
				{
					LastUsed = Find.Context;
					break;
				}

				Prev = Find.Context->Prev;
			}

			ContextStack[SeqLookAt++] = Find;
			OrderLooksLeft--;
		}

		if (!OrderLooksLeft)
		{
			LastUsed = StaticContext;

			u32 ExclTotal = getExcludedTotal(StaticContext) + 1;
			u32 CurrFreq = Decoder.getCurrFreq(ExclTotal);
			decode_symbol_result DecodedSymbol = getSymbolFromFreq(StaticContext, CurrFreq, ExclTotal);
			ResultSymbol = DecodedSymbol.Symbol;

			// if it not end of stream
			if (ResultSymbol != EscapeSymbol)
			{
				Decoder.updateDecodeRange(DecodedSymbol.Prob);
				Assert(SeqLookAt);
			}
		}

		if (update(ResultSymbol))
		{
			updateOrderSeq(ResultSymbol);
		}

		clearExclusion();

		return ResultSymbol;
	}

	void encodeEndOfStream(ArithEncoder& Encoder)
	{
		u32 OrderIndex = 0;
		while (OrderIndex <= CurrSetOrderCount)
		{
			find_context_result* Find = ContextStack + OrderIndex++;
			encodeSymbol(Encoder, Find->Context, EscapeSymbol);
			updateExclusionData(Find->Context);
		}

		prob Prob = {};
		getProb(StaticContext, Prob, EscapeSymbol);
		Encoder.encode(Prob);
	}

	void reset()
	{
		SubAlloc.reset();
		initModel();
	}

private:

	decode_symbol_result getSymbolFromFreq(context* Context, u32 DecodeFreq, u32 ExclTotal)
	{
		decode_symbol_result Result = {};

		u32 CumFreq = 0;
		for (; Result.SymbolIndex < Context->SymbolCount; ++Result.SymbolIndex)
		{
			context_data* Data = Context->Data + Result.SymbolIndex;
			u32 ModFreq = Data->Freq & Exclusion->Data[Data->Symbol];
			u32 CheckFreq = CumFreq + ModFreq;

			if (CheckFreq > DecodeFreq) break;
			//Exclusion->Data[Data->Symbol] = context_data_excl::ClearMask;

			CumFreq += ModFreq;
		}

		Result.Prob.lo = CumFreq;
		if (Result.SymbolIndex < Context->SymbolCount)
		{
			context_data* MatchSymbol = Context->Data + Result.SymbolIndex;

			Result.Symbol = MatchSymbol->Symbol;
			Result.Prob.hi = CumFreq + MatchSymbol->Freq;
			Result.Prob.count = ExclTotal;

			MatchSymbol->Freq += 1;
			Context->TotalFreq += 1;

			if (Context->TotalFreq >= FreqMaxValue)
			{
				rescale(Context);
			}
		}
		else
		{
			Result.Prob.count = Result.Prob.hi = ExclTotal;
			Result.Symbol = EscapeSymbol;
		}

		return Result;
	}

	u32 getExcludedTotal(context* Context)
	{
		u32 Result = 0;
		for (u32 i = 0; i < Context->SymbolCount; ++i)
		{
			context_data* Data = Context->Data + i;
			Result += Data->Freq & Exclusion->Data[Data->Symbol];
		}

		return Result;
	}

	b32 decodeSymbol(ArithDecoder& Decoder, context* Context, u32* ResultSymbol)
	{
		b32 Success = false;
		u32 ExclTotal = getExcludedTotal(Context) + Context->EscapeFreq;
		u32 CurrFreq = Decoder.getCurrFreq(ExclTotal);

		decode_symbol_result DecodedSymbol = getSymbolFromFreq(Context, CurrFreq, ExclTotal);
		Decoder.updateDecodeRange(DecodedSymbol.Prob);

		if (DecodedSymbol.Symbol != EscapeSymbol)
		{
			*ResultSymbol = DecodedSymbol.Symbol;
			Success = true;
		}

		return Success;
	}

	void rescale(context* Context)
	{
		Context->TotalFreq = 0;
		for (u32 i = 0; i < Context->SymbolCount; ++i)
		{
			context_data* Data = Context->Data + i;
			u32 NewFreq = (Data->Freq + 1) / 2;
			Data->Freq = NewFreq;
			Context->TotalFreq += NewFreq;
		}
	}

	//NOTE: ad hoc value
	inline u32 getContextDataPreallocCount(context* Context)
	{
		u32 Result = Context->SymbolCount;

		if ((Result * 16) > Context->TotalFreq)
		{
			if (Result < 16) Result += 6;
			else if (Result < 32) Result += 9;
			else if (Result < 64) Result += 12;
			else
			{
				Result += 15;
				Result = Result > 256 ? 256 : Result;
			}
		}

		return Result;
	}

	b32 addSymbol(context* Context, u32 Symbol)
	{
		b32 Result = false;

		u32 PreallocSymbol = getContextDataPreallocCount(Context);
		Context->Data = SubAlloc.realloc(Context->Data, ++Context->SymbolCount, PreallocSymbol);

		if (Context->Data)
		{
			context_data* Data = Context->Data + (Context->SymbolCount - 1);
			Data->Freq = 1;
			Data->Symbol = Symbol;
			Data->Next = nullptr;

			Context->TotalFreq += 1;
			Context->EscapeFreq += 1;

			Result = true;
		}

		return Result;
	}

	b32 initContext(context* Context, u32 Symbol)
	{
		Assert(Context)
		ZeroStruct(*Context);

		b32 Result = false;

		Context->Data = SubAlloc.alloc<context_data>(2);
		if (Context->Data)
		{
			Context->TotalFreq = 1;
			Context->EscapeFreq = 1;
			Context->SymbolCount = 1;

			context_data* Data = Context->Data;
			Data->Symbol = Symbol;
			Data->Freq = 1;
			Data->Next = nullptr;

			Result = true;
		};

		return Result;
	}

	b32 getProb(context* Context, prob& Prob, u32 Symbol)
	{
		b32 Result = false;

		u32 SymbolIndex = 0;
		for (; SymbolIndex < Context->SymbolCount; ++SymbolIndex)
		{
			context_data* Data = Context->Data + SymbolIndex;
			if (Data->Symbol == Symbol) break;
			
			u32 Freq = Data->Freq & Exclusion->Data[Data->Symbol];
			//Exclusion->Data[Data->Symbol] = context_data_excl::ClearMask;
			Prob.lo += Freq;
		}

		if (SymbolIndex < Context->SymbolCount)
		{
			context_data* MatchSymbol = Context->Data + SymbolIndex;
			Prob.hi = Prob.lo + MatchSymbol->Freq;

			u32 CumFreqHi = 0;
			for (u32 i = SymbolIndex; i < Context->SymbolCount; ++i)
			{
				context_data* Data = Context->Data + i;
				u32 Freq = Data->Freq & Exclusion->Data[Data->Symbol];
				CumFreqHi += Freq;
			}

			Prob.count = Prob.lo + CumFreqHi + Context->EscapeFreq;

			MatchSymbol->Freq += 1;
			Context->TotalFreq += 1;

			if (Context->TotalFreq >= FreqMaxValue)
			{
				rescale(Context);
			}

			Result = true;
		}
		else
		{
			Prob.count = Prob.hi = Prob.lo + Context->EscapeFreq;
		}

		return Result;
	}

	symbol_search_result findSymbolIndex(context* Context, u32 Symbol)
	{
		symbol_search_result Result = {};

		for (u32 i = 0; i < Context->SymbolCount; ++i)
		{
			context_data* Data = Context->Data + i;
			if (Data->Symbol == Symbol)
			{
				Result.Index = i;
				Result.Success = true;
				break;
			}
		}

		return Result;
	}

	b32 encodeSymbol(ArithEncoder& Encoder, context* Context, u32 Symbol)
	{
		b32 Success = false;

		Assert(Context->EscapeFreq)
		prob Prob = {};
		Success = getProb(Context, Prob, Symbol);
		Encoder.encode(Prob);

		return Success;
	}

	find_context_result findContext()
	{
		find_context_result Result = {};

		context* CurrContext = ContextZero;

		u32 LookAtOrder = SeqLookAt; // from
		u32 To = CurrSetOrderCount;
		while (LookAtOrder < To)
		{
			u32 SymbolAtContext = ContextSeq[LookAtOrder];

			Assert(SymbolAtContext < 256);
			Assert(CurrContext->Data)

			symbol_search_result Search = findSymbolIndex(CurrContext, SymbolAtContext);			
			if (!Search.Success)
			{
				Result.SymbolMiss = true;
				break;
			};
			
			context_data* Data = CurrContext->Data + Search.Index;
			if (!Data->Next)
			{
				Result.ChainMissIndex = Search.Index;
				break;
			}
		
			CurrContext = Data->Next;
			LookAtOrder++;
		}

		Result.Context = CurrContext;
		Result.Order = LookAtOrder;
		Result.IsNotComplete = To - LookAtOrder;

		return Result;
	}

	b32 update(u32 Symbol)
	{
		b32 Result = true;
		context* Prev = LastUsed;

		u32 ProcessStackIndex = SeqLookAt;
		for (; ProcessStackIndex > 0; --ProcessStackIndex)
		{
			find_context_result* Update = ContextStack + (ProcessStackIndex - 1);
			context* ContextAt = Update->Context;

			if (Update->IsNotComplete)
			{
				u32 SeqAt = Update->Order;
				context_data* BuildContextFrom = 0;

				if (Update->SymbolMiss)
				{
					b32 Success = addSymbol(ContextAt, ContextSeq[SeqAt]);
					if (!Success) break;

					if (ContextAt->TotalFreq >= FreqMaxValue)
					{
						rescale(ContextAt);
					}

					BuildContextFrom = ContextAt->Data + (ContextAt->SymbolCount - 1);
				}
				else
				{
					BuildContextFrom = ContextAt->Data + Update->ChainMissIndex;
				}

				SeqAt += 1;

				u32 To = CurrSetOrderCount;
				while (SeqAt < To)
				{
					context* Next = SubAlloc.alloc<context>(1);
					if (!Next) break;

					ContextCount++;

					ContextAt = BuildContextFrom->Next = Next;

					b32 Success = initContext(ContextAt, ContextSeq[SeqAt]);
					if (!Success) break;

					BuildContextFrom = ContextAt->Data;
					SeqAt++;
				}

				if (SeqAt != To) break;

				context* EndSeqContext = SubAlloc.alloc<context>(1);
				if (!EndSeqContext) break;

				ContextCount++;

				ContextAt = BuildContextFrom->Next = EndSeqContext;

				b32 Success = initContext(ContextAt, Symbol);
				if (!Success) break;
			}
			else
			{
				Assert(ContextAt->Data);

				b32 Success = addSymbol(ContextAt, Symbol);
				if (!Success) break;

				if (ContextAt->TotalFreq >= FreqMaxValue)
				{
					rescale(ContextAt);
				}

			}

			ContextAt->Prev = Prev;
			Prev = ContextAt;
		}

		if (ProcessStackIndex)
		{
			// memory not enough, restart model
			//printf("ctx: %d\n", ContextCount);
			reset();
			Result = false;
		}

		return Result;
	}


	inline void updateOrderSeq(u32 Symbol)
	{
		u32 UpdateSeqIndex = CurrSetOrderCount;

		if (CurrSetOrderCount == OrderCount)
		{
			UpdateSeqIndex--;

			for (u32 i = 0; i < (OrderCount - 1); ++i)
			{
				ContextSeq[i] = ContextSeq[i + 1];
			}
		}
		else
		{
			CurrSetOrderCount++;
		}

		ContextSeq[UpdateSeqIndex] = Symbol;
	}

	inline void clearExclusion()
	{
		MemSet<u16>(reinterpret_cast<u16*>(Exclusion), sizeof(context_data_excl) / sizeof(Exclusion->Data[0]), context_data_excl::Mask);
	}

	void updateExclusionData(context* Context)
	{
		context_data* ContextData = Context->Data;
		for (u32 i = 0; i < Context->SymbolCount; ++i)
		{
			context_data* Data = ContextData + i;
			Exclusion->Data[Data->Symbol] = 0;
		}
	}

	void initModel()
	{
		CurrSetOrderCount = ContextCount = 0;

		StaticContext = SubAlloc.alloc<context>(1);
		ZeroStruct(*StaticContext);

		StaticContext->Data = SubAlloc.alloc<context_data>(256);
		StaticContext->EscapeFreq = 1;
		StaticContext->TotalFreq = 256;
		StaticContext->SymbolCount = 256;

		for (u32 i = 0; i < StaticContext->SymbolCount; ++i)
		{
			StaticContext->Data[i].Freq = 1;
			StaticContext->Data[i].Symbol = i;
			StaticContext->Data[i].Next = nullptr;
		}

		Exclusion = SubAlloc.alloc<context_data_excl>(1);
		clearExclusion();

		ContextCount++;

		ContextZero = SubAlloc.alloc<context>(1);
		ZeroStruct(*ContextZero);

		initContext(ContextZero, 0);
		ContextZero->Prev = StaticContext;

		// last encoded symbols
		ContextSeq = SubAlloc.alloc<u32>(OrderCount);

		// max symbol seq + context for that seq
		ContextStack = SubAlloc.alloc<find_context_result>(OrderCount + 1);

		ContextCount++;
	}
};