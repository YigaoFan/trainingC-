#pragma once
extern void fileTest(bool executed);
extern void TestFileReaderObjectBytes(bool executed);
extern void byteConverterTest(bool executed);
extern void TestStorageAllocator(bool executed);
extern void TestFileCache(bool executed);
extern void typeConverterTest(bool executed);
extern void compileTest(bool executed);
extern void TestLabelNode(bool executed);
extern void TestObjectRelationTree(bool executed);

namespace FuncLib::Test
{
	void AllTest(bool executed)
	{
		TestLabelNode(executed);
		TestObjectRelationTree(executed);
		TestStorageAllocator(executed);
		TestFileReaderObjectBytes(executed);
		fileTest(false);
		TestFileCache(false);
		typeConverterTest(false);
		byteConverterTest(false);
		compileTest(false);
	}
}