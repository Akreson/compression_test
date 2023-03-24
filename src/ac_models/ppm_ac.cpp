#include "ppm_ac.h"

class PPMByte
{
	context_data_excl* Exclusion;
	context* MaxContext;
	context* MinContext;
	context_data** ContextStack;
	context_data* LastEncSym;

	u16 OrderCount;
	u16 LastMaskedCount;

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
		prob Prob = {};
		b32 Success = getEncodeProb(Prob, Symbol);

		calcEncBits(Prob, Success);
		Encoder.encode(Prob);

		while (!Success)
		{
			do
			{
				MinContext = MinContext->Prev;
			} while (MinContext && (MinContext->SymbolCount == LastMaskedCount));

			if (!MinContext) break;

			Success = getEncodeProb(Prob, Symbol);

			calcEncBits(Prob, Success);
			Encoder.encode(Prob);
		}

		if (MinContext == MaxContext)
		{
			MinContext = MaxContext = LastEncSym->Next;
		}
		else if (MinContext)
		{
			update();
		}

		clearExclusion();
	}

	u32 decode(ArithDecoder& Decoder)
	{
		prob Prob = {};
		decode_symbol_result DecSym;

		DecSym = getSymbolFromFreq(Decoder);
		Decoder.updateDecodeRange(DecSym.Prob);

		while (DecSym.Symbol == EscapeSymbol)
		{
			do
			{
				MinContext = MinContext->Prev;
			} while (MinContext && (MinContext->SymbolCount == LastMaskedCount));

			if (!MinContext) break;

			DecSym = getSymbolFromFreq(Decoder);
			Decoder.updateDecodeRange(DecSym.Prob);
		}

		if (MinContext == MaxContext)
		{
			MinContext = MaxContext = LastEncSym->Next;
		}
		else if (MinContext)
		{
			update();
		}

		clearExclusion();
		return DecSym.Symbol;
	}

	void encodeEndOfStream(ArithEncoder& Encoder)
	{
		encode(Encoder, PPMByte::EscapeSymbol);

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

	decode_symbol_result getSymbolFromFreq(ArithDecoder& Decoder)
	{
		decode_symbol_result Result = {};

		Result.Prob.scale = getExcludedTotal(MinContext) + MinContext->EscapeFreq;
		u32 DecodeFreq = Decoder.getCurrFreq(Result.Prob.scale);

		u32 CumFreq = 0;
		u32 SymbolIndex = 0;
		for (; SymbolIndex < MinContext->SymbolCount; ++SymbolIndex)
		{
			context_data* Data = MinContext->Data + SymbolIndex;
			u32 ModFreq = Data->Freq & Exclusion->Data[Data->Symbol];
			CumFreq += ModFreq;

			if (CumFreq > DecodeFreq) break;
		}

		if (SymbolIndex < MinContext->SymbolCount)
		{
			context_data* MatchSymbol = MinContext->Data + SymbolIndex;
			LastEncSym = MatchSymbol;
			
			Result.Prob.hi = CumFreq;
			Result.Prob.lo = CumFreq - MatchSymbol->Freq;
			Result.Symbol = MatchSymbol->Symbol;

			MatchSymbol->Freq += 1;
			MinContext->TotalFreq += 1;

			if (MinContext->TotalFreq >= FREQ_MAX_VALUE)
			{
				rescale(MinContext);
			}
		}
		else
		{
			Result.Prob.hi = Result.Prob.scale;
			Result.Prob.lo = Result.Prob.scale - MinContext->EscapeFreq;
			Result.Symbol = EscapeSymbol;
			LastMaskedCount = MinContext->SymbolCount;
			updateExclusionData(MinContext);
		}

		return Result;
	}

	b32 getEncodeProb(prob& Prob, u32 Symbol)
	{
		b32 Result = false;

		u32 CumFreqLo = 0;
		u32 SymbolIndex = 0;
		for (; SymbolIndex < MinContext->SymbolCount; ++SymbolIndex)
		{
			context_data* Data = MinContext->Data + SymbolIndex;
			if (Data->Symbol == Symbol) break;

			CumFreqLo += Data->Freq & Exclusion->Data[Data->Symbol];
		}

		Prob.lo = CumFreqLo;
		if (SymbolIndex < MinContext->SymbolCount)
		{
			context_data* MatchSymbol = MinContext->Data + SymbolIndex;
			LastEncSym = MatchSymbol;

			Prob.hi = Prob.lo + MatchSymbol->Freq;

			u32 CumFreqHi = Prob.hi;
			for (u32 i = SymbolIndex + 1; i < MinContext->SymbolCount; ++i)
			{
				context_data* Data = MinContext->Data + i;
				u32 Freq = Data->Freq & Exclusion->Data[Data->Symbol];
				CumFreqHi += Freq;
			}

			Prob.scale = CumFreqHi + MinContext->EscapeFreq;

			MatchSymbol->Freq += 1;
			MinContext->TotalFreq += 1;

			if (MinContext->TotalFreq >= FREQ_MAX_VALUE)
			{
				rescale(MinContext);
			}

			Result = true;
		}
		else
		{
			Prob.hi = Prob.scale = Prob.lo + MinContext->EscapeFreq;
			LastMaskedCount = MinContext->SymbolCount;
			updateExclusionData(MinContext);
		}

		return Result;
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

	context_data* allocSymbol(context* Context)
	{
		context_data* Result = nullptr;

		u32 PreallocSymbol = getContextDataPreallocCount(Context);
		Context->Data = SubAlloc.realloc(Context->Data, ++Context->SymbolCount, PreallocSymbol);

		if (Context->Data)
		{
			Result = Context->Data + (Context->SymbolCount - 1);
			ZeroStruct(*Result);
		}

		return Result;
	}

	context* allocContext(context_data* From, context* Prev)
	{
		context* New = nullptr;
		New = SubAlloc.alloc<context>(1);
		if (New)
		{
			New->Data = SubAlloc.alloc<context_data>(2);
			if (New->Data)
			{
				From->Next = New;
				New->Prev = Prev;
				New->TotalFreq = 1;
				New->EscapeFreq = 1;
				New->SymbolCount = 0;
			}
			else
			{
				New = nullptr;
			}
		}

		return New;
	}

	void update()
	{
		context_data** StackPtr = ContextStack;
		context* ContextAt = MaxContext;

		u16 InitFreq = 1;

		if (ContextAt->SymbolCount == 0)
		{
			do
			{
				context_data* First = ContextAt->Data;
				First->Symbol = LastEncSym->Symbol;
				First->Freq = InitFreq;

				ContextAt->SymbolCount = 1;
				ContextAt = ContextAt->Prev;
				*StackPtr++ = First;
			} while (ContextAt->SymbolCount == 0);
		}

		context_data* NewSym;
		for (; ContextAt != MinContext; ContextAt = ContextAt->Prev, *StackPtr++ = NewSym)
		{
			NewSym = allocSymbol(ContextAt);
			if (!NewSym)
			{
				ContextAt = nullptr;
				break;
			}

			NewSym->Freq = InitFreq;
			NewSym->Symbol = LastEncSym->Symbol;
			ContextAt->EscapeFreq += 1;
			ContextAt->TotalFreq += 1;
		}

		if (ContextAt)
		{
			if (LastEncSym->Next)
			{
				ContextAt = MinContext = LastEncSym->Next;
			}
			else
			{
				ContextAt = allocContext(LastEncSym, ContextAt);
			}
		}

		if (ContextAt)
		{
			while (--StackPtr != ContextStack)
			{
				ContextAt = allocContext(*StackPtr, ContextAt);
				if (!ContextAt) break;
			}

			MaxContext = (*StackPtr)->Next = ContextAt;
		}

		if (!ContextAt) reset();
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
		Exclusion = SubAlloc.alloc<context_data_excl>(1);
		clearExclusion();

		ContextStack = SubAlloc.alloc<context_data*>(OrderCount);

		context* Order0 = SubAlloc.alloc<context>(1);
		Assert(Order0);
		ZeroStruct(*Order0);

		MaxContext = Order0;
		Order0->TotalFreq = 257;
		Order0->SymbolCount = 256;
		Order0->EscapeFreq = 1;
		Order0->Data = SubAlloc.alloc<context_data>(256);

		for (u32 i = 0; i < Order0->SymbolCount; ++i)
		{
			Order0->Data[i].Freq = 1;
			Order0->Data[i].Symbol = i;
			Order0->Data[i].Next = nullptr;
		}

		for (u32 OrderIndex = 1;; OrderIndex++)
		{
			MaxContext = allocContext(MaxContext->Data, MaxContext);
			if (OrderIndex == OrderCount) break;

			MaxContext->SymbolCount = 1;
			MaxContext->Data[0].Freq = 1;
			MaxContext->Data[0].Symbol = 0;
		}

		MinContext = Order0;
	}
};