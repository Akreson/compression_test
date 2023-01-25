//#include "ppm_ac.h"
//#include "ppm_see.cpp"

#include <cmath>

class PPMByte
{
	SEEState* SEE;
	see_context* LastSEECtx;

	context_data_excl* Exclusion;
	context* MaxContext;
	context* MinContext;
	context_data** ContextStack;
	context_data* LastEncSym;
	context* Order0;

	u8 InitEsc;
	u16 OrderCount;
	u16 LastMaskedCount;

public:
	StaticSubAlloc SubAlloc;
	static constexpr u32 EscapeSymbol = context::MaxSymbol + 1;

#ifdef _DEBUG
	f64 SymEnc;
	f64 EscEnc;
#endif

	PPMByte() = delete;
	PPMByte(u32 MaxOrderContext, u32 MemLimit) :
		SubAlloc(MemLimit, sizeof(context_data) * 2), SEE(nullptr), OrderCount(MaxOrderContext)
	{
		initModel();
		SEE = new SEEState;
		SEE->init();

#ifdef _DEBUG
		SymEnc = 0.0;
		EscEnc = 0.0;
#endif
	}

	~PPMByte() { delete SEE; }

	void encode(ArithEncoder& Encoder, u32 Symbol)
	{
		prob Prob;
		b32 Success = false;

		if (MinContext->SymbolCount == 1)
		{
			Success = getEncodeProbBin(MinContext, Prob, Symbol);
		}
		else
		{
			Success = getEncodeProbLeaf(MinContext, Prob, Symbol);
		}

#ifdef _DEBUG
		calcEncBits(Prob, Success);
#endif
		Encoder.encode(Prob);

		while (!Success)
		{
			do
			{
				MinContext = MinContext->Prev;
			} while (MinContext && (MinContext->SymbolCount == LastMaskedCount));

			if (!MinContext) break;

			Success = getEncodeProb(MinContext, Prob, Symbol);
#ifdef _DEBUG
			calcEncBits(Prob, Success);
#endif
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
		prob Prob;
		decode_symbol_result DecSym;

		if (MinContext->SymbolCount == 1)
		{
			u32 DecFreq = Decoder.getCurrFreq(FreqMaxValue);
			DecSym = getSymbolFromFreqBin(MinContext, DecFreq);
		}
		else
		{
			u32 DecFreq = Decoder.getCurrFreq(MinContext->TotalFreq);
			DecSym = getSymbolFromFreqLeaf(MinContext, DecFreq);
		}

		Decoder.updateDecodeRange(DecSym.Prob);

		while (DecSym.Symbol == EscapeSymbol)
		{
			do
			{
				MinContext = MinContext->Prev;
			} while (MinContext && (MinContext->SymbolCount == LastMaskedCount));

			if (!MinContext) break;

			DecSym = getSymbolFromFreq(Decoder, MinContext);
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

	inline void encodeEndOfStream(ArithEncoder& Encoder)
	{
		encode(Encoder, PPMByte::EscapeSymbol);
		SymEnc = SymEnc / 8.0;
		EscEnc = EscEnc / 8.0;
	}

	void reset()
	{
		SubAlloc.reset();
		initModel();
		SEE->init();
	}

private:

	void calcEncBits(prob Prob, b32 Success)
	{
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
	}

	inline void swapContextData(context_data* A, context_data* B)
	{
		context_data Tmp = *A;
		*A = *B;
		*B = Tmp;
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

	void rescale(context* Context)
	{
		context_data Temp;

		LastEncSym->Freq += 4;
		Context->TotalFreq += 4;
		context_data* First = Context->Data;

		u32 MoveCount = LastEncSym - Context->Data;
		if (MoveCount)
		{
			Temp = *LastEncSym;
			for (u32 i = MoveCount; i > 0; i--)
			{
				Context->Data[i] = Context->Data[i - 1];
			}
			*First = Temp;
		}

		u32 EscFreq = Context->TotalFreq - First->Freq;
		First->Freq = (First->Freq + 1) >> 1;

		Context->TotalFreq = First->Freq;

		for (u32 SymbolIndex = 1; SymbolIndex < Context->SymbolCount; SymbolIndex++)
		{
			context_data* Symbol = Context->Data + SymbolIndex;
			EscFreq -= Symbol->Freq;
			Symbol->Freq = (Symbol->Freq + 1) >> 1;
			Context->TotalFreq += Symbol->Freq;

			context_data* Prev = Symbol - 1;
			if (Symbol->Freq > Prev->Freq)
			{
				for (;;)
				{
					context_data* NextPrev = Prev - 1;
					if ((NextPrev != First) && (Symbol->Freq > NextPrev->Freq)) Prev = NextPrev;
					else break;
				}

				u32 MoveCount = Symbol - Prev;
				if (MoveCount)
				{
					Temp = *Symbol;
					for (u32 i = SymbolIndex; MoveCount > 0; i--, MoveCount--)
					{
						Context->Data[i] = Context->Data[i - 1];
					}
					*Prev = Temp;
				}
			}
		}

		EscFreq -= EscFreq >> 1;
		Context->TotalFreq += EscFreq;
	}

	decode_symbol_result getSymbolFromFreq(ArithDecoder& Decoder, context* Context)
	{
		decode_symbol_result Result = {};

		u32 MaskedDiff = Context->SymbolCount - LastMaskedCount;
		LastSEECtx = SEE->getContext(Context, MaskedDiff, LastMaskedCount);

		if (Context == Order0)
		{
			Result.Prob.scale = 1;
		}
		else
		{
			Result.Prob.scale = SEE->getMean(LastSEECtx);
		}

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
			LastEncSym = MatchSymbol;

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
			LastMaskedCount = Context->SymbolCount;
			updateExclusionData(Context);
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
			LastEncSym = First;
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
				LastEncSym = MatchSymbol;

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
				LastMaskedCount = Context->SymbolCount;
				updateExclusionData(Context);
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
			LastEncSym = First;
			Result.Prob.hi = BinCtx->Scale;
			First->Freq += (First->Freq < 128) ? 1 : 0;
			BinCtx->Scale += Interval - SEE->getBinMean(BinCtx->Scale);
			SEE->PrevSuccess = 1;
			Result.Symbol = First->Symbol;
		}
		else
		{
			SEE->PrevSuccess = 0;
			Result.Prob.lo = BinCtx->Scale;
			Result.Prob.hi = FreqMaxValue;
			Result.Symbol = EscapeSymbol;

			BinCtx->Scale -= SEE->getBinMean(BinCtx->Scale);
			InitEsc = ExpEscape[BinCtx->Scale >> 10];
			LastMaskedCount = 1;
			Exclusion->Data[First->Symbol] = 0;
		}

		return Result;
	}

	b32 getEncodeProb(context* Context, prob& Prob, u32 Symbol)
	{
		b32 Result = false;

		u32 MaskedDiff = Context->SymbolCount - LastMaskedCount;
		LastSEECtx = SEE->getContext(Context, MaskedDiff, LastMaskedCount);

		if (Context == Order0)
		{
			Prob.scale = 1;
		}
		else
		{
			Prob.scale = SEE->getMean(LastSEECtx);
		}

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
			LastEncSym = MatchSymbol;

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
			LastMaskedCount = Context->SymbolCount;
			updateExclusionData(Context);
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
			LastEncSym = First;
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
				LastEncSym = MatchSymbol;

				if (MatchSymbol->Freq > MaxFreq)
				{
					rescale(Context);
				}

				Result = true;
			}
			else
			{
				Prob.hi = Prob.scale;
				LastMaskedCount = Context->SymbolCount;
				updateExclusionData(Context);
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
			LastEncSym = First;
			Prob.lo = 0;
			Prob.hi = BinCtx->Scale;
			First->Freq += (First->Freq < 128) ? 1 : 0;
			BinCtx->Scale += Interval - SEE->getBinMean(BinCtx->Scale);
			SEE->PrevSuccess = 1;
			Success = true;
		}
		else
		{
			SEE->PrevSuccess = 0;
			Prob.lo = BinCtx->Scale;
			Prob.hi = FreqMaxValue;
			BinCtx->Scale -= SEE->getBinMean(BinCtx->Scale);
			InitEsc = ExpEscape[BinCtx->Scale >> 10];
			LastMaskedCount = 1;
			Exclusion->Data[First->Symbol] = 0;
		}

		return Success;
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

	//TODO: add init freq
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

	// TODO: alloc and check nullptr outside of the function?
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
				New->TotalFreq = 0;
				New->SymbolCount = 0;
			}
		}

		return New;
	}

	void update()
	{
		context_data** StackPtr = ContextStack;
		context* ContextAt = MaxContext;

		if (ContextAt->SymbolCount == 0)
		{
			do
			{
				context_data* First = ContextAt->Data;
				First->Symbol = LastEncSym->Symbol;
				First->Freq = 1;

				ContextAt->SymbolCount = 1;
				ContextAt = ContextAt->Prev;
				*StackPtr++ = First;
			} while (ContextAt->SymbolCount == 0);
		}

		context_data* NewSym;
		for (; ContextAt != MinContext; ContextAt = ContextAt->Prev, *StackPtr++ = NewSym)
		{
			u32 OldCount = ContextAt->SymbolCount;
			b32 Success = addSymbol(ContextAt, LastEncSym->Symbol);
			if (!Success)
			{
				ContextAt = nullptr;
				break;
			}

			if (OldCount == 1)
			{
				context_data* First = ContextAt->Data;
				if (First->Freq < ((MaxFreq / 4) - 1)) First->Freq += First->Freq;
				else First->Freq = MaxFreq - 4;

				ContextAt->TotalFreq = InitEsc + First->Freq + (MinContext->SymbolCount > 3);
			}
			else
			{	
				u16 AddFreq = (2 * ContextAt->SymbolCount < MinContext->SymbolCount) ? 1 : 0;
				u16 tmp = (4 * ContextAt->SymbolCount <= MinContext->SymbolCount) ? 1 : 0;
				tmp &= (ContextAt->TotalFreq <= 8 * ContextAt->SymbolCount);
				AddFreq += tmp * 2;
				ContextAt->TotalFreq += AddFreq;
			}

			NewSym = ContextAt->Data + (ContextAt->SymbolCount - 1);
			NewSym->Freq = 1;
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

		if (LastSEECtx)
		{
			SEE->updateContext(LastSEECtx);
			LastSEECtx = nullptr;
		}
		
		if (!ContextAt) reset();
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

	inline void clearExclusion()
	{
		MemSet<u8>(reinterpret_cast<u8*>(Exclusion), sizeof(context_data_excl) / sizeof(Exclusion->Data[0]), context_data_excl::Mask);
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
		Exclusion = SubAlloc.alloc<context_data_excl>(1);
		clearExclusion();

		ContextStack = SubAlloc.alloc<context_data*>(OrderCount);

		Order0 = SubAlloc.alloc<context>(1);
		Assert(Order0);
		ZeroStruct(*Order0);

		MaxContext = Order0;
		Order0->TotalFreq = 257;
		Order0->SymbolCount = 256;
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

		MinContext = MaxContext->Prev;
	}
};