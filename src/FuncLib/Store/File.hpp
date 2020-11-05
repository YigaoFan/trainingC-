#pragma once
#include <filesystem>
#include <memory>
#include <set>
#include <utility>
#include "../../Basic/TypeTrait.hpp"
#include "StaticConfig.hpp"
#include "FileCache.hpp"
#include "FileReader.hpp"
#include "ObjectBytes.hpp"
#include "StorageAllocator.hpp"
#include "../FriendFuncLibDeclare.hpp"// 因为 DiskPos 里面有功能依赖 File，所以这里只能 ByteConverter 的声明
#include "IWriterConcept.hpp"
#include "ObjectRelationTree.hpp"

namespace FuncLib::Store
{
	using ::std::enable_shared_from_this;
	using ::std::make_shared;
	using ::std::move;
	using ::std::pair;
	using ::std::set;
	using ::std::shared_ptr;
	using ::std::filesystem::exists;
	using ::std::filesystem::path;

	template <typename... Ts>
	struct TypeList;

	template <typename T>
	struct TypeList<T>
	{
		static constexpr bool IsLast = true;
		using Current = T;
	};

	template <typename T, typename... Ts>
	struct TypeList<T, Ts...>
	{
		static constexpr bool IsLast = false;
		using Current = T;
		using Remain = TypeList<Ts...>;
	};

	template <template <typename> class Handler, typename... Ts>
	auto ForEach(TypeList<Ts...> typelist)
	{
		using T = Handler<typename decltype(typelist)::Current>;
		// if (handler(T)) return type should here

		if constexpr (not typelist.IsLast)
		{
			ForEach<Handler>(typelist.Remain());
		}
	}

	// template <typename T>
	// using SearchRoutine = TypeList<NodeBase<T>, MiddleNode<T>, LeafNode<T>>;
	/// 一个路径仅有一个 File 对象，这里的功能大部分是提供给模块外部使用的
	/// 那这里就有点问题了，Btree 内部用这个是什么功能
	/// 这样使用场景是不是就不干净了？指的是内外都用。
	/// 不一定，因为使用方那里还不完善，给那边先保留着这么大的能力吧
	class File : public enable_shared_from_this<File>
	{
	private:
		static set<File*> Files;

		shared_ptr<path> _filename;
		FileCache _cache;
		StorageAllocator _allocator;
		set<pos_lable> _toDeallocateLables;// 之后可以基于这个调整文件大小
		ObjectRelationTree _objRelationTree;
	public:
		static shared_ptr<File> GetFile(path const& filename);
		/// below for make_shared use in File class only
		File(unsigned int fileId, path filename, StorageAllocator allocator, ObjectRelationTree relationTree);
		File(File&& that) noexcept = delete;
		File(File const& that) = delete;

		shared_ptr<path> Path() const;
		~File();

		template <typename T>
		shared_ptr<T> Read(pos_lable posLable)
		{
			// TODO
			// if constexpr (T == NodeBase) use a type list to store these inherited type
			// {
			// 	Read LeafNode
			// }
			// else
			// {
			// 	Read MiddleNode
			// }
			// New 的时候要用本来的类型，而不要用动态类型
			
			if (_cache.Cached<T>(posLable))
			{
				return _cache.Read<T>(posLable);
			}
			else
			{
				return AddToCache(ReadOn<T>(posLable), posLable);
			}
		}

		template <typename T>
		pair<pos_lable, shared_ptr<T>> New(T t)
		{
			auto lable = _allocator.AllocatePosLable();
			auto obj = AddToCache(move(t), lable);
			return { lable, obj };
		}

		template <typename T>
		pair<pos_lable, shared_ptr<T>> New(shared_ptr<T> t)
		{
			auto lable = _allocator.AllocatePosLable();
			auto obj = AddToCache(move(t), lable);
			return { lable, obj };
		}

		/// 使用这个方法进行存储只能是最外层的对象，比如用在 OuterDiskPtr 这样
		template <typename T>
		void Store(pos_lable posLable, shared_ptr<T> const& object)
		{
			constexpr ofstream::openmode openmode = ofstream::binary | ofstream::in | ofstream::out;
			ofstream fs{*_filename, openmode };
			auto allocate = [&](ObjectBytes* bytes)
			{
				auto size = bytes->Size();
				auto start = _allocator.GiveSpaceTo(bytes->Lable(), size);// 不用返回 start 了
			};

			auto resize = [&](ObjectBytes* bytes)
			{
				auto start = _allocator.ResizeSpaceTo(bytes->Lable(), bytes->Size()); // 不用返回 start 了
			};

			auto write = [&](ObjectBytes* bytes)
			{
				// 如何合并所有写入 TODO
				auto start = _allocator.GetConcretePos(bytes->Lable());
				bytes->WriteIn(&fs, start);
			};

			_toDeallocateLables.erase(posLable);

			// 这里的 SizeStable 不考虑指针指向的对象，牵涉到 ByteConverter<DiskPtr> 和这下面的 if
			auto isNew = not _allocator.Ready(posLable);
			WriteQueue toWrites;
			AllocateSpaceQueue toAllocates;
			ResizeSpaceQueue toResize;
			ObjectBytes bytes{ posLable, &toWrites, &toAllocates, &toResize };
			ProcessStore(posLable, object, &bytes);

			// Ensure file exist before write
			if (!exists(*_filename)) { ofstream f(*_filename); }

			toResize | resize | write; // Resize first, then allocate. Below allocates can reuse place.
			toAllocates | allocate | write;
			toWrites | write;

			if (isNew)
			{
				_objRelationTree.Add(&bytes);
			}
			else
			{
				_objRelationTree.UpdateWith(&bytes, [](pos_lable) {});
			}
		}

		// 还有没有读的部分，在写入的时候要怎么处理
		// inner 如果大小固定可以和上层放在一块，不固定就可以隔离出去

		template <typename T>
		void StoreInner(pos_lable posLable, shared_ptr<T> const& object, IWriter auto* parentWriter)
		{
			_toDeallocateLables.erase(posLable);// 这个操作在有 inner 的情况下对不对 TODO

			auto parent = parentWriter;
			auto bytes = new ObjectBytes(posLable, parent->ToWrites, parent->ToAllocates, parent->ToResize);
			parentWriter->AddSub(bytes);
			ProcessStore(posLable, object, bytes);
		}

		template <typename T>
		void Delete(pos_lable posLable, shared_ptr<T> object) // 这个模仿 delete 这个接口，但暂不处理 object
		{
			// delete related sub pos_lable TODO
			// also related to toBeDeleteLables
			// Store 那里同样要做这个工作
			_objRelationTree.DeleteTopLevelObj(posLable, [&](pos_lable lable)
			{
				_toDeallocateLables.erase(lable);
				_allocator.DeallocatePosLable(lable);
			});

			_cache.Remove<T>(posLable);
		}

	private:
		template <typename T>
		auto ReadOn(pos_lable posLable)
		{
			// 触发 读 的唯一一个地方
			auto start = _allocator.GetConcretePos(posLable);
			auto reader = FileReader(this, _filename, start);
			return ByteConverter<T>::ReadOut(&reader);
		}

		// 还有 DiskPos 依赖也不是 File 的所有功能，所以可以考虑剥离一部分功能出来形成一个类，来让 DiskPos 依赖
		template <typename T>
		shared_ptr<T> AddToCache(T t, pos_lable posLable)
		{
			_toDeallocateLables.insert(posLable);
			shared_ptr<T> obj = make_shared<T>(move(t));
			_cache.Add<T>(posLable, obj);
			return obj;
		}

		template <typename T>
		shared_ptr<T> AddToCache(shared_ptr<T> obj, pos_lable posLable)
		{
			_toDeallocateLables.insert(posLable);
			_cache.Add<T>(posLable, obj);
			return obj;
		}

		template <typename T>
		void ProcessStore(pos_lable posLable, shared_ptr<T> const& object, ObjectBytes* bytes)
		{
			ByteConverter<T>::WriteDown(*object, bytes);// 这里把 bytes 准备好，这里的 bytes 都是和地址无关的

			// 决定下一步去向
			if (_allocator.Ready(posLable))
			{
				if constexpr (not ByteConverter<T>::SizeStable)
				{
					auto previousSize = _allocator.GetAllocatedSize(posLable);
					auto newSize = bytes->Size();
					if (previousSize < newSize)
					{
						// 加入重分配区
						bytes->ToResizes->Add(bytes);
						return;
					}
				}

				// 加入待写区
				bytes->ToWrites->Add(bytes);
			}
			else
			{
				// 加入待分配区
				bytes->ToAllocates->Add(bytes);
			}
		}
	};
}