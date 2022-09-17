struct context_model_data
{
	union
	{
		struct
		{
			u32 Freq[257];
			u32 TotalFreq;
		};

		u32 CumFreq[258];
	};
};

struct context_model
{
	context_model_data* Data;
	context_model* Next;

	static constexpr u32 FreqArraySize = ArrayCount(Data->Freq);
	static constexpr u32 EscapeSymbolIndex = FreqArraySize - 1;
	static constexpr u32 CumFreqArraySize = ArrayCount(Data->CumFreq);

	inline void clear()
	{
		if (Data)
		{
			ZeroSize(static_cast<void*>(Data), sizeof(context_model_data));
		}

		if (Next)
		{
			ZeroSize(static_cast<void*>(Next), sizeof(context_model) * 256);
		}
	}
};

struct decode_symbol_result
{
	prob Prob;
	u32 Symbol;
};

class PPMByte
{
	static constexpr u32 FreqArraySize = context_model::FreqArraySize;
	static constexpr u32 CumFreqArraySize = context_model::CumFreqArraySize;

	context_model_data* ExclusionData;
	context_model_data* StaticContext; // order -1
	context_model* ContextZero;

	u32 ContextAllocated;

	u32 OrderCount;
	u32 CurrSetOrderCount;
	//u32 StackSize;

	u32* ContextSeq;
	context_model** ContextStack;

public:
	static constexpr u32 EscapeSymbolIndex = context_model::EscapeSymbolIndex;

	// For debug
	u64 ContextCount;
	u64 SymbolProcessed;
	u64 MemUse;

	PPMByte() = delete;
	PPMByte(u32 MaxOrderContext) : OrderCount(MaxOrderContext), CurrSetOrderCount(0), ContextAllocated(0), MemUse(0), SymbolProcessed(0), ContextCount(0)
	{
		StaticContext = new context_model_data;
		MemSet(reinterpret_cast<u32*>(StaticContext), sizeof(context_model_data) / 4, 1);

		ExclusionData = new context_model_data;
		clearExclusion(ExclusionData);

		ContextZero = new context_model;
		allocContext(ContextZero);

		ContextCount += 1;
		MemUse += sizeof(context_model_data) * 2;
		MemUse += sizeof(context_model);

		// last encoded symbols
		ContextSeq = new u32[MaxOrderContext];

		// max symbol seq + context_model for that seq
		ContextStack = new context_model * [MaxOrderContext + 1];
	}

	~PPMByte()
	{
		reset();

		delete StaticContext;
		delete ContextZero;
		delete ContextSeq;
		delete ContextStack;
		delete ExclusionData;
	}

	void encode(ArithEncoder& Encoder, u32 Symbol)
	{
		u32 LookFromSeqIndex = 0;
		u32 OrderLooksLeft = CurrSetOrderCount + 1;
		//context_model* PrevContext = 0;
		while (OrderLooksLeft)
		{
			context_model* LookAtContext = findContext(LookFromSeqIndex, CurrSetOrderCount);
			ContextStack[LookFromSeqIndex++] = LookAtContext;

			b32 Success = encodeSymbol(Encoder, LookAtContext->Data, Symbol);
			if (Success) break;

			if (LookAtContext->Data->TotalFreq)
			{
				updateExclusionData(LookAtContext);
			}

			OrderLooksLeft--;
		}

		if (!OrderLooksLeft)
		{
			prob Prob = getProb(StaticContext, Symbol);
			Encoder.encode(Prob);

			if (!LookFromSeqIndex)
			{
				LookFromSeqIndex++;
				ContextStack[0] = ContextZero;
			}
		}

		updateOrderSeq(Symbol);
		update(Symbol, LookFromSeqIndex);
		clearExclusion(ExclusionData);

		SymbolProcessed++;
	}

	void encodeEndOfStream(ArithEncoder& Encoder)
	{
		u32 OrderIndex = 0;
		while (OrderIndex <= CurrSetOrderCount)
		{
			context_model* Context = ContextStack[OrderIndex++];
			encodeSymbol(Encoder, Context->Data, EscapeSymbolIndex);
			updateExclusionData(Context);
		}

		prob Prob = getProb(StaticContext, EscapeSymbolIndex);
		Encoder.encode(Prob);
	}

	u32 decode(ArithDecoder& Decoder)
	{
		u32 ResultSymbol;
		u32 LookFromSeqIndex = 0;
		u32 OrderLooksLeft = CurrSetOrderCount + 1;
		//context_model* PrevContext = 0;
		while (OrderLooksLeft)
		{
			context_model* LookAtContext = findContext(LookFromSeqIndex, CurrSetOrderCount);
			ContextStack[LookFromSeqIndex++] = LookAtContext;

			b32 Success = false;
			if (LookAtContext->Data->TotalFreq)
			{
				Success = decodeSymbol(Decoder, LookAtContext->Data, &ResultSymbol);
			}

			if (Success) break;

			updateExclusionData(LookAtContext);
			OrderLooksLeft--;
		}

		if (!OrderLooksLeft)
		{
			u32 ExclTotal = getExcludedTotal(StaticContext);
			u32 CurrFreq = Decoder.getCurrFreq(ExclTotal);
			decode_symbol_result DecodedSymbol = getSymbolFromFreq(StaticContext, CurrFreq, ExclTotal);
			ResultSymbol = DecodedSymbol.Symbol;

			// if it not end of stream
			if (ResultSymbol != EscapeSymbolIndex)
			{
				Decoder.updateDecodeRange(DecodedSymbol.Prob);

				if (!LookFromSeqIndex)
				{
					LookFromSeqIndex++;
					ContextStack[0] = ContextZero;
				}
			}
		}

		updateOrderSeq(ResultSymbol);
		update(ResultSymbol, LookFromSeqIndex);
		clearExclusion(ExclusionData);

		return ResultSymbol;
	}

	void reset()
	{
		if (ContextZero->Next)
		{
			for (u32 i = 0; i < 256; ++i)
			{
				context_model* Context = ContextZero->Next + i;
				freeContext(Context);
			}
		}
		ContextZero->clear();

		CurrSetOrderCount = 0;
		ZeroSize(static_cast<void*>(ContextSeq), sizeof(*ContextSeq) * OrderCount);
		ZeroSize(static_cast<void*>(ContextStack), sizeof(*ContextStack) * OrderCount);
	}

private:
	prob getProb(context_model_data* Data, u32 Symbol, u32 CumFreqLo, u32 ExclTotal)
	{
		prob Result;

		Result.lo = CumFreqLo;
		Result.hi = CumFreqLo + Data->Freq[Symbol];
		Result.count = ExclTotal;

		return Result;
	}

	decode_symbol_result getSymbolFromFreq(context_model_data* Data, u32 DecodeFreq, u32 ExclTotal)
	{
		u32 SymbolIndex = 0;
		u32 CumFreq = 0;
		for (;; ++SymbolIndex)
		{
			u32 ModFreq = Data->Freq[SymbolIndex] & ExclusionData->Freq[SymbolIndex];
			u32 CheckFreq = CumFreq + ModFreq;

			if (CheckFreq > DecodeFreq) break;

			CumFreq += ModFreq;
		}

		decode_symbol_result Result;
		Result.Prob = getProb(Data, SymbolIndex, CumFreq, ExclTotal);
		Result.Symbol = SymbolIndex;

		return Result;
	}

	u32 getExcludedTotal(context_model_data* Data)
	{
		u32 Result = 0;
		for (u32 i = 0; i < FreqArraySize; ++i)
		{
			Result += Data->Freq[i] & ExclusionData->Freq[i];
		}

		return Result;
	}

	b32 decodeSymbol(ArithDecoder& Decoder, context_model_data* Data, u32* ResultSymbol)
	{
		b32 Success = false;
		u32 ExclTotal = getExcludedTotal(Data);
		u32 CurrFreq = Decoder.getCurrFreq(ExclTotal);

		decode_symbol_result DecodedSymbol = getSymbolFromFreq(Data, CurrFreq, ExclTotal);
		Decoder.updateDecodeRange(DecodedSymbol.Prob);

		if (DecodedSymbol.Symbol != EscapeSymbolIndex)
		{
			*ResultSymbol = DecodedSymbol.Symbol;
			Success = true;
		}

		return Success;
	}

	void rescale(context_model_data* Data)
	{
		Data->TotalFreq = Data->Freq[EscapeSymbolIndex];
		for (u32 i = 0; i < EscapeSymbolIndex; ++i)
		{
			u32 NewFreq = Data->Freq[i];
			NewFreq = (NewFreq + 1) / 2;
			Data->Freq[i] = NewFreq;
			Data->TotalFreq += NewFreq;
		}
	}

	void update(u32 Symbol, u32 StackSize)
	{
		u32 UpdateIndex = 0;
		while (StackSize--)
		{
			context_model* UpdateContext = ContextStack[UpdateIndex++];
			context_model_data* UpdateData = UpdateContext->Data;

			if (UpdateData->TotalFreq >= FreqMaxValue)
			{
				rescale(UpdateData);
			}

			// method C
			if (!UpdateData->Freq[Symbol])
			{
				UpdateData->Freq[EscapeSymbolIndex] += 1;
				UpdateData->TotalFreq++;
			}

			// method A
			/*u32 Escape = UpdateData->Freq[EscapeSymbolIndex];
			if (!Escape)
			{
				UpdateData->Freq[EscapeSymbolIndex] = 1;
				UpdateData->TotalFreq++;
			}*/

			u32 Inc = 1;
			UpdateData->Freq[Symbol] += Inc;
			UpdateData->TotalFreq += Inc;
		}
	}

	prob getProb(context_model_data* Data, u32 Symbol)
	{
		prob Result;

		u32 CumFreqLo = 0;
		for (u32 i = 0; i < Symbol; ++i)
		{
			u32 Freq = Data->Freq[i] & ExclusionData->Freq[i];
			CumFreqLo += Freq;
		}

		u32 CumFreqHi = 0;
		for (u32 i = Symbol; i < FreqArraySize; ++i)
		{
			CumFreqHi += Data->Freq[i] & ExclusionData->Freq[i];
		}

		Result.lo = CumFreqLo;
		Result.hi = CumFreqLo + Data->Freq[Symbol];
		Result.count = CumFreqLo + CumFreqHi;

		return Result;
	}

	b32 encodeSymbol(ArithEncoder& Encoder, context_model_data* Data, u32 Symbol)
	{
		b32 Success = false;

		prob Prob = {};
		if (Data->Freq[Symbol])
		{
			Success = true;
			prob Prob = getProb(Data, Symbol);
			Encoder.encode(Prob);
		}
		else
		{
			if (Data->Freq[EscapeSymbolIndex])
			{
				prob Prob = getProb(Data, EscapeSymbolIndex);
				Encoder.encode(Prob);
			}
		}

		return Success;
	}

	// TODO: break from loop if context not created?
	context_model* findContext(u32 From, u32 To)
	{
		u32 LookAtOrder = From;
		context_model* CurrContext = ContextZero;

		while (LookAtOrder < To)
		{
			u32 SymbolAtContext = ContextSeq[LookAtOrder];
			Assert(SymbolAtContext < 256);

			if (!CurrContext->Next)
			{
				CurrContext->Next = new context_model[256];
				ZeroSize(static_cast<void*>(CurrContext->Next), sizeof(context_model) * 256);

				MemUse += (sizeof(context_model) * 256);
				ContextCount++;
			}

			context_model* NextContext = CurrContext->Next + SymbolAtContext;

			CurrContext = NextContext;
			LookAtOrder++;
		}

		if (!CurrContext->Data)
		{
			CurrContext->Data = new context_model_data;
			ZeroSize(static_cast<void*>(CurrContext->Data), sizeof(context_model_data));

			MemUse += sizeof(context_model_data);
			if (!CurrContext->Next)
			{
				ContextCount++;
			}
		}

		return CurrContext;
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

	inline void clearExclusion(context_model_data* Data)
	{
		MemSet(reinterpret_cast<u32*>(Data), sizeof(context_model_data) / 4, MaxUInt32);
	}

	void updateExclusionData(context_model* Context)
	{
		context_model_data* ContextData = Context->Data;
		for (u32 i = 0; i < EscapeSymbolIndex; ++i)
		{
			u32 ToExcl = ContextData->Freq[i] ? 0 : MaxUInt32;
			ExclusionData->Freq[i] = ToExcl;
		}
	}

	void freeContext(context_model* Context)
	{
		if (Context->Data)
		{
			delete Context->Data;
		}

		if (Context->Next)
		{
			for (u32 i = 0; i < 256; ++i)
			{
				context_model* LookContext = Context->Next + i;
				freeContext(LookContext);
			}

			delete Context->Next;
		}
	}

	void allocContext(context_model* Context)
	{
		Context->Data = new context_model_data;
		Context->Next = new context_model[256];
		Context->clear();

		MemUse += sizeof(context_model_data) + (sizeof(context_model) * 256);
		ContextCount++;
	}

	inline void resetStatic(context_model_data& ContextData)
	{
		ContextData.TotalFreq = FreqArraySize;
		for (u32 i = 0; i < CumFreqArraySize; ++i)
		{
			ContextData.CumFreq[i] = i;
		}
	}
};