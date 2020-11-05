#include "../../Basic/Exception.hpp"
#include "StorageAllocator.hpp"
#include "../Persistence/ByteConverter.hpp"

namespace FuncLib::Store
{
	constexpr pos_lable FileLable = 0;

	StorageAllocator StorageAllocator::ReadAllocatedInfoFrom(FileReader* reader)
	{
		// using DataMember0 = decltype(allocator._posLableTable);
		// auto posLableTable = ByteConverter<DataMember0>::ReadOut(reader);

		return { 0, {} };
	}

	void StorageAllocator::WriteAllocatedInfoTo(StorageAllocator const& allocator, ObjectBytes* bytes)
	{
		using DataMember0 = decltype(allocator._posLableTable);
		ByteConverter<DataMember0>::WriteDown(allocator._posLableTable, bytes);
	}

	StorageAllocator::StorageAllocator(pos_int currentPos, map<pos_lable, pair<pos_int, size_t>> posLableTable)
		: _currentPos(currentPos), _posLableTable(move(posLableTable))
	{ }

	bool StorageAllocator::Ready(pos_lable posLable) const
	{
		return _posLableTable.contains(posLable);
	}

	pos_int StorageAllocator::GetConcretePos(pos_lable posLable) const
	{
		return _posLableTable.find(posLable)->second.first;
	}

	size_t StorageAllocator::GetAllocatedSize(pos_lable posLable) const
	{
		return _posLableTable.find(posLable)->second.second;
	}

	pos_lable StorageAllocator::AllocatePosLable()
	{
		/// 0 represents File, so from 1
		return _posLableTable.size() + 1;
	}

	void StorageAllocator::DeallocatePosLable(pos_lable posLable)
	{
		auto allocateInfoIter = _posLableTable.find(posLable);
		_deletedLables.insert(*allocateInfoIter);
		_posLableTable.erase(posLable); // 那是什么时候调整分配大小，那时候调用上面具体位置的地方就会受影响，所以要尽量少的依赖具体位置
	}

#define VALID_CHECK                                                                                     \
	if (_deletedLables.contains(posLable))                                                              \
	{                                                                                                   \
		using ::Basic::InvalidAccessException;                                                          \
		throw InvalidAccessException("Apply space for deleted lable, it means have wrong code logic."); \
	}
	/// for first use
	pos_int StorageAllocator::GiveSpaceTo(pos_lable posLable, size_t size)
	{
		VALID_CHECK;
		
		// 这里是个粗糙的内存分配算法实现
		_posLableTable.insert({ posLable, { _currentPos, size }});
		_currentPos += size;
		return _currentPos - size;
	}

	pos_int StorageAllocator::ResizeSpaceTo(pos_lable posLable, size_t biggerSize)
	{
		VALID_CHECK;

		using ::std::make_pair;
		auto allocateInfoIter = _posLableTable.find(posLable);
		_deletedLables.insert({ FileLable, allocateInfoIter->second });
		allocateInfoIter->second = make_pair(_currentPos, biggerSize);
		_currentPos += biggerSize;
		return _currentPos - biggerSize;
	}
#undef VALID_CHECK

	void StorageAllocator::DeallocatePosLables(set<pos_lable> const& posLables)
	{
		for (auto p : posLables)
		{
			DeallocatePosLable(p);
		}
	}
}
