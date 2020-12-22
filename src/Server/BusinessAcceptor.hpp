#pragma once
#include <array>
#include <memory>
#include <functional>
#include <asio.hpp>
#include "../Json/Parser.hpp"
#include "../Json/JsonConverter/JsonConverter.hpp"
#include "../Log/Logger.hpp"
#include "BasicType.hpp"
#include "ThreadPool.hpp"
#include "FuncLibWorker.hpp"
#include "Responder.hpp"
#include "ClientService.hpp"
#include "AdminService.hpp"

namespace Server
{
	struct LoginInfo
	{
		string Username;
		string Password;
	};
}

namespace Json::JsonConverter
{
	using Server::LoginInfo;

	template <>
	JsonObject Serialize(LoginInfo const& account)
	{
#define nameof(VAR) #VAR
		auto [username, password] = account;
		JsonObject::_Object obj;
		obj.insert({ nameof(username), Serialize(username) });
		obj.insert({ nameof(password), Serialize(password) });

		return JsonObject(move(obj));
	}

	template <>
	LoginInfo Deserialize(JsonObject const& jsonObj)
	{
		auto username = Deserialize<string>(jsonObj[nameof(username)]);
		auto password = Deserialize<string>(jsonObj[nameof(password)]);

		return LoginInfo{ username, password };
	}
#undef nameof
}

namespace Server
{
	using Log::Logger;
	using ::std::array;
	using ::std::function;
	using ::std::shared_ptr;
	using namespace Json;

	class BusinessAcceptor
	{
	private:
		bool _startAccept = false;
		ThreadPool* _threadPool = nullptr;
		Responder _responder;
		function<ClientService(shared_ptr<Socket>, Responder*)> _clientServiceFactory;
		function<AdminService(shared_ptr<Socket>, Responder*)> _adminServiceFactory;
		Logger _logger;

	public:
		BusinessAcceptor(Responder responder)
			: _responder(move(responder))
		{
		}

		void SetThreadPool(ThreadPool* threadPool)
		{
			_threadPool = threadPool;
		}

		template <typename ClientServiceFactory, typename AdminServiceFactory>
		void SetServiceFactory(ClientServiceFactory clientServiceFactory, AdminServiceFactory adminServiceFactory)
		{
			_clientServiceFactory = move(clientServiceFactory);
			_adminServiceFactory = move(adminServiceFactory);
		}

		// asio 的异步服务器示例不是并发处理的吗，而是一个完了接一个的吗？
		void StartAcceptBackground()
		{
			_startAccept = true;
		}

		void HandleAccept(asio::error_code const& error, Socket peer)
		{
			if (not _startAccept)
			{
				return;
			}
			// 这只是 access log，还要有其他日志，比如处理过程日志
#define ACCESS_LOG_FORMAT STRING("%h %u %i %r %a %s")
			// client ip: XXX.XXX
			// username:
			// request id
			// request function 这里可以放请求的功能: -
			// argument
			// request result status: 这里可以放请求结果的状态，可以自定义一些状态码
			// timestamp 这个要不默认吧  %t

			// 可能要允许 subLogger 复制，因为同一个 connect 可能有多个 request，而且都是异步的
			auto subLogger = _logger.GenerateSubLogger(ACCESS_LOG_FORMAT);
#undef ACCESS_LOG_FORMAT



			// 对端可能中途意外关掉
			if (error)
			{
				// 这里可能 connect 都没建立，所以不用 connectLogger
				subLogger.Error("Wrong connect: " + error.message());
				return;
			}
			
			auto ipAddr = peer.remote_endpoint().address().to_string();
			auto connectLogger = subLogger.BornNewWith(ipAddr);
			_threadPool->Execute([this, p = make_shared<Socket>(move(peer)), conLogger = move(connectLogger)] () mutable
			{
				CommunicateWith(move(p), move(conLogger));
			});
		}

		// peer 是连接中对端的意思
		template <typename ConnectLogger>
		void CommunicateWith(shared_ptr<Socket> peer, ConnectLogger connectLogger)
		{
			array<char, 128> buff;
			auto size = peer->receive(asio::buffer(buff));

			auto send = [&](string const& message)
			{
				_responder.RespondTo(peer, message);
			};

			try
			{
				// 下面的逻辑要重整下 TODO
				// 会不会有多余的空格问题？这个两端发送的时候要统一好
				if (string_view(buff.data(), size) == "hello server")
				{
					send("server ok");
				}
				else
				{
					// error
				}

				auto isAdmin = false;
				// client send username and password 鉴权
				size = peer->receive(asio::buffer(buff));
				auto loginInfoJObj = Json::Parse({ buff.data(), size });
				auto loginInfo = JsonConverter::Deserialize<LoginInfo>(loginInfoJObj);

				auto userLogger = connectLogger.BornNewWith(loginInfo.Username);
				{
					// search account in account info file
				}

				// 可以 DRY 这种固定的问答模式吗？
				if (isAdmin)
				{
					send("server ok. welcome admin.");
					auto s = _adminServiceFactory(move(peer), &_responder);
					return s.Run(move(userLogger));
				}
				else
				{
					send("server ok");
					auto s = _clientServiceFactory(move(peer), &_responder);
					return s.Run(move(userLogger));
				}
			}
			catch (...)
			{
				// get info from exception
				send("Exception happend");
			}
		}
	};
}
