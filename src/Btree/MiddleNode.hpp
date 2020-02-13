#pragma once
#include <memory>
#include <functional>
#include "Basic.hpp"
#include "EnumeratorPipeline.hpp"
#include "Elements.hpp"
#include "NodeBase.hpp"
#include "LeafNode.hpp"
#include "NodeAddRemoveCommon.hpp"

namespace Collections 
{
	using ::std::move;
	using ::std::unique_ptr;
	using ::std::cref;
	using ::std::reference_wrapper;
	using ::std::pair;
	using ::std::make_pair;
	using ::std::bind;
	using ::std::function;
	using ::std::placeholders::_1;

	template <typename Key, typename Value, order_int BtreeOrder>
	class MiddleNode : public NodeBase<Key, Value, BtreeOrder>
	{
	private:
		function<MiddleNode*(MiddleNode*)> _queryPrevious;
		function<MiddleNode*(MiddleNode*)> _queryNext;
		using Base = NodeBase<Key, Value, BtreeOrder>;
		using _LessThan = LessThan<Key>;
		Elements<reference_wrapper<Key const>, unique_ptr<Base>, BtreeOrder, _LessThan> _elements;

	public:
		bool Middle() const override { return true; }

		MiddleNode(shared_ptr<_LessThan> lessThanPtr)
			: Base(), _elements(lessThanPtr)
		{
			SetSubNode();
		}	

		template <typename Iterator>
		MiddleNode(Enumerator<unique_ptr<Base>&, Iterator> enumerator, shared_ptr<_LessThan> lessThanPtr)
			: Base(), _elements(EnumeratorPipeline<unique_ptr<Base>&, typename decltype(_elements)::Item>(enumerator, bind(&MiddleNode::ConvertToKeyBasePtrPair, _1)), lessThanPtr)
		{
			SetSubNode();
		}

		MiddleNode(MiddleNode const& that) : Base(that), _elements(CloneElements())
		{ }

		MiddleNode(MiddleNode&& that) noexcept : Base(move(that)), _elements(move(that._elements))
		{ }

		~MiddleNode() override = default;

		unique_ptr<Base> Clone() const override
		{
			// _queryPrevious how to process
			auto cloneOne = make_unique<MiddleNode>(_elements.LessThanPtr);
			for (auto const& e : _elements)
			{
				auto subCloned = e.second->Clone();
				cloneOne->_elements.Append(ConvertToKeyBasePtrPair(subCloned));
			}

			return move(cloneOne);
		}

		decltype(_elements) CloneElements() const
		{
			decltype(_elements) thatElements(_elements.LessThanPtr);
			for (auto const& e : _elements)
			{
				auto ptr = e.second->Clone();
				// TODO code below maybe compile error
				thatElements.Add(cref(ptr->Minkey()), move(ptr));
			}

			return move(thatElements);
		}

		vector<Key> Keys() const override
		{
			return MinSon()->Keys();
		}

		Key const& MinKey() const override
		{
			return _elements[0].first;
		}

#define SELECT_BRANCH(KEY) auto i = _elements.SelectBranch(KEY)
		bool ContainsKey(Key const& key) const override
		{
			SELECT_BRANCH(key);
			return _elements[i].second->ContainsKey(key);
		}

		Value GetValue(Key const& key) const override
		{
			SELECT_BRANCH(key);
			return _elements[i].second->GetValue(key);
		}

		void ModifyValue(Key const& key, Value value) override
		{
			SELECT_BRANCH(key);
			_elements[i].second->ModifyValue(key, move(value));
		}

		void Add(pair<Key, Value> p) override
		{
			SELECT_BRANCH(p.first);
			_elements[i].second->Add(move(p));
			// TODO why up code prefer global []
		}

		void Remove(Key const& key) override
		{
			SELECT_BRANCH(key);
			_elements[i].second->Remove(key);
		}
#undef SELECT_BRANCH

	private:
		Base* MinSon() const { return _elements[0].second.get(); }

		void AddSubNodeCallback(Base* srcNode, unique_ptr<Base> newNextNode)
		{
			//auto predicate = [srcNode = srcNode]((typename (decltype _elements)::Item const&) item)
			// TODO why up is a compile error
			// auto predicate = [srcNode = srcNode](auto& item)
			// {
			// 	if (item.second.get() == srcNode)
			// 	{
			// 		return true;
			// 	}

			// 	return false;
			// };
			
			// find index of srcNode and add new NextNode
			// if this is Full(), combine the node or call the upper node callback
			if (!_elements.Full())
			{
				_elements.Emplace(_elements.IndexKeyOf(srcNode->MinKey()), { cref(newNextNode->MinKey()), move(newNextNode) });
				return;
			}

			auto _next = _queryNext(this);
			auto _previous = _queryPrevious(this);
			typename decltype(_elements)::Item p{ cref(newNextNode->MinKey()), move(newNextNode) };
			ADD_COMMON(false);
		}

		using Leaf = LeafNode<Key, Value, BtreeOrder>;
#define LEF_CAST(NODE) static_cast<Leaf *>(NODE)
		void DeleteSubNodeCallback(Base* node)
		{
			auto i = _elements.IndexKeyOf(node->MinKey());
			if (!node->Middle())
			{
				auto leafNode = LEF_CAST(node);
				// Fresh the _next of LeafNode
				if (i != 0)
				{
					LEF_CAST(_elements[i - 1].second.get())->NextLeaf(leafNode->NextLeaf());
				}

				// Fresh the _previous of LeafNode
				if (i != _elements.Count() - 1)
				{
					LEF_CAST(_elements[i + 1].second.get())->PreviousLeaf(leafNode->PreviousLeaf());
				}
			}

			_elements.RemoveAt(i);
			// Below two variables is to macro
			auto _next = _queryNext(this);
			auto _previous = _queryPrevious(this);
			REMOVE_COMMON;
		}

#define MID_CAST(NODE) static_cast<MiddleNode *>(NODE)
		// For sub node call
		MiddleNode* QueryPrevious(MiddleNode* subNode)
		{
			auto i = _elements.IndexKeyOf(subNode->MinKey());
			return i != 0 ? MID_CAST(_elements[i - 1].second.get()) : nullptr;
		}

		// For sub node call
		MiddleNode* QueryNext(MiddleNode* subNode)
		{
			auto i = _elements.IndexKeyOf(subNode->MinKey());
			return (i != _elements.Count() - 1) ? MID_CAST(_elements[i + 1].second.get()) : nullptr;
		}

		void SetSubNode()
		{
			using ::std::placeholders::_1;
			using ::std::placeholders::_2;
			auto f1 = bind(&MiddleNode::AddSubNodeCallback, this, _1, _2);
			auto f2 = bind(&MiddleNode::DeleteSubNodeCallback, this, _1);
			Leaf* lastLeaf = nullptr;
			for (auto& e : _elements)
			{
				auto& node = e.second;
				node->SetUpNodeCallback(f1, f2);
				if (node->Middle())
				{
					MID_CAST(node.get())->_queryNext = bind(&MiddleNode::QueryNext, this, _1);
					MID_CAST(node.get())->_queryPrevious = bind(&MiddleNode::QueryPrevious, this, _1);
				}
				else
				{
					auto nowLeaf = LEF_CAST(node.get());
					nowLeaf->PreviousLeaf(lastLeaf);
					if (lastLeaf != nullptr)
					{
						lastLeaf->NextLeaf(nowLeaf);
					}

					lastLeaf = nowLeaf;
				}
				
			}
		}
#undef MID_CAST		
#undef LEF_CAST

		static typename decltype(_elements)::Item ConvertToKeyBasePtrPair(unique_ptr<Base>& node)
		{
			using pairType = typename decltype(_elements)::Item;
			return make_pair<pairType::first_type, pairType::second_type>(cref(node->MinKey()), move(node));
		}
	};
}