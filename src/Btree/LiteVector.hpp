#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include <array>
#include <type_traits>
#include "IEnumerator.hpp"

namespace Collections
{
	using ::std::size_t;
	using ::std::uint8_t;
	using ::std::move;
	using ::std::vector;
	using ::std::array;
	using ::std::remove_const_t;

	template <typename T, typename size_int, size_int Capacity>
	class LiteVector
	{
	private:
		array<uint8_t, sizeof(T) * Capacity> _mem;
		T* const _ptr = reinterpret_cast<T*>(&_mem[0]);
		size_int _count = 0;

	public:
		using Item = T;
		
		LiteVector() { }

		template <typename... Ts>
		LiteVector(T t, Ts... ts)
		{
			Init(move(t), move(ts)...);
		}

		LiteVector(IEnumerator<T> auto enumerator)
		{
			while (enumerator.MoveNext())
			{
				Add(move(enumerator.Current()));
			}
		}

		LiteVector(LiteVector const& that)
		{
			for (auto& i : that)
			{
				Add(i);
				//_ptr[_count++] = i;
				// difference to up: this is assign.
				// Maybe need to destruct left value first(but must some operation), then copy
				// but here memory is raw, error
			}
		}

		LiteVector(LiteVector&& that)
			: _mem(move(that._mem)), _count(that._count)
		{ 
			that._count = 0;
		}

		~LiteVector()
		{
			while (_count != 0)
			{
				auto& e = _ptr[--_count];
				e.~T();
			}
		}

		void Add(T t)
		{
			new (_ptr + (_count++))T(move(t));
		}

		void RemoveAt(size_int i)
		{
			MoveLeftOnePlaceFrom(i + 1);
			--_count;
		}

		vector<T> PopOutItems(size_int count)
		{
			vector<T> outItems;
			outItems.reserve(count);
			for (; count != 0; --count)
			{
				outItems.insert(outItems.begin(), PopOut());
			}

			return outItems;
		}

		vector<T> PopOutAll()
		{
			return PopOutItems(_count);
		}

		T PopOut()
		{
			auto t = move(_ptr[--_count]);
			_ptr[_count].~T();
			return t;
		}

		T FrontPopOut()
		{
			auto p = move(_ptr[0]);
			RemoveAt(0);
			return move(p);
		}

		/// Emplace on back is wrong behavior
		void Emplace(size_int index, T t)
		{
			new (_ptr + _count) T(move(_ptr[_count - 1]));

			// i is move to place
			for (auto i = _count - 1; i > index; --i)
			{
				_ptr[i] = move(_ptr[i - 1]);
			}

			_ptr[index] = move(t);
			++_count;
		}

		void EmplaceHead(T t)
		{
			Emplace(0, move(t));
		}

		T& operator[] (size_int i) { return _ptr[i]; }
		T const& operator[] (size_int i) const { return _ptr[i]; }
		T* begin() { return _ptr; }
		T* end() { return _ptr + _count; }
		T const* begin() const { return _ptr; }
		T const* end() const { return _ptr + _count; }
		T& FirstOne() { return _ptr[0]; }
		T& LastOne() { return _ptr[_count - 1]; }
		T const& FirstOne() const { return _ptr[0]; }
		T const& LastOne() const { return _ptr[_count - 1]; }

		size_int Count() const { return _count; }
		bool Full() const { return _count == Capacity; }
		bool Empty() const { return _count == 0; }

	private:
		template <typename... Ts>
		void Init(T t, Ts... ts)
		{
			Add(move(t));
			if constexpr (sizeof...(ts) > 0)
			{
				Init(move(ts)...);
			}
		}

		/// Tail position will be destructed
		void MoveLeftOnePlaceFrom(size_int index)
		{
			// i is move to place
			auto i = index - 1;

			for (; i < _count - 1; ++i)
			{
				_ptr[i] = move(_ptr[i + 1]);
			}
			_ptr[i].~T();
		}
	};
}