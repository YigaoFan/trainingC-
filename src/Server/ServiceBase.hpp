#pragma once
#include <tuple>
#include <memory>
#include <string>
#include <type_traits>
#include "RequestId.hpp"
#include "../Json/JsonConverter/JsonConverter.hpp"
#include "FuncLibWorker.hpp"
#include "Responder.hpp"
#include "../Network/Util.hpp"
#include "AwaiterVoid.hpp"

namespace Server
{
	using Json::JsonObject;
	using Log::ResultStatus;
	using Network::ByteProcessWay;
	using ::std::is_same_v;
	using ::std::shared_ptr;
	using ::std::string;
	using ::std::tuple;
	using ::std::tuple_size_v;

	class ServiceBase
	{
	protected:
		shared_ptr<Socket> _peer;
		FuncLibWorker* _funcLibWorker;
		Responder* _responder;

	public:
		ServiceBase(shared_ptr<Socket> peer, FuncLibWorker* funcLibWorker, Responder* responder)
			: _peer(move(peer)), _funcLibWorker(funcLibWorker), _responder(responder)
		{ }
	
	protected:
		template <typename Return,
				  bool ReturnAdditionalRawByte = false,
				  ByteProcessWay ByteProcessWay = ByteProcessWay::ParseThenDeserial>
		static auto ReceiveFromPeer(Socket* peer)
		{
			return Receive<Return, ReturnAdditionalRawByte, ByteProcessWay>(peer);
		}

		static Void AsyncLoop(auto userLogger, auto callback)
		{
			try
			{
			for (;;)
			{
				// 下面这行是想引发复制操作，复制里面 CurryedAccessLog 里包含的数据，制造出专属于这个 callback 的 userlogger
				// 否则因为这是个 for 循环，一个 userLogger 会在多个 callback BornNewWith，而 BornNewWith 里的调用 CurryedAccessLog
				// 自带 move 性质，这样一些公共信息就只在第一个 callback 看到，后面的 callback 就为空了
				auto l = userLogger;
				co_await callback(&l);
			}
			}
			catch (std::exception const& e)
			{
				printf("Loop has exception %s\n", e.what());
				throw e;
			}
		}

		template <typename Receive, typename Dispatcher, typename... Handlers>
		static Void AsyncLoopAcquireThenDispatch(auto userLogger, shared_ptr<Socket> peer, Dispatcher dispatcher, Handlers... handlers)
		{
			tuple handlersTuple = { move(handlers)...};
			for (;;)
			{
				auto r = ReceiveFromPeer<Receive>(peer.get());
				auto d = dispatcher(move(r));
				printf("dispatch result %d\n", d);
				auto l = userLogger; // 看 AsyncLoop 的注释
				co_await InvokeWhenEqual(d, handlersTuple, &l);
			}
		}

		template <auto CurrentOption = 0, typename... Handlers>
		static auto InvokeWhenEqual(int dispatcherResult, tuple<Handlers...>& handlersTuple, auto userLoggerPtr)
		{
			if (dispatcherResult == CurrentOption)
			{
				return std::get<CurrentOption>(handlersTuple)(userLoggerPtr);
			}
			else if constexpr ((CurrentOption + 1) < tuple_size_v<tuple<Handlers...>>)
			{
				return InvokeWhenEqual<CurrentOption + 1>(dispatcherResult, handlersTuple, userLoggerPtr);
			}
			else
			{
				throw string("No handler of ") + std::to_string(dispatcherResult);
			}
		}

		template <typename Exception>
		static void ReturnToPeer(Responder* responder, shared_ptr<Socket> peer, Exception const& e)
		{
			JsonObject::_Object _obj;
			_obj.insert({ "type", JsonObject(string("exception")) });
			_obj.insert({ "value", JsonObject(e.what()) });
			responder->RespondTo(peer, JsonObject(move(_obj)).ToString());
		}

		static void ReturnToPeer(Responder* responder, shared_ptr<Socket> peer, JsonObject result)
		{
			JsonObject::_Object _obj;
			_obj.insert({ "type", JsonObject(string("not exception")) });
			_obj.insert({ "value", move(result) });
			responder->RespondTo(peer, JsonObject(move(_obj)).ToString());
		}

#define nameof(VAR) #VAR
#define PRIMITIVE_CAT(A, B) A##B
#define CAT(A, B) PRIMITIVE_CAT(A, B)
// 以上两个不能 undef，因为 FlyTest 里面也定义了
#define ASYNC_HANDLER(NAME, WORKER)                                                                \
	[responder = _responder, worker = WORKER, peer = _peer](auto userLoggerPtr) mutable -> Void    \
	{                                                                                              \
		auto [paras, rawStr] = ReceiveFromPeer<CAT(NAME, Request)::Content, true>(peer.get());     \
		auto id = GenerateRequestId();                                                             \
		auto idLogger = userLoggerPtr->BornNewWith(id);                                            \
		responder->RespondTo(peer, id);                                                            \
		auto requestLogger = idLogger.BornNewWith(nameof(NAME));                                   \
		auto argLogger = requestLogger.BornNewWith(move(rawStr));                                  \
		JsonObject result;                                                                         \
		try                                                                                        \
		{                                                                                          \
			if constexpr (not is_same_v<void, decltype(worker->NAME(move(paras)).await_resume())>) \
			{                                                                                      \
				auto ret = co_await worker->NAME(move(paras));                                     \
				result = Json::JsonConverter::Serialize(ret);                                      \
			}                                                                                      \
			else                                                                                   \
			{                                                                                      \
				co_await worker->NAME(move(paras));                                                \
			}                                                                                      \
		}                                                                                          \
		catch (std::exception const& e)                                                            \
		{                                                                                          \
			ReturnToPeer(responder, peer, e);                                                      \
			argLogger.BornNewWith(ResultStatus::Failed);                                           \
			co_return;                                                                             \
		}                                                                                          \
		ReturnToPeer(responder, peer, move(result));                                               \
		argLogger.BornNewWith(ResultStatus::Complete);                                             \
	}

#define ASYNC_HANDLER_WITHOUT_ARG(NAME, WORKER)                                                                \
	[responder = _responder, worker = WORKER, peer = _peer](auto userLoggerPtr) mutable -> Void    \
	{                                                                                              \
		auto id = GenerateRequestId();                                                             \
		auto idLogger = userLoggerPtr->BornNewWith(id);                                            \
		responder->RespondTo(peer, id);                                                            \
		auto requestLogger = idLogger.BornNewWith(nameof(NAME));                                   \
		auto argLogger = requestLogger.BornNewWith(nameof(-));                                     \
		JsonObject result;                                                                         \
		try                                                                                        \
		{                                                                                          \
			if constexpr (not is_same_v<void, decltype(worker->NAME().await_resume())>)            \
			{                                                                                      \
				auto ret = co_await worker->NAME();                                                \
				result = Json::JsonConverter::Serialize(ret);                                      \
			}                                                                                      \
			else                                                                                   \
			{                                                                                      \
				co_await worker->NAME();                                                           \
			}                                                                                      \
		}                                                                                          \
		catch (std::exception const& e)                                                            \
		{                                                                                          \
			ReturnToPeer(responder, peer, e);                                                      \
			argLogger.BornNewWith(ResultStatus::Failed);                                           \
			co_return;                                                                             \
		}                                                                                          \
		ReturnToPeer(responder, peer, move(result));                                               \
		argLogger.BornNewWith(ResultStatus::Complete);                                             \
	}
#define ASYNC_FUNC_LIB_HANDLER(NAME) ASYNC_HANDLER(NAME, _funcLibWorker)
	};
}
