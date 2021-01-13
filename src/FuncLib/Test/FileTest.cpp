#include <memory>
#include <array>
#include <vector>
#include <cstddef>
#include <cstdio>
#include "Util.hpp"
#include "../../TestFrame/FlyTest.hpp"
#include "../Persistence/ByteConverter.hpp"
#include "../Persistence/TypeConverter.hpp"

using namespace std;
using namespace FuncLib;
using namespace FuncLib::Persistence;
using namespace FuncLib::Store;
using namespace FuncLib::Test;

TESTCASE("File test")
{
	auto filename = "fileTest";
	// // 函数库要写一些元信息和校验数据，平台限定
	// // 需要使用 std::bit_cast 来转换使用不相关类型吗？
		

	SECTION("Simple")
	{
		auto file = File::GetFile(filename);
		auto [posLabel, obj] = file->New(1);
		auto obj1 = file->Read<int>(posLabel);
		ASSERT(obj == obj1);
		ASSERT(*obj == 1);
		file->Store(posLabel, obj);
		// file->Delete(posLabel, obj);
		Cleaner c(filename);
	}
	
	SECTION("Complex")
	{
		// 重读就有问题
		auto file = File::GetFile(filename);
		using namespace Collections;
		using Tree = Btree<4, int, int>; // 之后用 string 作为 key，value
		auto lessThan = [](int a, int b) { return a < b; };
		Tree b(lessThan);
		auto n = 100;
		for (auto i = 0; i < n; ++i)
		{
			b.Add({ i, i });
		}
		
		auto t = TypeConverter<Tree>::ConvertFrom(b, file.get());
		// operate on converted tree
		for (auto i = 0; i < n; ++i)
		{
			ASSERT(t.ContainsKey(i));
		}

		for (auto i = n; i < n + 100; ++i)
		{
			t.Add({ i, i });
			ASSERT(t.ContainsKey(i));
		}

		// auto [label, treeObj] = file->New(move(t));

		// file->Store(label, treeObj);
	}

	// TODO 读取已经存在的 File 有问题
	// TODO TypeConverter 转换后的 Btree 有问题
	// TODO 转换后 Btree 来进行一些 Btree 的操作操作，注意加入、删除和修改一些节点的时候

	// Cleaner c(filename);
}

DEF_TEST_FUNC(TestFile)