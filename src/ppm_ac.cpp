#include "ppm_ac.h"
#include "ppm_see.cpp"

class PPMByte
{
	SEEState* SEE;

	context_data_excl* Exclusion;
	context* StaticContext; // order -1
	context* ContextZero;
	context* LastUsed;

	u32* ContextSeq;
	find_context_result* ContextStack;
	see_context* LastSEECtx;
	
	u32 OrderCount;
	u32 CurrSetOrderCount;
	u32 SeqLookAt;
	u32 LastMaskedCount;

public:
	StaticSubAlloc SubAlloc;
	static constexpr u32 EscapeSymbol = context::MaxSymbol + 1;

	// For debug
	u64 ContextCount;

	PPMByte() = delete;
	PPMByte(u32 MaxOrderContext, u32 MemLimit) :
		SubAlloc(MemLimit, sizeof(context_data)*2), SEE(nullptr), OrderCount(MaxOrderContext)
	{
		initModel();
	}

	~PPMByte() {delete SEE;}

	void encode(ArithEncoder& Encoder, u32 Symbol)
	{
		LastMaskedCount = SeqLookAt = 0;
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
				findContext(Find);
			}

			if (!Find.IsNotComplete)
			{
				Assert(Find.Context->TotalFreq);

				b32 Success = encodeSymbol(Encoder, Find.Context, Symbol);
				if (Success)
				{
					Assert(Find.Context->TotalFreq);
					LastUsed = Find.Context;
					break;
				}

				Find.SymbolMiss = true;
				Prev = Find.Context->Prev;
				LastMaskedCount = Find.Context->SymbolCount;
				updateExclusionData(Find.Context);
			}

			ContextStack[SeqLookAt++] = Find;
			OrderLooksLeft--;
		}

		if (!OrderLooksLeft)
		{
			LastUsed = StaticContext;
			
			prob Prob = {};
			b32 Success = getEncodeProb(StaticContext, Prob, Symbol);

			Assert(Success);
			Assert(SeqLookAt);

			Encoder.encode(Prob);
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
		
		LastMaskedCount = SeqLookAt = 0;
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
				findContext(Find);
			}

			if (!Find.IsNotComplete)
			{
				Assert(Find.Context->TotalFreq)

				b32 Success = decodeSymbol(Decoder, Find.Context, &ResultSymbol);
				if (Success)
				{
					Assert(Find.Context->TotalFreq);
					LastUsed = Find.Context;
					break;
				}

				Find.SymbolMiss = true;
				Prev = Find.Context->Prev;
				LastMaskedCount = Find.Context->SymbolCount;
				updateExclusionData(Find.Context);
			}

			ContextStack[SeqLookAt++] = Find;
			OrderLooksLeft--;
		}

		if (!OrderLooksLeft)
		{
			LastUsed = StaticContext;
			decode_symbol_result DecodedSymbol = getSymbolFromFreq(Decoder, StaticContext);
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
		u32 OrderIndex = LastMaskedCount = SeqLookAt = 0;

		context* Prev = 0;
		context* EscContext = 0;
		u32 OrderLooksLeft = CurrSetOrderCount + 1;
		while (OrderLooksLeft)
		{
			find_context_result Find = findContext();

			if (!Find.IsNotComplete)
			{
				EscContext = Find.Context;
				break;
			}

			SeqLookAt++;
			OrderLooksLeft--;
		}

		while (EscContext->Prev)
		{
			encodeSymbol(Encoder, EscContext, EscapeSymbol);
			updateExclusionData(EscContext);
			EscContext = EscContext->Prev;
		}

		prob Prob = {};
		getEncodeProb(StaticContext, Prob, EscapeSymbol);
		Encoder.encode(Prob);
	}

	void reset()
	{
		SubAlloc.reset();
		initModel();
	}

private:

	void rescale(context* Context)
	{
		Context->TotalFreq = Context->BinExcVal;
		for (u32 i = 0; i < Context->SymbolCount; ++i)
		{
			context_data* Data = Context->Data + i;
			u32 NewFreq = (Data->Freq + 1) / 2;
			Data->Freq = NewFreq;
			Context->TotalFreq += NewFreq;
		}
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

	inline void swapContextData(context_data* A, context_data* B)
	{
		context_data Tmp = *A;
		*A = *B;
		*B = Tmp;
	}

	decode_symbol_result getSymbolFromFreq(ArithDecoder& Decoder, context* Context)
	{
		decode_symbol_result Result = {};

		u32 MaskedDiff = Context->SymbolCount - LastMaskedCount;
		LastSEECtx = SEE->getContext(Context, MaskedDiff, LastMaskedCount);
		Result.Prob.scale = SEE->getMean(LastSEECtx);

		Result.Prob.scale += getExcludedTotal(Context);
		u32 DecodeFreq = Decoder.getCurrFreq(Result.Prob.scale);

		u32 CumFreq = 0;
		u32 SymbolIndex = 0;
		for (; SymbolIndex < Context->SymbolCount; ++SymbolIndex)
		{
			context_data* Data = Context->Data + SymbolIndex;
			u32 ModFreq = Data->Freq & Exclusion->Data[Data->Symbol];
			u32 CheckFreq = CumFreq + ModFreq;

			if (CheckFreq > DecodeFreq) break;
			CumFreq = CheckFreq;
		}

		Result.Prob.lo = CumFreq;
		if (SymbolIndex < Context->SymbolCount)
		{
			context_data* MatchSymbol = Context->Data + SymbolIndex;
			Result.Prob.hi = CumFreq + MatchSymbol->Freq;
			Result.Symbol = MatchSymbol->Symbol;

			MatchSymbol->Freq += 4;
			Context->TotalFreq += 4;

			if (MatchSymbol->Freq > MaxFreq)
			{
				rescale(Context);
			}
		}
		else
		{
			Result.Prob.hi = Result.Prob.scale;
			LastSEECtx->Summ += Result.Prob.scale;
			Result.Symbol = EscapeSymbol;
		}

		return Result;
	}

	decode_symbol_result getSymbolFromFreqLeaf(context* Context, u32 DecodeFreq)
	{
		decode_symbol_result Result = {};

		Result.Prob.scale = Context->TotalFreq;
		context_data* First = Context->Data;
		if (First->Freq > DecodeFreq)
		{
			SEE->PrevSuccess = ((First->Freq * 2) > Context->TotalFreq) ? 1 : 0;

			Result.Prob.lo = 0;
			Result.Prob.hi = First->Freq;
			Result.Symbol = First->Symbol;

			Context->TotalFreq += 4;
			First->Freq += 4;

			if (First->Freq > MaxFreq)
			{
				rescale(Context);
			}
		}
		else
		{
			SEE->PrevSuccess = 0;
			Result.Prob.lo = First->Freq;

			u32 SymbolIndex = 1;
			u32 CumFreq = Result.Prob.lo;
			context_data* MatchSymbol = 0;
			for (; SymbolIndex < Context->SymbolCount; ++SymbolIndex)
			{
				MatchSymbol = Context->Data + SymbolIndex;
				CumFreq += MatchSymbol->Freq;

				if (CumFreq > DecodeFreq) break;
			}

			if (SymbolIndex < Context->SymbolCount)
			{
				Result.Prob.hi = CumFreq;
				Result.Prob.lo = CumFreq - MatchSymbol->Freq;
				Result.Symbol = MatchSymbol->Symbol;

				Context->TotalFreq += 4;
				MatchSymbol->Freq += 4;

				context_data* PrevSymbol = MatchSymbol - 1;
				if (MatchSymbol->Freq > PrevSymbol->Freq)
				{
					swapContextData(MatchSymbol, PrevSymbol);
					MatchSymbol = PrevSymbol;
				}

				if (MatchSymbol->Freq > MaxFreq)
				{
					rescale(Context);
				}
			}
			else
			{
				Result.Prob.lo = CumFreq;
				Result.Prob.hi = Result.Prob.scale;
				Result.Symbol = EscapeSymbol;
			}
		}

		return Result;
	}

	decode_symbol_result getSymbolFromFreqBin(context* Context, u32 DecodeFreq)
	{
		decode_symbol_result Result = {};
		Result.Prob.scale = FreqMaxValue;
		see_bin_context* BinCtx = SEE->getBinContext(Context);

		context_data* First = Context->Data;	
		if (DecodeFreq < BinCtx->Scale)
		{
			Result.Prob.hi = BinCtx->Scale;
			First->Freq += (First->Freq < 128) ? 1 : 0;
			BinCtx->Scale += Interval - SEE->getBinMean(BinCtx->Scale);
			SEE->PrevSuccess = 1;
			Result.Symbol = First->Symbol;
		}
		else
		{
			Result.Prob.lo = BinCtx->Scale;
			Result.Prob.hi = FreqMaxValue;
			BinCtx->Scale -= SEE->getBinMean(BinCtx->Scale);
			
			// TODO: move to update
			u8 EscVal = ExpEscape[BinCtx->Scale >> 10];
			Context->TotalFreq += EscVal + First->Freq;
			Context->BinExcVal = EscVal;

			SEE->PrevSuccess = 0;
			Result.Symbol = EscapeSymbol;
		}

		return Result;
	}

	b32 decodeSymbol(ArithDecoder& Decoder, context* Context, u32* ResultSymbol)
	{
		b32 Success = false;
		decode_symbol_result DecodedSymbol;

		if (!SeqLookAt)
		{
			if (Context->SymbolCount == 1)
			{
				u32 CurrFreq = Decoder.getCurrFreq(FreqMaxValue);
				DecodedSymbol = getSymbolFromFreqBin(Context, CurrFreq);
			}
			else
			{
				u32 CurrFreq = Decoder.getCurrFreq(Context->TotalFreq);
				DecodedSymbol = getSymbolFromFreqLeaf(Context, CurrFreq);
			}
		}
		else
		{
			// TODO: remove
			if (Context->SymbolCount == 1)
			{
				u32 CurrFreq = Decoder.getCurrFreq(FreqMaxValue);
				DecodedSymbol = getSymbolFromFreqBin(Context, CurrFreq);
			}
			else
			{
				DecodedSymbol = getSymbolFromFreq(Decoder, Context);
			}
		}

		Decoder.updateDecodeRange(DecodedSymbol.Prob);

		if (DecodedSymbol.Symbol != EscapeSymbol)
		{
			*ResultSymbol = DecodedSymbol.Symbol;
			Success = true;
		}

		return Success;
	}

	b32 getEncodeProb(context* Context, prob& Prob, u32 Symbol)
	{
		b32 Result = false;

		u32 MaskedDiff = Context->SymbolCount - LastMaskedCount;
		LastSEECtx = SEE->getContext(Context, MaskedDiff, LastMaskedCount);
		Prob.scale = SEE->getMean(LastSEECtx);

		u32 CumFreq = 0;
		u32 SymbolIndex = 0;
		for (; SymbolIndex < Context->SymbolCount; ++SymbolIndex)
		{
			context_data* Data = Context->Data + SymbolIndex;
			if (Data->Symbol == Symbol) break;
			
			u32 Freq = Data->Freq & Exclusion->Data[Data->Symbol];
			CumFreq += Freq;
		}

		Prob.lo = CumFreq;
		if (SymbolIndex < Context->SymbolCount)
		{
			context_data* MatchSymbol = Context->Data + SymbolIndex;
			Prob.hi = Prob.lo + MatchSymbol->Freq;

			u32 CumFreqHi = Prob.lo;
			for (u32 i = SymbolIndex; i < Context->SymbolCount; ++i)
			{
				context_data* Data = Context->Data + i;
				u32 Freq = Data->Freq & Exclusion->Data[Data->Symbol];
				CumFreqHi += Freq;
			}

			Prob.scale += CumFreqHi;
			
			MatchSymbol->Freq += 4;
			Context->TotalFreq += 4;

			if (MatchSymbol->Freq > MaxFreq)
			{
				rescale(Context);
			}

			Result = true;
		}
		else
		{
			Prob.scale += Prob.lo;
			Prob.hi = Prob.scale;
			LastSEECtx->Summ += Prob.scale;
		}

		return Result;
	}

	b32 getEncodeProbLeaf(context* Context, prob& Prob, u32 Symbol)
	{
		b32 Result = false;

		Prob.scale = Context->TotalFreq;
		context_data* First = Context->Data;
		if (First->Symbol == Symbol)
		{
			SEE->PrevSuccess = ((First->Freq * 2) > Context->TotalFreq) ? 1 : 0;

			Prob.lo = 0;
			Prob.hi = First->Freq;
			Context->TotalFreq += 4;
			First->Freq += 4;

			if (First->Freq > MaxFreq)
			{
				rescale(Context);
			}

			Result = true;
		}
		else
		{
			SEE->PrevSuccess = 0;
			Prob.lo = First->Freq;

			u32 SymbolIndex = 1;
			context_data* MatchSymbol = 0;
			for (; SymbolIndex < Context->SymbolCount; ++SymbolIndex)
			{
				MatchSymbol = Context->Data + SymbolIndex;
				if (MatchSymbol->Symbol == Symbol) break;

				Prob.lo += MatchSymbol->Freq;
			}

			if (SymbolIndex < Context->SymbolCount)
			{
				Prob.hi = Prob.lo + MatchSymbol->Freq;

				Context->TotalFreq += 4;
				MatchSymbol->Freq += 4;

				context_data* PrevSymbol = MatchSymbol - 1;
				if (MatchSymbol->Freq > PrevSymbol->Freq)
				{
					swapContextData(MatchSymbol, PrevSymbol);
					MatchSymbol = PrevSymbol;
				}

				if (MatchSymbol->Freq > MaxFreq)
				{
					rescale(Context);
				}

				Result = true;
			}
			else
			{
				Prob.hi = Prob.scale;
			}
		}

		return Result;
	}

	b32 getEncodeProbBin(context* Context, prob& Prob, u32 Symbol)
	{
		b32 Success = false;
		Prob.scale = FreqMaxValue;
		see_bin_context* BinCtx = SEE->getBinContext(Context);

		context_data* First = Context->Data;
		if (First->Symbol == Symbol)
		{
			Prob.hi = BinCtx->Scale;
			First->Freq += (First->Freq < 128) ? 1 : 0;
			BinCtx->Scale += Interval - SEE->getBinMean(BinCtx->Scale);
			SEE->PrevSuccess = 1;
			Success = true;
		}
		else
		{
			Prob.lo = BinCtx->Scale;
			Prob.hi = FreqMaxValue;
			BinCtx->Scale -= SEE->getBinMean(BinCtx->Scale);

			// TODO: Move to update
			u8 EscVal = ExpEscape[BinCtx->Scale >> 10];
			Context->TotalFreq += EscVal + First->Freq;
			Context->BinExcVal = EscVal;

			SEE->PrevSuccess = 0;
		}

		return Success;
	}

	b32 encodeSymbol(ArithEncoder& Encoder, context* Context, u32 Symbol)
	{
		b32 Success = false;
		prob Prob = {};

		if (!SeqLookAt)
		{
			if (Context->SymbolCount == 1)
			{
				Success = getEncodeProbBin(Context, Prob, Symbol);
			}
			else
			{
				Success = getEncodeProbLeaf(Context, Prob, Symbol);
			}
		}
		else
		{
			// TODO: remove?
			if (Context->SymbolCount == 1)
			{
				Success = getEncodeProbBin(Context, Prob, Symbol);
			}
			else
			{
				Success = getEncodeProb(Context, Prob, Symbol);
			}
		}

		Encoder.encode(Prob);
		return Success;
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

	inline find_context_result findContext(void)
	{
		find_context_result Find = {};
		findContext(Find);
		return Find;
	}

	void findContext(find_context_result& Result)
	{
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

			Result = true;
		}

		return Result;
	}

	b32 initContext(context* Context, u32 Symbol)
	{
		Assert(Context);
		ZeroStruct(*Context);

		b32 Result = false;

		Context->Data = SubAlloc.alloc<context_data>(2);
		if (Context->Data)
		{
			Context->TotalFreq = 1;
			Context->SymbolCount = 1;

			context_data* Data = Context->Data;
			Data->Freq = 1;
			Data->Symbol = Symbol;
			Data->Next = nullptr;

			Result = true;
		};

		return Result;
	}

	b32 update(u32 Symbol)
	{
		b32 Result = true;
		context* Prev = LastUsed;

		if (LastSEECtx)
		{
			SEE->updateContext(LastSEECtx);
			LastSEECtx = nullptr;
		}

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
		MemSet<u8>(reinterpret_cast<u8*>(Exclusion), sizeof(context_data_excl) / sizeof(Exclusion->Data[0]), context_data_excl::Mask);
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

	void initSEE()
	{
		if (SEE == nullptr)
		{
			SEE = new SEEState;
		}

		SEE->init();
	}

	void initModel()
	{
		LastSEECtx = nullptr;
		CurrSetOrderCount = ContextCount = 0;

		StaticContext = SubAlloc.alloc<context>(1);
		ZeroStruct(*StaticContext);

		StaticContext->Data = SubAlloc.alloc<context_data>(256);
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

		initSEE();
	}
};