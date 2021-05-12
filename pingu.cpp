#include <stdio.h>
#include <winsock2.h>
#include <cstdint>
#include <array>

#include <ws2tcpip.h>
//#include <stdio.h>
//#include <stdlib.h>   // Needed for _wtoi

#pragma comment(lib,"Ws2_32.lib")

enum ICMPType : uint8_t {
	ECHO_REPLY = 0,
	ECHO_REQUEST = 8,
};
//
#pragma pack(push,1)
typedef struct ICMPMessage
{
	ICMPType type;
	uint8_t code;
	uint16_t checksum;
	uint16_t identifire;
	uint16_t sequence;
} ICPMessage;
#pragma pack(pop)


const int IP_HEADER_LENGTH = 20;



// calc ICMP header checksum
uint16_t CalcChecksum(uint16_t* message, size_t size)
{
	uint16_t sum = 0;
	while (1 < size)
	{
		uint16_t temp = sum + *message++;
		if (temp < sum)
		{
			temp += 1;
		}
		sum = temp;
		size -= 2;
	}
	if (size)
	{
		uint16_t temp = sum + *(uint8_t*)message;
		if (temp < sum)
		{
			temp += 1;
		}
		sum = temp;
	}
	return ~sum;
}

// debug
void Dump(byte* data, size_t size)
{
	//printf("-Dump ============================================\n");
	//for (int i = 0; i < size; i++)
	//{
	//    if (1 < i && i % 16 == 0) printf("\n");
	//    printf("%02X ", data[i]);
	//    if ((i + 1) % 8 == 0) printf("  ");
	//}
	//printf("\n-End =============================================\n");
}

HANDLE break_handle = CreateEventA(nullptr, FALSE, FALSE, nullptr);

// main ======================================================================================================================
int main(int argc, char** argv)
{
	// TODO
	// TTL
	// n Times ping 

	// break (ctrl-c)
	::SetConsoleCtrlHandler([](DWORD event) -> BOOL {
		if (event == CTRL_C_EVENT)
		{
			::SetEvent(break_handle);
			printf("Break\n");
			return TRUE;
		}
		return FALSE;
	}, TRUE);



	// dest from arguments
	if (argc < 2)
	{
		printf("ip must be specified\n");
		return 1;
	}

	// initialize winsock
	WSADATA wsa_data = { 0 };
	int startup_err = ::WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (startup_err)
	{
		WSACleanup();
		printf("Failed to WSAStartup. [0x%08x]", startup_err);
		return startup_err;
	}


	// create socket
	SOCKET sock = ::WSASocketW(
		AF_INET,        // int af
		SOCK_RAW,       // int type
		IPPROTO_ICMP,   // int protocol
		nullptr,        // LPWSAPROTOCOL_INFO lpProtocolInfo
		0,              // GROUP g
		WSA_FLAG_OVERLAPPED               // DWORD dwFlags
	);
	if (sock == INVALID_SOCKET)
	{
		auto last_error = WSAGetLastError();
		printf("Failed to WSASocket(). INVALID_SOCKET. err: 0x%08x\n", last_error);
		return 1;
	}


	// create ICMP header
	ICMPMessage icmp = {};
	icmp.type = ICMPType::ECHO_REQUEST;
	icmp.checksum = CalcChecksum((uint16_t*)&icmp, sizeof(icmp));

	sockaddr_in to_addr = { 0 };
	to_addr.sin_family = AF_INET;
	int pton_res = ::InetPtonA(AF_INET, argv[1], &to_addr.sin_addr);
	if (pton_res != 1)
	{
		printf("Failed to InetPton(). err: 0x%08x\n", pton_res);
		WSACleanup();
	}


	// send
	//auto res = ::sendto(sock, (const char*)&icmp, sizeof(icmp), 0, (sockaddr*)&addr, sizeof(addr));
	printf("ping to %s ...\n", argv[1]);
	WSABUF buffer = { 0 };
	buffer.buf = (CHAR*)&icmp;
	buffer.len = sizeof(icmp);
	DWORD bytes_sent = 0;

	int send_res = ::WSASendTo(
		sock,                   // SOCKET s
		&buffer,                // LPWSABUF lpBuffers
		1,                      // DWORD dwBufferCount
		&bytes_sent,            // LPDWORD lpNumberOfByteSent
		0,                      // DWORD dwFlags
		(sockaddr*)&to_addr,    // const sockaddr *lpTo
		sizeof(to_addr),        // int iTolen
		nullptr,                // LPWSAOVERLAPPED lpOverlapped
		nullptr                 // LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
	);
	if (send_res)
	{
		int send_err = ::WSAGetLastError();
		printf("Failed to WSASendTo. err: 0x%08x\n", send_err);
		::WSACleanup();
		return send_err;
	}
	if (bytes_sent != sizeof(icmp))
	{
		printf("Failed to send all data\n");
		return 1;
	}

	Dump((byte*)&icmp, sizeof(icmp));




	// receive

	// overlap
	WSAOVERLAPPED overlapped = { 0 };
	overlapped.hEvent = ::WSACreateEvent();
	if (overlapped.hEvent == WSA_INVALID_EVENT)
	{
		int event_err = WSAGetLastError();
		printf("Failed to WSACreateEvent(). err: 0x%08x\n", event_err);
		WSACleanup();
		return event_err;
	}


	std::array<byte, 256> b{};
	WSABUF recv_buf = { 0 };
	recv_buf.buf = (char*)b.data();
	recv_buf.len = b.size();
	sockaddr_in from = {};
	int from_len = sizeof(from);
	DWORD recv_len = 0;
	DWORD flags = 0;


	// receive
	int recv_res = ::WSARecvFrom(
		sock,              // SOCKET s
		&recv_buf,         // LPWSABUF lpBuffers
		1,                 // DWORD dwBufferCount
		&recv_len,         // LPDWORD lpNumberObBytesRecvd
		&flags,            // LPDWORD lpFlags
		(sockaddr*)&from,  // sockaddr *lpFrom
		&from_len,         // LPINT lpFromlen
		&overlapped,       // LPWSAOVERLAPPED lpOverlapped
		nullptr            // LPWSAOVERLAPPED lpCompletionRoutine
	);

	if (recv_res != 0)
	{
		int recv_err = ::WSAGetLastError();
		if (recv_err != ERROR_IO_PENDING)
		{
			printf("Failed to WSARecvFrom. err: 0x%08x\n", recv_err);
			::WSACleanup();
			return recv_err;
		}
	}
	//WaitForSingleObject(overlapped.hEvent, 10000);
	HANDLE handles[] = { break_handle, overlapped.hEvent };
	DWORD wait_res = ::WaitForMultipleObjects(2, handles, FALSE, 10000);
	if (wait_res == WAIT_TIMEOUT)
	{
		printf("timeout\n");
		::WSACleanup();
		return 0;
	}
	if (wait_res - WAIT_OBJECT_0 == 0) // breaked
	{
		::WSACleanup();
		return 0;
	}
	if (recv_len < 0)
	{
		auto last_error = WSAGetLastError();
		printf("Failed to recvfrom. Err: 0x%08x\n", last_error);
		WSACleanup();
		return 2;
	}

	Dump(b.data(), recv_len);

	// result
	ICMPMessage* result = (ICMPMessage*)&b[IP_HEADER_LENGTH];
	char buf[INET_ADDRSTRLEN];
	PCSTR ntop_res = ::InetNtopA(AF_INET, &from.sin_addr.s_addr, buf, sizeof(buf));
	if (!ntop_res)
	{
		int ntop_err = WSAGetLastError();
		printf("Failed to InetNtop(). err: 0x%08x\n", ntop_err);
	}
	else
	{
		printf("receive from %s\n", buf);
	}


	// cleanup
	WSACleanup();

}
