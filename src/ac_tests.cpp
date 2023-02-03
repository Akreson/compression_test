
void
CompressFile(StaticByteModel& Model, file_data& InputFile, ByteVec& OutBuffer)
{
	ArithEncoder Encoder(OutBuffer);

	for (u32 i = 0; i < InputFile.Size; ++i)
	{
		prob SymbolProb = Model.getProb(InputFile.Data[i]);
		Encoder.encode(SymbolProb);
		Model.update(InputFile.Data[i]);
	}

	prob SymbolProb = Model.getEndStreamProb();
	Encoder.encode(SymbolProb);
}

void
DecompressFile(StaticByteModel& Model, file_data& OutputFile, ByteVec& InputBuffer)
{
	ArithDecoder Decoder(InputBuffer);

	u64 ByteIndex = 0;
	for (;;)
	{
		u32 DecodedFreq = Decoder.getCurrFreq(Model.getCount());

		u32 DecodedSymbol;
		prob Prob = Model.getByteFromFreq(DecodedFreq, &DecodedSymbol);

		if (DecodedSymbol == StaticByteModel::EndOfStreamSymbolIndex) break;

		Decoder.updateDecodeRange(Prob);
		Model.update(DecodedSymbol);

		Assert(ByteIndex <= OutputFile.Size);
		OutputFile.Data[ByteIndex++] = DecodedSymbol;
	}
}

void
TestStaticModel(file_data& InputFile)
{
	StaticByteModel Model;
	ByteVec CompressBuffer;

	CompressFile(Model, InputFile, CompressBuffer);
	u64 CompressedSize = CompressBuffer.size();

	printf("compression ratio %.3f", (double)InputFile.Size / (double)CompressedSize);

	Model.reset();

	file_data OutputFile;
	OutputFile.Size = InputFile.Size;
	OutputFile.Data = new u8[OutputFile.Size];

	DecompressFile(Model, OutputFile, CompressBuffer);

	for (u32 i = 0; i < InputFile.Size; ++i)
	{
		b32 cmp = InputFile.Data[i] == OutputFile.Data[i];
		Assert(cmp);
	}

	delete[] OutputFile.Data;
}

void
CompressFile(PPMByte& Model, file_data& InputFile, ByteVec& OutBuffer)
{
	ArithEncoder Encoder(OutBuffer);

	for (u32 i = 0; i < InputFile.Size; ++i)
	{
		Model.encode(Encoder, InputFile.Data[i]);

		if (!(i & 0xffff))
		{
			//printf("%d %d %d %d\n", i, Model.SubAlloc.FreeListCount, Model.SubAlloc.FreeMem >> 10, Model.SubAlloc.FreeMem >> 20);
			printf("%d\r", i);
		}
	}

	Model.encodeEndOfStream(Encoder);
}

void
DecompressFile(PPMByte& Model, file_data& OutputFile, ByteVec& InputBuffer, file_data& InputFile)
{
	ArithDecoder Decoder(InputBuffer);

	u64 ByteIndex = 0;
	for (;;)
	{
		u32 DecodedSymbol = Model.decode(Decoder);
		if (DecodedSymbol == PPMByte::EscapeSymbol) break;

		Assert(ByteIndex <= OutputFile.Size);
		Assert(InputFile.Data[ByteIndex] == DecodedSymbol)
			OutputFile.Data[ByteIndex++] = DecodedSymbol;

		if (!(ByteIndex & 0xffff))
		{
			//printf("%d %d %d %d\r", ByteIndex, Model.SubAlloc.FreeListCount, Model.SubAlloc.FreeMem >> 10, Model.SubAlloc.FreeMem >> 20);
			printf("%d\r", ByteIndex);
		}
	}
}

void
TestPPMModel(file_data& InputFile)
{
	u32 MemLimit = 20 << 20;
	PPMByte PPMModel(4, MemLimit);
	ByteVec CompressBuffer;

	CompressFile(PPMModel, InputFile, CompressBuffer);
	u64 CompressedSize = CompressBuffer.size();
	printf("compression ratio %.3f\n", (f64)InputFile.Size / (f64)CompressedSize);
	printf("Sym: %.3f | Esc: %.3f", PPMModel.SymEnc, PPMModel.EscEnc);

	PPMModel.reset();

#if 1
	file_data OutputFile;
	OutputFile.Size = InputFile.Size;
	OutputFile.Data = new u8[OutputFile.Size];

	DecompressFile(PPMModel, OutputFile, CompressBuffer, InputFile);

	delete[] OutputFile.Data;
#endif
}