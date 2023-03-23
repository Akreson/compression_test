#include "ppm_ac.h"

class PPMByte
{
	context_data_excl* Exclusion;
	context* StaticContext; // order -1
	context* Order0;
	context* LastUsed;

	u32* ContextSeq;
	find_context_result* ContextStack;

	u32 OrderCount;
	u32 CurrMaxOrder;
	u32 SeqLookAt;

public:
	StaticSubAlloc<32> SubAlloc;
	static constexpr u32 EscapeSymbol = context::MaxSymbol + 1;

	f64 SymEnc;
	f64 EscEnc;

	PPMByte() = delete;
	PPMByte(u32 MaxOrderContext, u32 MemLimit = 0) :
		SubAlloc(MemLimit), OrderCount(MaxOrderContext)
	{
		initModel();

		SymEnc = 0.0;
		EscEnc = 0.0;
	}

	~PPMByte() {}

	void encode(ArithEncoder& Encoder, u32 Symbol)
	{
		SeqLookAt = 0;
		u32 OrderLooksLeft = CurrMaxOrder + 1;

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

				Prev = Find.Context->Prev;
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
			calcEncBits(Prob, Success);
		}

		update(Symbol);
		updateOrderSeq(Symbol);

		clearExclusion();
	}

	u32 decode(ArithDecoder& Decoder)
	{
		u32 ResultSymbol;

		SeqLookAt = 0;
		u32 OrderLooksLeft = CurrMaxOrder + 1;

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

				Prev = Find.Context->Prev;
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

		update(ResultSymbol);
		updateOrderSeq(ResultSymbol);

		clearExclusion();
		return ResultSymbol;
	}

	void encodeEndOfStream(ArithEncoder& Encoder)
	{
		u32 OrderIndex = SeqLookAt = 0;

		context* EscContext = 0;
		u32 OrderLooksLeft = CurrMaxOrder + 1;
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
			Assert(EscContext->Prev);
			EscContext = EscContext->Prev;
		}

		prob Prob = {};
		getEncodeProb(StaticContext, Prob, EscapeSymbol);
		Encoder.encode(Prob);

#ifdef _DEBUG
		SymEnc = SymEnc / 8.0;
		EscEnc = EscEnc / 8.0;
#endif
	}

	void reset()
	{
		SubAlloc.reset();
		initModel();
	}

private:

	inline void calcEncBits(prob Prob, b32 Success)
	{
#ifdef _DEBUG
		f64 diff = (f64)Prob.hi - (f64)Prob.lo;
		f64 p = diff / (f64)Prob.scale;
		if (Success)
		{
			SymEnc += -std::log2(p);
		}
		else
		{
			EscEnc += -std::log2(p);
		}
#endif
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

// DECODE STYLE
#if 1
	decode_symbol_result getSymbolFromFreq(ArithDecoder& Decoder, context* Context)
	{
		decode_symbol_result Result = {};

		Result.Prob.scale = getExcludedTotal(Context) + Context->EscapeFreq;
		u32 DecodeFreq = Decoder.getCurrFreq(Result.Prob.scale);

		u32 CumFreq = 0;
		u32 SymbolIndex = 0;
		for (; SymbolIndex < Context->SymbolCount; ++SymbolIndex)
		{
			context_data* Data = Context->Data + SymbolIndex;
			u32 ModFreq = Data->Freq & Exclusion->Data[Data->Symbol];
			CumFreq += ModFreq;

			if (CumFreq > DecodeFreq) break;
		}

		if (SymbolIndex < Context->SymbolCount)
		{
			context_data* MatchSymbol = Context->Data + SymbolIndex;
			
			Result.Prob.hi = CumFreq;
			Result.Prob.lo = CumFreq - MatchSymbol->Freq;
			Result.Symbol = MatchSymbol->Symbol;

			MatchSymbol->Freq += 1;
			Context->TotalFreq += 1;

			if (Context->TotalFreq >= FREQ_MAX_VALUE)
			{
				rescale(Context);
			}
		}
		else
		{
			Result.Prob.hi = Result.Prob.scale;
			Result.Prob.lo = Result.Prob.scale - Context->EscapeFreq;
			Result.Symbol = EscapeSymbol;
		}

		return Result;
	}
#else // DECODE STYLE
	decode_symbol_result getSymbolFromFreq(ArithDecoder& Decoder, context* Context)
	{
		decode_symbol_result Result = {};

		context_data* MaskArr[256];
		context_data** MaskPtr = MaskArr;
		context_data* LastPastOne = Context->Data + Context->SymbolCount;
		context_data* DataPtr = Context->Data;

		u32 MaskedTotal = 0;
		for (; DataPtr != LastPastOne; DataPtr++)
		{
			if (Exclusion->Data[DataPtr->Symbol])
			{
				MaskedTotal += DataPtr->Freq;
				*MaskPtr++ = DataPtr;
			}
		}

		Result.Prob.scale = MaskedTotal + Context->EscapeFreq;
		u32 DecodeFreq = Decoder.getCurrFreq(Result.Prob.scale);
		
		u32 CumFreq = 0;
		context_data* MatchSymbol = 0;
		u32 MaskedCount = MaskPtr - MaskArr;
		if (MaskedCount)
		{
			MaskPtr = MaskArr;
			DataPtr = *MaskArr;

			do
			{
				CumFreq += DataPtr->Freq;
				if (CumFreq > DecodeFreq)
				{
					MatchSymbol = DataPtr;
					break;
				};

				DataPtr = *++MaskPtr;
			} while (--MaskedCount);
		}

		if (MatchSymbol)
		{
			Result.Prob.hi = CumFreq;
			Result.Prob.lo = CumFreq - MatchSymbol->Freq;
			Result.Symbol = MatchSymbol->Symbol;

			MatchSymbol->Freq += 1;
			Context->TotalFreq += 1;

			if (Context->TotalFreq >= FREQ_MAX_VALUE)
			{
				rescale(Context);
			}
		}
		else
		{
			Result.Prob.hi = Result.Prob.scale;
			Result.Prob.lo = Result.Prob.scale - Context->EscapeFreq;
			Result.Symbol = EscapeSymbol;
		}

		return Result;
	}
#endif // DECODE STYLE

	b32 decodeSymbol(ArithDecoder& Decoder, context* Context, u32* ResultSymbol)
	{
		b32 Success = false;
		decode_symbol_result Decoded = getSymbolFromFreq(Decoder, Context);
		Decoder.updateDecodeRange(Decoded.Prob);

		if (Decoded.Symbol != EscapeSymbol)
		{
			Success = true;
			*ResultSymbol = Decoded.Symbol;
		}

		calcEncBits(Decoded.Prob, Success);

		return Success;
	}

	b32 getEncodeProb(context* Context, prob& Prob, u32 Symbol)
	{
		b32 Result = false;

		u32 CumFreqLo = 0;
		u32 SymbolIndex = 0;
		for (; SymbolIndex < Context->SymbolCount; ++SymbolIndex)
		{
			context_data* Data = Context->Data + SymbolIndex;
			if (Data->Symbol == Symbol) break;

			CumFreqLo += Data->Freq & Exclusion->Data[Data->Symbol];
		}

		Prob.lo = CumFreqLo;
		if (SymbolIndex < Context->SymbolCount)
		{
			context_data* MatchSymbol = Context->Data + SymbolIndex;
			Prob.hi = Prob.lo + MatchSymbol->Freq;

			u32 CumFreqHi = Prob.hi;
			for (u32 i = SymbolIndex + 1; i < Context->SymbolCount; ++i)
			{
				context_data* Data = Context->Data + i;
				u32 Freq = Data->Freq & Exclusion->Data[Data->Symbol];
				CumFreqHi += Freq;
			}

			Prob.scale = CumFreqHi + Context->EscapeFreq;

			MatchSymbol->Freq += 1;
			Context->TotalFreq += 1;

			if (Context->TotalFreq >= FREQ_MAX_VALUE)
			{
				rescale(Context);
			}

			Result = true;
		}
		else
		{
			Prob.hi = Prob.scale = Prob.lo + Context->EscapeFreq;
		}

		return Result;
	}

	b32 encodeSymbol(ArithEncoder& Encoder, context* Context, u32 Symbol)
	{
		Assert(Context->EscapeFreq);

		b32 Success = false;
		prob Prob = {};

		Success = getEncodeProb(Context, Prob, Symbol);
		Encoder.encode(Prob);

		calcEncBits(Prob, Success);

		return Success;
	}

	symbol_search_result findSymbolIndex(context* Context, u32 Symbol)
	{
		symbol_search_result Result = {};

		for (u32 i = 0; i < Context->SymbolCount; ++i)
		{
			if (Context->Data[i].Symbol == Symbol)
			{
				Result.Index = i;
				Result.Success = true;
				break;
			}
		}

		return Result;
	}

	void findContext(find_context_result& Result)
	{
		context* CurrContext = Order0;

		u32 LookAtOrder = SeqLookAt; // from
		while (LookAtOrder < CurrMaxOrder)
		{
			u32 SymbolAtContext = ContextSeq[LookAtOrder];

			Assert(SymbolAtContext < 256);
			Assert(CurrContext->Data)

			symbol_search_result Search = findSymbolIndex(CurrContext, SymbolAtContext);
			if (!Search.Success)
			{
				Result.SymbolMiss = true;
				break;
			}

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
		Result.SeqIndex = LookAtOrder;
		Result.IsNotComplete = CurrMaxOrder - LookAtOrder;
	}

	inline find_context_result findContext(void)
	{
		find_context_result Find = {};
		findContext(Find);
		return Find;
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
		Assert(Context);
		ZeroStruct(*Context);

		b32 Result = false;

		Context->Data = SubAlloc.alloc<context_data>(2);
		if (Context->Data)
		{
			Context->TotalFreq = 1;
			Context->EscapeFreq = 1;
			Context->SymbolCount = 1;

			context_data* Data = Context->Data;
			Data->Freq = 1;
			Data->Symbol = Symbol;
			Data->Next = nullptr;

			Result = true;
		};

		return Result;
	}

	context* allocContext(u32 Symbol)
	{
		context* New = SubAlloc.alloc<context>(1);
		if (New)
		{
			ZeroStruct(*New);
			if (!initContext(New, Symbol))
			{
				New = nullptr;
			}
		}

		return New;
	}

	void update(u32 Symbol)
	{
		context* Prev = LastUsed;

		u32 ProcessStackIndex = SeqLookAt;
		for (; ProcessStackIndex > 0; --ProcessStackIndex)
		{
			find_context_result* Update = ContextStack + (ProcessStackIndex - 1);
			context* ContextAt = Update->Context;

			if (Update->IsNotComplete)
			{
				context_data* BuildContextFrom = 0;

				if (Update->SymbolMiss)
				{
					if (!addSymbol(ContextAt, ContextSeq[Update->SeqIndex])) break;

					BuildContextFrom = ContextAt->Data + (ContextAt->SymbolCount - 1);
				}
				else
				{
					BuildContextFrom = ContextAt->Data + Update->ChainMissIndex;
				}

				u32 SeqAt = Update->SeqIndex + 1;
				while (SeqAt < CurrMaxOrder)
				{
					context* Next = allocContext(ContextSeq[SeqAt]);
					if (!Next) break;

					ContextAt = BuildContextFrom->Next = Next;
					BuildContextFrom = ContextAt->Data;
					SeqAt++;
				}

				if (SeqAt != CurrMaxOrder) break;

				context* EndSeqContext = allocContext(Symbol);
				if (!EndSeqContext) break;

				ContextAt = BuildContextFrom->Next = EndSeqContext;
			}
			else
			{
				Assert(ContextAt->Data);
				if (!addSymbol(ContextAt, Symbol)) break;
			}

			ContextAt->Prev = Prev;
			Prev = ContextAt;
		}

		if (ProcessStackIndex)
		{
			// memory not enough, restart model
			reset();
		}
	}

	inline void updateOrderSeq(u32 Symbol)
	{
		u32 UpdateSeqIndex = CurrMaxOrder;

		if (CurrMaxOrder == OrderCount)
		{
			UpdateSeqIndex--;

			for (u32 i = 0; i < (OrderCount - 1); ++i)
			{
				ContextSeq[i] = ContextSeq[i + 1];
			}
		}
		else
		{
			CurrMaxOrder++;
		}

		ContextSeq[UpdateSeqIndex] = Symbol;
	}

	inline void clearExclusion()
	{
		MemSet(&Exclusion->Data[0], ArrayCount(Exclusion->Data), context_data_excl::Mask);
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
		CurrMaxOrder = 0;

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

		Order0 = allocContext(0);
		Order0->Prev = StaticContext;

		// last encoded symbols
		ContextSeq = SubAlloc.alloc<u32>(OrderCount);

		// max symbol seq + context for that seq
		ContextStack = SubAlloc.alloc<find_context_result>(OrderCount + 1);
	}
};