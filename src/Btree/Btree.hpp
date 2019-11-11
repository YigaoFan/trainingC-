#pragma once
#include <vector>       // for vector
#include <functional>   // for function
#include <memory>       // for unique_ptr
#include <utility>      // for pair
#include <algorithm>    // for sort
#include <array>        // for array
#include <exception>    // for exception
#include "SiblingFunc.hpp"

namespace Btree {
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
		void        add    (pair<Key, Value>); // without check
		// void        tryAdd(pair<Key, Value>);
		void        modify (pair<Key, Value>); // without exception
		// void        modifyWithWarn(pair<Key, Value>);
		vector<Key> explore() const;
		void        remove (const Key&);
		bool        have   (const Key&) const;
		bool        empty  () const;

	private:
		shared_ptr<LessThan> _lessThanPtr;
		uint32_t             _keyNum { 0 };
		unique_ptr<Base>     _root { nullptr };

		vector<Leaf*> traverseLeaf(const function<bool(Leaf*)>&) const;
		template <bool FirstCall=true, typename E, size_t Size>
		void constructTreeFromLeafToRoot(array<E, Size>&) noexcept; // When need to use std::move, not mark const

		template <size_t NumOfEle>
		static inline bool duplicateIn(const array<pair<Key, Value>, NumOfEle>&, const Key*&);
		static Leaf* minLeaf(Base*);
		static Leaf* recurSelectNode(Base*, function<Base*(Middle*)>&);
	};
}

namespace Btree {
#define BTREE Btree<BtreeOrder, Key, Value>

	/**
	 *
	 * @tparam BtreeOrder
	 * @tparam Key
	 * @tparam Value
	 * @tparam NumOfEle
	 * @tparam MemoryAlloc
	 * @param lessThan
	 * @param pairArray
	 */
	BTREE_TEMPLATE
	template <size_t NumOfEle, MemoryAllocate MemoryAlloc>
	BTREE::Btree(
		LessThan lessThan,
		array<pair<Key, Value>, NumOfEle> pairArray
	) :
		_lessThanPtr(make_shared<LessThan>(lessThan))
	{
		if constexpr (NumOfEle == 0) { return; }

		sort(pairArray.begin(),
			 pairArray.end(),
			 [&] (const auto& p1, const auto& p2) {
				 return lessThan(p1.first, p2.first);
			 });
		const Key* duplicateKeyPtr;
		if (duplicateIn(pairArray, duplicateKeyPtr)) {
			throw runtime_error("Duplicate key in key-value array");
		}

		if constexpr (MemoryAlloc == MemoryAllocate::Tight) {
			constructTreeFromLeafToRoot(pairArray);
			_keyNum += NumOfEle;
		} else {
			// TODO allocate with free space
		}
	}

	BTREE_TEMPLATE
	template <bool FirstCall, typename E, size_t Size>
	void
	BTREE::constructTreeFromLeafToRoot(array<E, Size>& nodesMaterial) noexcept
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
		
		constexpr auto upperNodeNum =  Size % BtreeOrder == 0 ?  (Size / BtreeOrder) :  (Size / BtreeOrder + 1);
		array<pair<Key, unique_ptr<Base>>, upperNodeNum> upperNodes;

		auto head = nodesMaterial.begin();
		auto end  = nodesMaterial.end();
		auto tail = head + BtreeOrder;
		uint32_t i = 0; // array index

		// construct Leaf need
		auto  firstLeaf = true;
		Leaf* lastLeaf  = nullptr;

		do {
			if constexpr (FirstCall) {
				auto leaf = make_unique<Leaf>(head, tail, _lessThanPtr);
				auto leafPtr = leaf.get();
				// set previous and next
				leafPtr->previousLeaf(lastLeaf);
				if (firstLeaf) {
					firstLeaf = false;
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

		constructTreeFromLeafToRoot<false>(upperNodes);
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
}
