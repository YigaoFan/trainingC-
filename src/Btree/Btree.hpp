#pragma once
#include <vector>       // for vector
#include <functional>   // for function
#include <memory>       // for unique_ptr
#include <utility>      // for pair
#include <algorithm>    // for sort
#include <array>        // for array
#include <exception>    // for exception
#include <optional>
#include "Exception.hpp"
#include "SiblingFunc.hpp"

namespace Btree {
	// TODO use Enumerator to refactor code
	enum class MemoryAllocate {
		Tight,
		WithFree,
	};

	using std::function;
	using std::array;
	using std::pair;
	using std::make_pair;
	using std::vector;
	using std::sort;
	using std::exception;
	using std::runtime_error;
	using std::unique_ptr;
	using std::make_shared;
	using std::size_t;

#define BTREE_TEMPLATE template <uint16_t BtreeOrder, typename Key, typename Value>

	BTREE_TEMPLATE
	class Btree final {
	private:
		using Base   = NodeBase  <Key, Value, BtreeOrder>;
		using Leaf   = LeafNode  <Key, Value, BtreeOrder>;
		using Middle = MiddleNode<Key, Value, BtreeOrder>;

	public:
		using LessThan = typename Base::LessThan;
		template <size_t NumOfEle, MemoryAllocate MemoryAlloc=MemoryAllocate::Tight>
			// not should be array, a type can be iterate is OK?
		Btree(LessThan, array<pair<Key, Value>, NumOfEle>);
		Btree(const Btree&);
		Btree(Btree&&) noexcept;
		Btree& operator=(const Btree&);
		Btree& operator=(Btree&&) noexcept;

		Value       search (const Key&) const;
		vector<Key> explore() const;
		bool        have   (const Key &) const;
		bool        empty  () const;

		void        add    (pair<Key, Value>); // without check
		// void        tryAdd(pair<Key, Value>);
		void        modify (pair<Key, Value>); // without exception
		// void        modifyWithWarn(pair<Key, Value>);

		void        remove (const Key&);

	private:
		shared_ptr<LessThan> _lessThanPtr;
		uint32_t             _keyNum { 0 };
		unique_ptr<Base>     _root { nullptr };

		vector<Leaf*> traverseLeaf(const function<bool(Leaf*)>&) const;
		template <bool FirstCall=true, typename E, size_t Size>
		void constructFromLeafToRoot(array<E, Size>&) noexcept; // When need to use std::move, not mark const

		template <size_t NumOfEle>
		static inline bool duplicateIn(const array<pair<Key, Value>, NumOfEle>&, const Key*&);
		static Leaf* minLeaf(Base*);
		static Leaf* recurSelectNode(Base*, function<Base*(Middle*)>&);
	};
}

namespace Btree {
#define BTREE Btree<BtreeOrder, Key, Value>

	/**
	 * Possible throw 
	 */
	BTREE_TEMPLATE
	template <size_t NumOfEle, MemoryAllocate MemoryAlloc>
	BTREE::Btree(
		LessThan lessThan,
		array<pair<Key, Value>, NumOfEle> pairArray
	) :
		_lessThanPtr(make_shared<LessThan>(lessThan)),
		_root(nullptr)
	{
		if constexpr (NumOfEle == 0) { return; }

		// 可以自己实现一个排序算法，这样找重复的容易些
		// 而且反正 pairArray 是在成功的情况下是要复制的，
		// 这个构造函数这也要复制，不如构造函数传引用，排序算法确定不重复的情况下，就直接复制到堆上
		// 可以确定好几个后一起构造
		sort(pairArray.begin(), pairArray.end(),
			 [&] (const auto& p1, const auto& p2) {
				 return lessThan(p1.first, p2.first);
			 });

		if (const Key* dupKeyPtr; duplicateIn(pairArray, dupKeyPtr)) {
			throw DuplicateKeyException(*dupKeyPtr, "Duplicate key in constructor arg");
		}

		if constexpr (MemoryAlloc == MemoryAllocate::Tight) {
			constructFromLeafToRoot(pairArray);
			_keyNum += NumOfEle;
		} else {
			// TODO allocate with free space
		}
	}

	BTREE_TEMPLATE
	template <bool FirstCall, typename E, size_t Size>
	void
	BTREE::constructFromLeafToRoot(array<E, Size>& nodesMaterial) noexcept
	{
		// TODO should ensure w/2(up bound) to w per node
		if constexpr (Size <= BtreeOrder) {
			if constexpr (FirstCall) {
				_root = make_unique<Leaf>(nodesMaterial.begin(), nodesMaterial.end(), _lessThanPtr);
			} else {
				_root = make_unique<Middle>(nodesMaterial.begin(), nodesMaterial.end(), _lessThanPtr);
			}
			return;
		}
		
		// 这里需要写一个平均分布节点的算法函数
		constexpr auto upperNodeNum = Size % BtreeOrder == 0 ? (Size / BtreeOrder) : (Size / BtreeOrder + 1);
		array<pair<Key, unique_ptr<Base>>, upperNodeNum> upperNodes;

		auto head = nodesMaterial.begin();
		auto end  = nodesMaterial.end();
		auto tail = head + BtreeOrder;
		uint32_t i = 0; // array index

		// construct Leaf need
		auto  _1stLeafFlag = true;
		Leaf* lastLeaf  = nullptr;

		do {
			if constexpr (FirstCall) {
				auto leaf = make_unique<Leaf>(head, tail, _lessThanPtr);
				auto leafPtr = leaf.get();
				// set previous and next
				leafPtr->previousLeaf(lastLeaf);
				if (_1stLeafFlag) {
					_1stLeafFlag = false;
				} else {
					lastLeaf->nextLeaf(leafPtr);
				}
				lastLeaf = leafPtr;

				// TODO how does it work? it's Leaf type
				upperNodes[i] = make_pair(copy(leaf->maxKey()), std::move(leaf));
			} else {
				auto middle = make_unique<Middle>(head, tail, _lessThanPtr);
				upperNodes[i] = make_pair(middle->maxKey(), std::move(middle));
			}

			head = tail;
			tail = (end - tail > BtreeOrder) ? (tail + BtreeOrder) : end;
			++i;
		} while (end - head > 0);

		constructFromLeafToRoot<false>(upperNodes);
	}

	BTREE_TEMPLATE
	BTREE::Btree(const Btree& that)
		: _keyNum(that._keyNum), _root(that._root->clone()), _lessThanPtr(that._lessThanPtr)
	{}

	BTREE_TEMPLATE
	BTREE::Btree(Btree&& that) noexcept
		: _keyNum(that._keyNum), _root(that._root.release()), _lessThanPtr(that._lessThanPtr)
	{
		that._keyNum = 0;
	}

	BTREE_TEMPLATE
	BTREE&
	BTREE::operator=(const Btree& that)
	{
		this->_root.reset(that._root->clone());
		this->_keyNum      = that._keyNum;
		this->_lessThanPtr = that._lessThanPtr;
	}

	BTREE_TEMPLATE
	BTREE&
	BTREE::operator=(Btree&& that) noexcept
	{
		this->_root.reset(that._root.release());
		this->_keyNum      = that._keyNum;
		this->_lessThanPtr = that._lessThanPtr;
	}

	BTREE_TEMPLATE
	Value
	BTREE::search(const Key& key) const
	{
		if (empty()) {
			throw runtime_error("The tree is empty");
		}

		return _root->search(key);
	}

	BTREE_TEMPLATE
	void
	BTREE::add(pair<Key, Value> p)
	{
		if (empty()) {
			auto leaf = make_unique<Leaf>(&p, &p + 1, _lessThanPtr);
			_root.reset(leaf.release());
		} else {
			vector<Base*> passedNodeTrackStack;
			if (_root->have(p.first, passedNodeTrackStack)) {
				throw runtime_error("The key-value has already existed, can't be added.");
			} else {
				_root->add(std::move(p), passedNodeTrackStack);
			}
		}

		++_keyNum;
	}

	BTREE_TEMPLATE
	void
	BTREE::modify(pair<Key, Value> pair)
	{
		if (!empty()) {
			_root->modify(pair.first, std::move(pair.second));
		}
	}

	BTREE_TEMPLATE
	vector<Key>
	BTREE::explore() const
	{
		vector<Key> keys;
		keys.reserve(_keyNum);

		traverseLeaf([&keys](Leaf* l) {
			auto&& ks = l->allKey();
			keys.insert(keys.end(), ks.begin(), ks.end());

			return false;
		});

		return keys;
	}

	BTREE_TEMPLATE
	void
	BTREE::remove(const Key& key)
	{
		vector<Base*> passedNodeTrackStack;
		auto& stack = passedNodeTrackStack;

		if (empty()) {
			return;
		}
		if (_root->have(key, stack)) {
			_root->remove(key, stack);
			--_keyNum;
		}
	}

	BTREE_TEMPLATE
	bool
	BTREE::have(const Key& key) const
	{
		if (!empty()) {
			return _root->have(key);
		}

		return false;
	}

	BTREE_TEMPLATE
	bool
	BTREE::empty() const
	{
		return _keyNum == 0;
	}

	BTREE_TEMPLATE
	vector<typename BTREE::Leaf*>
	BTREE::traverseLeaf(const function<bool(Leaf*)>& predicate) const
	{
		vector<Leaf*> leafCollection { };

		if (empty()) {
			return leafCollection;
		}
		auto current = minLeaf(_root.get());
		while (current != nullptr) {
			if (predicate(current)) {
				leafCollection.emplace_back(current);
			}

			current = current->nextLeaf();
		}

		return leafCollection;
	}

	BTREE_TEMPLATE
	template <size_t NumOfEle>
	bool
	BTREE::duplicateIn(const array<pair<Key, Value>, NumOfEle>& sortedPairArray, const Key*& duplicateKey)
	{
		auto& array = sortedPairArray;

		for (auto i = 1; i < NumOfEle; ++i) {
			// should use LessThan ? TODO
			if (array[i].first == array[i-1].first) {
				duplicateKey = &array[i].first;
				return true;
			}
		}

		duplicateKey = nullptr;
		return false;
	}

	BTREE_TEMPLATE
	typename BTREE::Leaf*
	BTREE::minLeaf(Base* node)
	{
		function<Base*(Middle*)> min = [] (auto n) {
			return n->minSon();
		};
        
		return recurSelectNode(node, min);
	}

	BTREE_TEMPLATE
	typename BTREE::Leaf*
	BTREE::recurSelectNode(Base* node, function<Base*(Middle*)>& choose)
	{
		while (node->middle()) {
			node = choose(static_cast<Middle*>(node));
		}

		return static_cast<Leaf*>(node);
	}

#undef BTREE
#undef BTREE_TEMPLATE

	template <auto Total, auto DivNum>
	constexpr
	auto Cal()
	{
		if constexpr (constexpr auto average = Total / DivNum; auto remainder = Total % DivNum == 0) {
			// make a seq that contains DivNum copies average
		} else {
			// make a seq that contains DivNum copies average, too
			// allocate the remainder to each item in part of seq in suitable way
		}

	}
}

template <int Element>
struct GetEle
{
	static constexpr auto get(int n)
	{
		return Element;
	}
};

template <uint32_t Num, int Item, size_t... I>
constexpr auto
ImpDupItem(index_sequence<I...>)
{
	return integer_sequence<uint32_t, GetEle<Item>::get(I)...>();
}

template <uint32_t Num, int Item>
constexpr auto
DupEle()
{
	constexpr auto indexs = make_index_sequence<Num>();
	return ImpDupItem<Num, Item, decltype(indexs)>(indexs);
}

int main()
{
	auto eles = DupEle<3, 4>();
	using T = decltype(eles);
	static_assert(!is_same_v<T, integer_sequence<int, 4, 4, 4>>, "Not same");
}
