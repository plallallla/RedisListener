#pragma comment(lib,"ws2_32.lib")
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <cpp_redis/core/client.hpp>

auto key_1 = "key_1";
auto key_2 = "key_2";


int main()
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	wVersionRequested = MAKEWORD(2, 2);
	err = WSAStartup(wVersionRequested, &wsaData);
	cpp_redis::client c;
	c.connect();
	c.sync_commit();
	for (size_t i = 0; i < 100000; i++)
	{
		char val[15];
		sprintf_s(val, "SomeValue1_%d\0", i);
		std::cout << "auto set " << val << std::endl;
		c.set(key_1, val);
		c.sync_commit();
		std::this_thread::sleep_for(std::chrono::milliseconds(80));
	}

}
