#pragma once
#include <type_traits>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "../../Basic/TypeTrait.hpp"
#include "../../Btree/Elements.hpp"
#include "../../Btree/NodeBase.hpp"
#include "../../Btree/MiddleNode.hpp"
#include "../../Btree/LeafNode.hpp"
#include "../../Btree/Btree.hpp"
#include "../../Btree/EnumeratorPipeline.hpp"
#include "../Store/File.hpp"
#include "ByteConverter.hpp"
#include "OwnerState.hpp"

namespace FuncLib::Persistence
{
	using ::Basic::ReturnType;
	using ::Collections::Btree;
	using ::Collections::Elements;
	using ::Collections::EnumeratorPipeline;
	using ::Collections::LeafNode;
	using ::Collections::MiddleNode;
	using ::Collections::NodeBase;
	using ::Collections::order_int;
	using ::Collections::StorePlace;
	using ::std::declval;
	using ::std::make_shared;
	using ::std::shared_ptr;
	using ::std::string;
	using ::std::unique_ptr;

	template <typename T, OwnerState Owner>
	struct TypeConverter
	{
		using From = T;
		using To = T;

		// file is not must use, only for string like
		static To ConvertFrom(From const& t, File* file)
		{
			return t;
		}
	};

	template <typename T>
	struct TypeConverter<T*>
	{
		using From = T*;
		using To = OwnerLessDiskPtr<T>;

		// file is not must use, only for string like
		static To ConvertFrom(From const &t, File* file)
		{
			return t;
		}
	};

	template <typename Key, typename Value, order_int Count, typename LessThan>
	struct TypeConverter<Elements<Key, Value, Count, LessThan>>
	{
		using From = Elements<Key, Value, Count>;
		using To = Elements<typename TypeConverter<Key>::To, typename TypeConverter<Value>::To, Count, LessThan>;

		static To ConvertFrom(From const& from, File* file)
		{
			To to;
			for (auto& e : from)
			{
				to.Append({
					TypeConverter<Key>::ConvertFrom(e.first, file),
					TypeConverter<Value>::ConvertFrom(e.second, file)
				});
			}

			return to;
		}
	};

	template <typename Key, typename Value, order_int Count>
	struct TypeConverter<MiddleNode<Key, Value, Count, StorePlace::Memory>>
	{
		using From = MiddleNode<Key, Value, Count, StorePlace::Memory>;
		using To = MiddleNode<Key, Value, Count, StorePlace::Disk>;

		static UniqueDiskPtr<NodeBase<Key, Value, Count, StorePlace::Disk>> CloneNodeToDisk(typename decltype(declval<From>()._elements)::Item const& item, File* file)
		{
			return TypeConverter<decltype(item.second)>::ConvertFrom(item.second, file);
		}

		static To ConvertFrom(From const& from, File* file)
		{
			using Node = NodeBase<Key, Value, Count, StorePlace::Disk>;
			using ::std::placeholders::_1;
			auto nodes = 
				EnumeratorPipeline<typename decltype(from._elements)::Item const&, UniqueDiskPtr<Node>>(from._elements.GetEnumerator(), bind(&CloneNodeToDisk, _1, file));
			return { move(nodes), from._elements.LessThanPtr };
		}
	};

	template <typename Key, typename Value, order_int Count>
	struct TypeConverter<LeafNode<Key, Value, Count, StorePlace::Memory>>
	{
		using From = LeafNode<Key, Value, Count, StorePlace::Memory>;
		using To = LeafNode<Key, Value, Count, StorePlace::Disk>;

		static To ConvertFrom(From const& from, File* file)
		{
			return { TypeConverter<decltype(from._elements)>::ConvertFrom(from._elements, file), nullptr, nullptr };
		}
	};

	template <typename Key, typename Value, order_int Count>
	struct TypeConverter<NodeBase<Key, Value, Count, StorePlace::Memory>>
	{
		using From = NodeBase<Key, Value, Count, StorePlace::Memory>;
		using To = shared_ptr<NodeBase<Key, Value, Count, StorePlace::Disk>>;

		static To ConvertFrom(From const& from, File* file)
		{
			if (from.Middle())
			{
				using FromMidNode = MiddleNode<Key, Value, Count, StorePlace::Memory>;
				using ToMidNode = MiddleNode<Key, Value, Count, StorePlace::Disk>;
				return make_shared<ToMidNode>(TypeConverter<FromMidNode>::ConvertFrom(static_cast<FromMidNode const&>(from), file));
			}
			else
			{
				using FromLeafNode = LeafNode<Key, Value, Count, StorePlace::Memory>;
				using ToLeafNode = LeafNode<Key, Value, Count, StorePlace::Disk>;
				return make_shared<ToLeafNode>(TypeConverter<FromLeafNode>::ConvertFrom(static_cast<FromLeafNode const&>(from), file));
			}
		}
	};

	template <typename T>
	struct TypeConverter<unique_ptr<T>>
	{
		using From = unique_ptr<T>;

		static auto ConvertFrom(From const& from, File* file)
		{
			auto p = from.get();
			auto c = TypeConverter<T>::ConvertFrom(*p, file);

			using ConvertedType = decltype(c);
			using ::Basic::IsSpecialization;
			// 这里的 MakeUnique 的参数类型会有问题
			if constexpr (IsSpecialization<ConvertedType, shared_ptr>::value)
			{
				return UniqueDiskPtr<typename ConvertedType::element_type>::MakeUnique(c, file);
			}
			// else if constexpr (IsSpecialization<ConvertedType, UniqueDiskPtr>::value)
			// {
			// 	return c;
			// }
			else
			{
				return UniqueDiskPtr<ConvertedType>::MakeUnique(make_shared<ConvertedType>(move(c)), file);
			}
		}

		using To = typename ReturnType<decltype(TypeConverter::ConvertFrom)>::Type;
	};

	template <typename Key, typename Value, order_int Count>
	struct TypeConverter<Btree<Count, Key, Value, StorePlace::Memory>>
	{
		using From = Btree<Count, Key, Value, StorePlace::Memory>;
		using To = Btree<Count, Key, Value, StorePlace::Disk>;

		static To ConvertFrom(From const& from, File* file)
		{
			return
			{ 
				from._keyCount, 
				TypeConverter<unique_ptr<NodeBase<Key, Value, Count, StorePlace::Memory>>>::ConvertFrom(from._root, file) 
			};
		}
	};

	// 思考 Btree 中类型转换过程
	template <>
	struct TypeConverter<string, OwnerState::FullOwner>
	{
		using From = string;
		using To = UniqueDiskRef<string>;

		static To ConvertFrom(From const& from, File* file)
		{
			return { UniqueDiskPtr<string>::MakeUnique(from, file) };
		}
	};

	template <>
	struct TypeConverter<string, OwnerState::OwnerLess>
	{
		using From = string;
		using To = OwnerLessDiskRef<string>;

		// static To ConvertFrom(From const& from, File* file)
		// {
		// 	return { UniqueDiskPtr<string>::MakeUnique(from, file) };
		// }
	};
}