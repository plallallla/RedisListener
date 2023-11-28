#pragma comment(lib,"ws2_32.lib")
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <fstream>
#include <cpp_redis/core/subscriber.hpp>
#include <cpp_redis/core/client.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <sstream>
#include <regex>
#include <rapidxml/rapidxml.hpp>

struct sPdlog
{
	std::shared_ptr<spdlog::logger> logger;
	sPdlog(std::string name)
	{

		auto now = std::chrono::system_clock::now();
		//通过不同精度获取相差的毫秒数
		uint64_t dis_millseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
			- std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count() * 1000;
		time_t tt = std::chrono::system_clock::to_time_t(now);
		tm time_tm;
		localtime_s(&time_tm, &tt);
		std::stringstream ss;
		ss << "logs/" << time_tm.tm_year + 1900 << "_"
			<< time_tm.tm_mon + 1 << "_"
			<< time_tm.tm_mday << "_"
			<< time_tm.tm_hour << "_"
			<< time_tm.tm_min << "_"
			<< time_tm.tm_sec << ".log";
		logger = spdlog::basic_logger_mt(name, ss.str());
		logger->flush_on(spdlog::level::trace);
	}
};

void message_handler(const std::string& channel, const std::string& msg)
{
	std::cout << "msg:" << msg << "@" << channel;
}

cpp_redis::subscriber::connect_callback_t connectCB = [=](const std::string& host, std::size_t port, cpp_redis::subscriber::connect_state status)
{
	switch (status)
	{
	case cpp_redis::subscriber::connect_state::dropped:
		std::cout << "dropped" << std::endl;
		break;
	case cpp_redis::subscriber::connect_state::start:
		std::cout << "start" << std::endl;
		break;
	case cpp_redis::subscriber::connect_state::sleeping:
		std::cout << "sleeping" << std::endl;
		break;
	case cpp_redis::subscriber::connect_state::ok:
		std::cout << "ok" << std::endl;
		break;
	case cpp_redis::subscriber::connect_state::failed:
		std::cout << "failed" << std::endl;
		break;
	case cpp_redis::subscriber::connect_state::lookup_failed:
		std::cout << "lookup_failed" << std::endl;
		break;
	case cpp_redis::subscriber::connect_state::stopped:
		std::cout << "stopped" << std::endl;
		break;
	default:
		break;
	}
};

bool InitSocket()
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	wVersionRequested = MAKEWORD(2, 2);
	err = WSAStartup(wVersionRequested, &wsaData);
	return err == 0;
}

std::list <std::string> sub_names;
std::list <std::string> get_names;
int time_span;

bool InitConfig()
{
	std::ifstream file("config.txt", std::ios::in);
	std::string line;
	std::getline(file, line);
	if (!strcmp(line.c_str(), "subscriber"))
	{
		std::getline(file, line);
		std::regex fliter{ "," };
		std::sregex_token_iterator it(line.begin(), line.end(), fliter, -1);
		std::sregex_token_iterator end;
		while (it != end)
		{
			sub_names.emplace_back(it->str());
			it++;
		}
	}
	else
	{
		spdlog::warn("config error @ subscriber");
		return false;
	}
	std::getline(file, line);
	if (!strcmp(line.c_str(), "channel"))
	{
		std::getline(file, line);
		std::regex fliter{ "," };
		std::sregex_token_iterator it(line.begin(), line.end(), fliter, -1);
		std::sregex_token_iterator end;
		while (it != end)
		{
			get_names.emplace_back(it->str());
			it++;
		}
	}
	else
	{
		spdlog::warn("config error @ channel");
		return false;
	}
	std::getline(file, line);
	if (!strcmp(line.c_str(), "timespan"))
	{
		std::getline(file, line);
		time_span = atoi(line.c_str());
	}
	else
	{
		spdlog::warn("config error @ timespan");
		return false;
	}
	return true;
}

bool stop{ false };
void GetWorker(const std::string &channel)
{
	while (!stop)
	{
		//TODO:get msg
		sPdlog sysLog{ channel };
		sysLog.logger->info("msg");
		std::this_thread::sleep_for(std::chrono::milliseconds(time_span));
	}
}

void funcTestGet()
{

}

int main()
{
	sPdlog sysLog{ "system" };
	//InitConfig();

	if (!InitSocket())
	{
		sysLog.logger->critical("error @ InitSocket");
		return 0;
	}
	try
	{
		cpp_redis::subscriber s;
		s.connect("127.0.0.1", 6379);
		s.psubscribe("*", message_handler);
		s.commit();
	}
	catch (const std::exception& e)
	{
		sysLog.logger->critical(e.what());
	}

	try
	{
		cpp_redis::client c;
		c.connect("127.0.0.1", 6379);
		c.get("hello", [](cpp_redis::reply & reply)
		{
			std::cout << reply << std::endl;
		});
	}
	catch (const std::exception & e)
	{
		sysLog.logger->critical(e.what());
	}

	cpp_redis::client client;

	client.connect();

	client.set("hello", "42");
	client.get("hello", [](cpp_redis::reply& reply) {
		std::cout << reply << std::endl;
	});
	client.sync_commit();

	//TODO:sub
	//std::vector<std::thread> getWorkers;
	//for (auto &channel : get_names)
	//{
	//	getWorkers.emplace_back(std::thread{ GetWorker ,channel });
	//}

	//stop = true;
	//for (auto &worker : getWorkers)
	//{
	//	worker.join();
	//}
	return 0;
}
