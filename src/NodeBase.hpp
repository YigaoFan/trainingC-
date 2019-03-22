#pragma once

#include <vector>
#include "Elements.hpp" // for Elements

namespace btree {
    struct leaf_type {};
    struct middle_type {};

    // maybe template arg can be less, like BtreeType maybe reduce
    template <typename Key, typename Value, unsigned BtreeOrder, typename BtreeType>
    class NodeBase {
    public:
        const bool middle;
        BtreeType& btree; // TODO should replaced by compare_function&, maybe btree has not one function

        template <typename Iter>
        NodeBase(BtreeType&, leaf_type, Iter, Iter);
        template <typename Iter>
        NodeBase(BtreeType&, middle_type,Iter, Iter);
        virtual ~NodeBase();

        inline Key max_key() const;

        inline bool have(const Key&) const;
        inline std::vector<Key> all_key() const;

    protected:
        using Ele = Elements<Key, Value, BtreeOrder, NodeBase>;
        NodeBase(const NodeBase&, BtreeType&, leaf_type);
        NodeBase(const NodeBase&, middle_type);
        virtual NodeBase* Clone(BtreeType&) const = 0;
        void father(NodeBase*);

    private:
        NodeBase* father_{ nullptr };
        Elements<Key, Value, BtreeOrder, NodeBase> elements_;

        Ele CloneElements() const;
    };
}

// implementation
namespace btree {
    template <typename Key, typename Value, unsigned BtreeOrder, typename BtreeType>
    template <typename Iter>
    NodeBase<Key, Value, BtreeOrder, BtreeType>::NodeBase(BtreeType& btree, leaf_type, Iter begin, Iter end)
        : middle(false), btree(btree), elements_(begin, end)
    {
        // null
    }

    template <typename Key, typename Value, unsigned BtreeOrder, typename BtreeType>
    template <typename Iter>
    NodeBase<Key, Value, BtreeOrder, BtreeType>::NodeBase(BtreeType& btree, middle_type, Iter begin, Iter end)
        : middle(true), btree(btree), elements_(begin, end)
    {
        for (; begin != end; ++begin) {
            // need to use SFINE to control the property below?
            begin->second->father_ = this;
        }
    }

    template <typename Key, typename Value, unsigned BtreeOrder, typename BtreeType>
    NodeBase<Key, Value, BtreeOrder, BtreeType>::NodeBase(const NodeBase& that, BtreeType& tree, leaf_type)
        : middle(that.middle), btree(tree), elements_(that.elements_)
    {
        // null
    }

    template <typename Key, typename Value, unsigned BtreeOrder, typename BtreeType>
    NodeBase<Key, Value, BtreeOrder, BtreeType>::NodeBase(const NodeBase& that, middle_type)
        : middle(that.middle), btree(/* TODO */), elements_(std::move(that.CloneElements()))
    {                          // can solved by pass a para to Clone
        // null
    }

    template <typename Key, typename Value, unsigned BtreeOrder, typename BtreeType>
    void
    NodeBase<Key, Value, BtreeOrder, BtreeType>::father(NodeBase* father)
    {
    	father_ = father;
    }

    template <typename Key, typename Value, unsigned BtreeOrder, typename BtreeType>
    NodeBase<Key, Value, BtreeOrder, BtreeType>::~NodeBase() = default;

    template <typename Key, typename Value, unsigned BtreeOrder, typename BtreeType>
    Key
    NodeBase<Key, Value, BtreeOrder, BtreeType>::max_key() const
    {
        elements_.max_key();
    }

    template <typename Key, typename Value, unsigned BtreeOrder, typename BtreeType>
    bool
    NodeBase<Key, Value, BtreeOrder,BtreeType>::have(const Key& key) const
    {
        return elements_.have(key);
    }

    template <typename Key, typename Value, unsigned BtreeOrder, typename BtreeType>
    std::vector<Key>
    NodeBase<Key, Value, BtreeOrder, BtreeType>::all_key() const
    {
        return elements_.all_key();
    }

    template <typename Key, typename Value, unsigned BtreeOrder, typename BtreeType>
    typename NodeBase<Key, Value, BtreeOrder, BtreeType>::Ele
    NodeBase<Key, Value, BtreeOrder, BtreeType>::CloneElements() const
    {
    	Ele res{ Ele() }; // TODO don't have this default constructor

        if (middle) { // depend on middle or leaf
            for (auto& e : elements_) {
                res.add(e.first, e.second->Clone());
            }
        } else {
        	res.adds(elements_);
        }

        return res; // may be can use std::move?
    }
}
