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
#include <memory>
#include <vector>
#include <atomic>

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
		ss << "logs/" << time_tm.tm_year + 1900
			<< "_" << time_tm.tm_mon + 1
			<< "_"<< time_tm.tm_mday
			<< "_"<< time_tm.tm_hour 
			//<< "_"<< time_tm.tm_min TODO
			//<< "_" << time_tm.tm_sec
			<< ".log";
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

sPdlog recorder{"getTest"};

std::vector <std::string> sub_names;
std::vector <std::string> get_keys;
int time_span;

std::string ip;
int port;
bool InitConfig()
{
	std::ifstream in_file("Config.xml", std::ios::binary | std::ios::ate);
	int size = in_file.tellg();
	in_file.seekg(0, std::ios::beg);
	char *buffer = new char[size + 1];
	in_file.read(buffer, size);
	buffer[size] = 0;
	rapidxml::xml_document<> xDoc;
	xDoc.parse<0>(buffer);
	auto root = xDoc.first_node("redis_listen");
	int mode;
	if (auto config = root->first_node("redis_config"))
	{
		ip = config->first_node("IP")->first_attribute("value")->value();
		port = atoi(config->first_node("Port")->first_attribute("value")->value());
		mode = atoi(config->first_node("Mode")->first_attribute("value")->value());
	}
	if (mode / 10)
	{
		auto getNode = root->first_node("redis_get");
		time_span = atoi(getNode->first_node("time_span")->first_attribute("value")->value());
		int count = atoi(getNode->first_node("key_count")->first_attribute("value")->value());
		char key[20];
		for (int i = 0; i < count; i++)
		{
			sprintf_s(key, "key_%d", i);
			std::string keyStr{ getNode->first_node(key)->first_attribute("value")->value() };
			get_keys.emplace_back(keyStr);
		}
	}
	if (mode % 10)
	{
		auto getNode = root->first_node("redis_sub");
	}
	delete[] buffer;
	in_file.close();
	return true;
}
std::atomic<bool> getFlag{ true };
std::vector<cpp_redis::reply> lastReplys;
void GetWorker()
{
	cpp_redis::client c;
	c.connect(ip, port);
	c.sync_commit();
	while (getFlag.load())
	{
		try
		{
			c.mget(get_keys, [=](cpp_redis::reply & reply)
			{
				auto &replys = reply.as_array();
				for (int i = 0; i < get_keys.size(); i++)
				{
					if (replys[i].as_string() != lastReplys[i].as_string())
					{
						std::cout << get_keys[i] + " get " + replys[i].as_string() << std::endl;
						recorder.logger->info(get_keys[i] + " get " + replys[i].as_string());
					}
				}
				lastReplys = replys;
			});
			c.sync_commit();
		}
		catch (const std::exception&)
		{
			recorder.logger->warn("exception@c.mget");
			return;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(time_span));
	}
}

void funcTestGetPart()
{

	cpp_redis::client c;
	c.connect(ip, port);
	c.sync_commit();
	try
	{
		c.mget(get_keys, [=](cpp_redis::reply & reply)
		{
			lastReplys = reply.as_array();
			for (int i = 0; i < get_keys.size(); i++)
			{
				std::cout << get_keys[i] + " get " + lastReplys[i].as_string() << " for the first time" << std::endl;
				recorder.logger->info(get_keys[i] + " get " + lastReplys[i].as_string() + " for the first time");
			}
		});
		c.sync_commit();
	}
	catch (const std::exception&)
	{
		recorder.logger->warn("exception@c.mget1st");
		return;
	}
	std::thread tWorker{ GetWorker };
	std::this_thread::sleep_for(std::chrono::milliseconds(10000));
	getFlag.store(false);
	tWorker.join();
}

cpp_redis::subscriber::subscribe_callback_t ProcessSubscribeRet = [](const std::string & c, const std::string & msg)
{
	std::cout << "c:" << c << ";msg:" << msg << std::endl;
};

void funcTestSubPart()
{
	cpp_redis::subscriber sub;
	sub.connect(ip, port);
	sub.commit();
	//sub.psubscribe("*");
}

int main()
{
	if (!InitSocket())
	{
		recorder.logger->critical("error @ InitSocket");
		return 0;
	}
	InitConfig();
	funcTestGetPart();
	//funcTestSubPart();
	return 0;
}
