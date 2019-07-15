#pragma once
#include <vector> // for vector
#include "Elements.hpp"

namespace btree {
    struct LeafFlag   {};
    struct MiddleFlag {};

#define NODE_BASE_TEMPLATE template <typename Key, typename Value, uint16_t BtreeOrder>

    NODE_BASE_TEMPLATE
    class NodeBase {
    public:
		using Ele      = Elements<Key, Value, BtreeOrder, NodeBase>;
		using LessThan = typename Ele::LessThan;
        const bool Middle;

        template <typename Iter>
        NodeBase(LeafFlag,   Iter, Iter, shared_ptr<LessThan>);
        template <typename Iter>
        NodeBase(MiddleFlag, Iter, Iter, shared_ptr<LessThan>);
        NodeBase(const NodeBase&);
        NodeBase(NodeBase&&) noexcept;
        virtual unique_ptr<NodeBase> clone() const = 0;
        virtual ~NodeBase() = default;

        inline Key         maxKey();
        inline bool        have(const Key&);
        Value*             search(const Key&);
        inline vector<Key> allKey() const;
        virtual NodeBase*  father() const;
		uint16_t           childCount() const;
		inline bool        empty() const;
		inline bool        full() const;
		virtual bool       add(pair<Key, Value>&&);
		bool               remove(const Key&);

    protected:
        NodeBase* father_{ nullptr };
        Elements<Key, Value, BtreeOrder, NodeBase> elements_;
    };
}

namespace btree {
#define NODE NodeBase<Key, Value, BtreeOrder>

	NODE_BASE_TEMPLATE
    template <typename Iter>
    NODE::NodeBase(LeafFlag, Iter begin, Iter end, shared_ptr<LessThan> lessThanPtr)
        : Middle(false), elements_(begin, end, lessThanPtr)
    { }

	NODE_BASE_TEMPLATE
    template <typename Iter>
    NODE::NodeBase(MiddleFlag, Iter begin, Iter end, shared_ptr<LessThan> funcPtr)
        : Middle(true), elements_(begin, end, funcPtr)
    {
		for (; begin != end; ++begin) {
            // need to use SFINE to control the property below?
            // TODO error
            begin->second->father_ = this;
        }
    }

    NODE_BASE_TEMPLATE
    NODE::NodeBase(const NodeBase& that)
        : Middle(that.Middle), elements_(that.elements_)
    { }

    NODE_BASE_TEMPLATE
	NODE::NodeBase(NodeBase&& that) noexcept
		: Middle(that.Middle), father_(that.father_), elements_(std::move(that.elements_))
	{ }


	NODE_BASE_TEMPLATE
    NODE*
    NODE::father() const
    {
    	return father_;
    }

    NODE_BASE_TEMPLATE
	uint16_t
    NODE::childCount() const
    {
		return elements_.count();
    }

	NODE_BASE_TEMPLATE
	bool
	NODE::empty() const
	{
		return elements_.count() == 0;
	}

	NODE_BASE_TEMPLATE
	bool
	NODE::full() const
	{
		return elements_.full();
	}

	NODE_BASE_TEMPLATE
    Key
    NODE::maxKey()
    {
        return elements_[elements_.count() - 1].first;
    }

    NODE_BASE_TEMPLATE
    bool
    NODE::have(const Key& key)
    {
		// a little duplicate to search, but not very
		if (!Middle) {
			return elements_.have(key);
		} else {
			for (auto& e : elements_) {
				if ((*elements_.LessThanPtr)(key, e.first)) {
					Ele::ptr(e.second)->have(key);
				} else if (!(*elements_.LessThanPtr)(e.first, key)) {
					return true;
				}
			}

			return false;
		}
    }

    NODE_BASE_TEMPLATE
	Value*
	NODE::search(const Key& key)
	{
		if (!Middle) {
			if (elements_.have(key)) {
				return &Ele::value(elements_[key]);
			} else {
				return nullptr;
			}
		} else {
			for (auto& e : elements_) {
				if ((*elements_.LessThanPtr)(key, e.first)) {
					return Ele::ptr(e.second)->search(key);
				} else if (!(*elements_.LessThanPtr)(e.first, key)) {
					auto subNodePtr = Ele::ptr(e.second);

				SearchInThisNode:
					if (!subNodePtr->Middle) {
						return &Ele::value(subNodePtr->elements_.max());
					} else {
						subNodePtr = Ele::ptr(subNodePtr->elements_.max());
						goto SearchInThisNode;
					}
				}
			}

			return nullptr;
		}
	}

    NODE_BASE_TEMPLATE
    vector<Key>
    NODE::allKey() const
    {
	    vector<Key> keys;
	    keys.reserve(elements_.count());
	    for (auto& e : elements_) {
	        keys.emplace_back(e.first);
	    }

	    return keys;
    }

    NODE_BASE_TEMPLATE
    unique_ptr<NODE>
    NODE::clone() const
    {
	    throw runtime_error("NodeBase's clone is invalid.");
    }

    NODE_BASE_TEMPLATE
    bool
    NODE::remove(const Key& key)
    {
	    // TODO
    }

#undef NODE
#undef NODE_BASE_TEMPLATE
}
