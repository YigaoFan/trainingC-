#pragma once
#include <filesystem>
#include <memory>
#include <map>
#include "DiskDataConverter.hpp"
#include "File.hpp"

namespace FuncLib
{
	using ::std::shared_ptr;
	using ::std::map;
	using ::std::size_t;
	using ::std::filesystem::path;

	template <typename T>
	class DiskPos
	{
	private:
		friend struct DiskDataConverter<DiskPos>;// TODO pre declare no problem
		static map<size_t, shared_ptr<T>> _cache; // 用 start 应该是没事的, if change, should delete

		size_t _start;
		shared_ptr<File> _file;
	public:
		using Index = decltype(_start);

		DiskPos(shared_ptr<File> file, size_t start) : _start(start), _file(file)
		{ }

		shared_ptr<T> ReadObject()
		{
			return DiskDataConverter::ConvertFromDiskData<T>(_file, _start);
			/*if (!this->_cache.contains(_start))
			{
				auto p = shared_ptr(CurrentFile::Read(_start, _size));
				_cache.insert({ _start, p });
				return p;
			}

			return this->_cache[_start];*/
		}

		void WriteObject()
		{

		}

		~DiskPos();

	private:

	};
}