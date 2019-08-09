#pragma once
#include <vector> // for vector
#include "Elements.hpp"

namespace btree {
#define NODE_TEMPLATE template <typename Key, typename Value, uint16_t BtreeOrder>
#define BASE NodeBase<Key, Value, BtreeOrder>

	NODE_TEMPLATE
	class NodeBase {
	public:
		using Ele      = Elements<Key, Value, BtreeOrder, NodeBase>;
		using LessThan = typename Ele::LessThan;

		template <typename Iter>
		NodeBase(Iter, Iter, shared_ptr<LessThan>);
		NodeBase(const NodeBase&);
		NodeBase(NodeBase&&) noexcept;
		virtual unique_ptr<NodeBase> clone() const = 0;
		virtual ~NodeBase() = default;

		inline bool        middle() const;
		inline Key         maxKey() const;
		inline vector<Key> allKey() const;
		inline bool        have  (const Key&, vector<NodeBase*>&);
		inline bool        have  (const Key&) const;
		Value*             search(const Key&);
		void               add   (pair<Key, Value>, vector<NodeBase*>&);
		bool               remove(const Key&);
		void               searchSiblingsIn(vector<NodeBase*>&, NodeBase*&, NodeBase*&) const;

	protected:
		Elements<Key, Value, BtreeOrder, NodeBase> elements_;

		uint16_t      childCount() const;
		inline bool   full()  const;
		inline bool   empty() const;
		inline void   changeInSearchDownPath(const Key&, const Key&);
		inline Value* searchWithSaveTrack(const Key&, vector<NodeBase*>&);

		template <typename T>
		void doAdd(pair<Key, T>, vector<NodeBase*>&);
		template <typename T>
		void reallocateSiblingElement(bool, NodeBase*, vector<NodeBase*>&, pair<Key, T>);
		void changeMaxKeyIn(vector<NodeBase*>&, const Key&) const;
		void replacePreviousNodeMaxKeyInTreeBySearchUpIn(vector<NodeBase*>&, const Key&, const Key&);
		template <typename T>
		void splitNode(pair<Key, T>, vector<NodeBase*>&);
		void insertLeafToUpper(unique_ptr<NodeBase>, vector<NodeBase*>&);
		inline bool haveHelper(const Key&, function<bool(NodeBase*, const Key&)>, function<void(NodeBase*)>);

		static bool spaceFreeIn(const NodeBase*);
	};
}

namespace btree {
	using std::make_pair;

	NODE_TEMPLATE
	template <typename Iter>
	BASE::NodeBase(Iter begin, Iter end, shared_ptr<LessThan> lessThanPtr)
		: elements_(begin, end, lessThanPtr)
	{ }

	NODE_TEMPLATE
	BASE::NodeBase(const NodeBase& that)
		: elements_(that.elements_)
	{ }

	NODE_TEMPLATE
	BASE::NodeBase(NodeBase&& that) noexcept
		: elements_(std::move(that.elements_))
	{ }

	NODE_TEMPLATE
	uint16_t
	BASE::childCount() const
	{
		return elements_.count();
	}

	NODE_TEMPLATE
	bool
	BASE::empty() const
	{
		return elements_.count() == 0;
	}	

	NODE_TEMPLATE
	bool
	BASE::full() const
	{
		return elements_.full();
	}

	NODE_TEMPLATE
	bool
	BASE::middle() const
	{
		return elements_.MiddleFlag;
	}

	NODE_TEMPLATE
	Key
	BASE::maxKey() const
	{
		return elements_[elements_.count() - 1].first;
	}

	NODE_TEMPLATE
	bool
	BASE::haveHelper(
		const Key& key,
		function<bool(NodeBase*, const Key&)> lessThanAndMiddleHandler,
		function<void(NodeBase*)>             keyBeyondMaxHandler
		)
	{
		auto& lessThan = *(elements_.LessThanPtr);

		for (auto& e : elements_) {
			if (lessThan(key, e.first)) {
				if (middle()) {
					return lessThanAndMiddleHandler(Ele::ptr(e.second), key);
				} else {
					return false;
				}
			} else if (!lessThan(e.first, key)) {
				return true;
			}
		}

		keyBeyondMaxHandler(this);
		
		return false;
	}

	NODE_TEMPLATE
	bool
	BASE::have(const Key& key, vector<NodeBase*>& passedNodeTrackStack)
	{
		auto& stack = passedNodeTrackStack;
		stack.push_back(this);

		// these function will be constructed repeatedly, will have some bad effect?
		function<bool(NodeBase*, const Key&)> callSelf = [&] (NodeBase* node, const Key& key) {
			return node->have(key, stack);
		};
		function<void(NodeBase*)> collectMaxDeep = [&] (const NodeBase* node) {
			auto maxIndex = node->childCount() - 1;
			auto maxChildPtr = Ele::ptr(node->elements_[maxIndex].second);
			stack.push_back(maxChildPtr);

			if (node->middle()) {
				collectMaxDeep(maxChildPtr);
			}
		};
		
		return haveHelper(key, callSelf, collectMaxDeep);
	}

	NODE_TEMPLATE
	bool
	BASE::have(const Key& key) const
	{
		auto callSelf = [&] (const NodeBase* node, const Key& key) -> bool {
			return node->have(key);
		};
		auto doNothing = [] (auto) { };
		return const_cast<NodeBase*>(this)->haveHelper(key, callSelf, doNothing);
	}

	NODE_TEMPLATE
	Value*
	BASE::search(const Key& key)
	{
		auto maxValueForContent = [] (Ele& e) -> typename Ele::ValueForContent& {
			return e[e.count() - 1].second;
		};

		if (!middle()) {
			return elements_.have(key) ? &Ele::value_Ref(elements_[key]) : nullptr;
		} else {
			for (auto& e : elements_) {
				if ((*elements_.LessThanPtr)(key, e.first)) {
					return Ele::ptr(e.second)->search(key);
				} else if (!(*elements_.LessThanPtr)(e.first, key)) {
					auto subNodePtr = Ele::ptr(e.second);

				SearchInThisNode:
					if (!subNodePtr->middle()) {
						return &Ele::value_Ref(maxValueForContent(subNodePtr->elements_));
					} else {
						subNodePtr = Ele::ptr(maxValueForContent(subNodePtr->elements_));
						goto SearchInThisNode;
					}
				}
			}

			return nullptr;
		}
	}

	NODE_TEMPLATE
	vector<Key>
	BASE::allKey() const
	{
		vector<Key> keys;
		keys.reserve(elements_.count());
		for (auto& e : elements_) {
			keys.emplace_back(e.first);
		}

		return keys;
	}

	NODE_TEMPLATE
	void
	BASE::add(pair<Key, Value> p, vector<NodeBase*>& passedNodeTrackStack)
	{
        // 模板参数匹配顺序
		doAdd(p, passedNodeTrackStack);
	}

	template <typename Key, typename Value, uint16_t BtreeOrder, typename T>
	void
	getSiblings(BASE*, vector<BASE*>, BASE*&, BASE*&);

	NODE_TEMPLATE
	template<typename T>
	void
	BASE::doAdd(pair<Key, T> p, vector<BASE*>& passedNodeTrackStack)
	{
		// TODO need use std::move p or not?
		auto& k = p.first;
		auto& stack = passedNodeTrackStack;
		auto& lessThan = *(elements_.LessThanPtr);

		// some doesn't have one of siblings

		BASE *previous = nullptr, *next = nullptr;
		getSiblings<Key, Value, BtreeOrder, T>(this, stack, previous, next);

		if (!full()) {
			if (lessThan(k, maxKey())) {
				elements_.insert(std::move(p));
			} else {
				elements_.append(std::move(p));
				changeMaxKeyIn(stack, k);
			}
		} else if (spaceFreeIn(previous)) {
			reallocateSiblingElement(true, previous, stack, std::move(p));
		} else if (spaceFreeIn(next)) {
			reallocateSiblingElement(false, next, stack, std::move(p));
		} else {
			splitNode(std::move(p), stack);
		}
	}

	NODE_TEMPLATE
	bool
	BASE::remove(const Key& key)
	{
		auto maxValue = [] (Ele& e) -> typename Ele::ValueForContent& {
			return e[e.count() - 1].second;
		};

		// TODO wrong
		// Keep internal node key count between w/2 and w
		if (!middle()) {
			elements_.remove(key);
			// check count
		} else {
			for (auto& e : elements_) {
				if ((*elements_.LessThanPtr)(key, e.first)) {
					// Recursive locate
				} else if (!(*elements_.LessThanPtr)(e.first, key)) {
					auto subNodePtr = Ele::ptr(e.second);

				SearchInThisNode:
					if (!subNodePtr->middle()) {
						subNodePtr->elements_.remove(key);
						// Check count
					} else {
						subNodePtr = Ele::ptr(maxValue(subNodePtr->elements_));
						goto SearchInThisNode;
					}
				}
			}
		}

		return false; // TODO wait to modify
	}


	NODE_TEMPLATE
	void
	BASE::changeInSearchDownPath(const Key& oldKey, const Key& newKey)
	{
		vector<NodeBase*> trackStack{};

		searchWithSaveTrack(oldKey, trackStack);

		auto rend = trackStack.rend();
		for (auto rIter = trackStack.rbegin(); rIter != rend; ++rIter) {
			auto& node = *rIter;

			auto i = node->elements_.indexOf(oldKey);
			if (i != -1 && node->middle()) { // No need to change leaf
				node->elements_[i].first = newKey;
			}
		}

	}

	NODE_TEMPLATE
	Value*
	BASE::searchWithSaveTrack(const Key& key, vector<NodeBase*>& trackStack)
	{
		// TODO modify, or think of search
		// TODO maybe not need return value
		trackStack.push_back(this);

		auto maxValueForContent = [] (Ele& e) -> typename Ele::ValueForContent& {
			return e[e.count() - 1].second;
		};

		if (!middle()) {
			return elements_.have(key) ? &Ele::value_Ref(elements_[key]) : nullptr;
		} else {
			for (auto& e : elements_) {
				if ((*elements_.LessThanPtr)(key, e.first)) {
					return Ele::ptr(e.second)->searchWithSaveTrack(key, trackStack);
				} else if (!(*elements_.LessThanPtr)(e.first, key)) {
					auto subNodePtr = Ele::ptr(e.second);

				SearchInThisNode:
					trackStack.push_back(subNodePtr);

					if (!subNodePtr->middle()) {
						return &Ele::value_Ref(maxValueForContent(subNodePtr->elements_));
					} else {
						subNodePtr = Ele::ptr(maxValueForContent(subNodePtr->elements_));
						goto SearchInThisNode;
					}
				}
			}

			return nullptr;
		}
	}

	NODE_TEMPLATE
	void
	BASE::searchSiblingsIn(vector<NodeBase*>& passedNodeTrackStack, NodeBase*& previous, NodeBase*& next) const
	{
		// TODO
	}

	NODE_TEMPLATE
	template <typename T>
	void
	BASE::reallocateSiblingElement(bool isPrevious, NodeBase* sibling, vector<NodeBase*>& passedNodeTrackStack, pair<Key, T> p)
	{
		auto& stack = passedNodeTrackStack;
		
		if (isPrevious) {
			auto min = elements_.exchangeMin(std::move(p));
			auto previousOldMax = sibling->maxKey();
			sibling->elements_.append(std::move(min));
			auto newMaxKey = min.first;
			// previous max change
			// TODO could use this stack to get sibling stack, then change upper
			replacePreviousNodeMaxKeyInTreeBySearchUpIn(stack, previousOldMax, newMaxKey);
		} else {
			auto&& max = elements_.exchangeMax(std::move(p));
			// TODO why max not be used
			// this max change
			changeMaxKeyIn(stack, maxKey());
			sibling->elements_.insert(std::move(p));
		}
	}

	NODE_TEMPLATE
	void
	BASE::changeMaxKeyIn(vector<NodeBase*>& passedNodeTrackStack, const Key& maxKey) const
	{
		auto& stack = passedNodeTrackStack;
		if (stack.size() < 2) {
			return;
		}

		auto  nodePtr = stack.back();
		stack.pop_back();
		auto& upperNode = *stack.back();
		// 以下这个修改 key 的部分可以复用
		auto  matchIndex = upperNode.elements_.indexOf(nodePtr);
		(upperNode.elements_)[matchIndex].first = maxKey;

		auto maxIndex = upperNode.childCount() - 1;
		if (matchIndex == maxIndex) {
			changeMaxKeyIn(stack, maxKey);
		}
	}

	// use stack to get root node(maybe not need root node), then use this early max key to search
	NODE_TEMPLATE
	void
	BASE::replacePreviousNodeMaxKeyInTreeBySearchUpIn(vector<NodeBase*>& passedNodeTrackStack, const Key& oldKey, const Key& newKey)
	{
		auto& stack = passedNodeTrackStack;
		auto emitUpperNode = [&] {
			auto last = stack.back();
			stack.pop_back();

			return last;
		};

		emitUpperNode(); // top pointer is leaf, it's useless
		

		for (NodeBase* node = emitUpperNode(); stack.size() != 0; node = emitUpperNode()) {
			// judge if the node is the same ancestor between this and previous node
			if (node->have(oldKey)) {
				node->changeInSearchDownPath(oldKey, newKey);

				// judge if need to change upper node
				auto i = node->elements_.indexOf(oldKey); // should create a method called this layer search
				auto maxIndex = childCount() - 1;
				if (i != -1 && i == maxIndex) {
					stack.push_back(node);
					// change upper node
					changeMaxKeyIn(stack, newKey);
					break;
				}
			}
		}
	}

	template <typename Key, typename Value, uint16_t BtreeOrder, typename T>
	void
	setSiblings(BASE*, BASE*);

	NODE_TEMPLATE
	template <typename T>
	void
	BASE::splitNode(pair<Key, T> p, vector<NodeBase*>& passedNodeTrackStack)
	{
		auto& key = p.first;
		auto& lessThan = *(elements_.LessThanPtr);
		auto& stack = passedNodeTrackStack;

		auto newPre = this->clone(); // TODO have a problem
		auto newPrePtr = newPre.get();
		// left is newPre, right is this
		setSiblings<Key, Value, BtreeOrder, T>(newPrePtr, this);

		auto i = elements_.suitablePosition(key);

		// [] not need reference if don't have last sentence?
		auto moveItems = [&] (uint16_t preNodeRemoveCount) {
			newPrePtr->elements_.removeItems(false, preNodeRemoveCount);
			this->elements_.removeItems(true, BtreeOrder - preNodeRemoveCount);
		};

		auto addIn = [&] (NodeBase* node, bool shouldAppend) {
			if (shouldAppend) {
				node->elements_.append(std::move(p));
			} else {
				node->elements_.insert(std::move(p));
			}
		};

#define HANDLE_ADD(leaf, maxBound) auto shouldAppend = (i == maxBound); do { addIn(leaf, shouldAppend); } while(0)

		constexpr bool odd = BtreeOrder % 2;
		constexpr auto middle = odd ? (BtreeOrder / 2 + 1) : (BtreeOrder / 2);
		if (i <= middle) {
			constexpr auto removeCount = middle;
			moveItems(removeCount);

			HANDLE_ADD(newPrePtr, middle);
		} else {
			constexpr auto removeCount = BtreeOrder - middle;
			moveItems(removeCount);

			HANDLE_ADD(this, BtreeOrder);
			if (shouldAppend) {
				auto stackCopy = stack;
				changeMaxKeyIn(stackCopy, key);
			}
		}
		
#undef HANDLE_ADD

		insertLeafToUpper(std::move(newPre), stack);
	}

	NODE_TEMPLATE
	void
	BASE::insertLeafToUpper(unique_ptr<NodeBase> node, vector<NodeBase*>& passedNodeTrackStack)
	{
#define EMIT_UPPER_NODE() stack.pop_back()

		// reduce useless node, prepare to upper level add, like start new leaf add
		auto& stack = passedNodeTrackStack;
		EMIT_UPPER_NODE();
		
		auto pair = make_pair<Key, unique_ptr<NodeBase>>(node->maxKey(), std::move(node));
		doAdd(std::move(pair), stack);
		// 这部分甚至可能动 root，做好心理准备

#undef EMIT_UPPER_NODE
	}

	NODE_TEMPLATE
	bool
	BASE::spaceFreeIn(const BASE* node)
	{
		if (node != nullptr) {
			return !node->full();
		}

		return false;
	}


#undef BASE
#undef NODE_TEMPLATE
}
