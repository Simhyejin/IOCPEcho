#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <process.h>

#define BUFSIZE 1024

typedef struct //소켓정보를구조체화.
{
	SOCKET hClntSock;
	SOCKADDR_IN clntAddr;

} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

typedef struct // 소켓의버퍼정보를구조체화.
{
	OVERLAPPED overlapped;
	char buffer[BUFSIZE];
	WSABUF wsaBuf;
} PER_IO_DATA, *LPPER_IO_DATA;

unsigned int __stdcall CompletionThread(LPVOID pComPort);

void ErrorHandling(char *message);

int main(int argc, char** argv)
{

	int port;
	WSADATA wsaData;
	HANDLE hCompletionPort;			 // 만들어질 CompletionPort가 전달될 Handle

	SYSTEM_INFO SystemInfo;
	SOCKADDR_IN servAddr;

	LPPER_IO_DATA PerIoData;		 //위에서 구조체로 만들어 준 소켓의 버퍼정보
	LPPER_HANDLE_DATA PerHandleData; //소켓정보가 저장될 구조체 여기서는 소켓핸들과 주소를가지고있다.

	SOCKET hServSock;
	int RecvBytes;
	int i, Flags;


	if (argc != 2) {
		port = 2738;
	}
	else
		port = argv[1];
	printf("port: %d \n", port);

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) /* Load Winsock 2.2 DLL */
		ErrorHandling("WSAStartup() error!");

	//1. Completion Port 생성.
	hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	

	//2. Thread Pool 생성
	GetSystemInfo(&SystemInfo);
	for (i = 0; i < SystemInfo.dwNumberOfProcessors * 2; i++)
		_beginthreadex(NULL, 0, CompletionThread, (LPVOID)hCompletionPort, 0, NULL);


	hServSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = port;

	bind(hServSock, (SOCKADDR*)&servAddr, sizeof(servAddr));
	listen(hServSock, 5);
	
	
	while (TRUE)
	{
		SOCKET hClntSock;
		SOCKADDR_IN clntAddr;
		int addrLen = sizeof(clntAddr);
		printf(" ==> Waiting for client's connection...\n");
		hClntSock = accept(hServSock, (SOCKADDR*)&clntAddr, &addrLen);

		printf(" ==> New client(%d:%d) connected...\n", clntAddr.sin_addr, clntAddr.sin_port);

		PerHandleData = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));
		PerHandleData->hClntSock = hClntSock;
		memcpy(&(PerHandleData->clntAddr), &clntAddr, addrLen);

		//3. 소켓과 CompletionPort의 연결.
		CreateIoCompletionPort((HANDLE)hClntSock, hCompletionPort, (DWORD)PerHandleData, 0);
		
		PerIoData = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
		memset(&(PerIoData->overlapped), 0, sizeof(OVERLAPPED));
		PerIoData->wsaBuf.len = BUFSIZE;
		PerIoData->wsaBuf.buf = PerIoData->buffer;
		
		Flags = 0;

		//4. 중첩된 데이터입력.
		WSARecv(PerHandleData->hClntSock,	// 데이터 입력소켓.
			&(PerIoData->wsaBuf),			// 데이터 입력 버퍼포인터.
			1,								// 데이터 입력 버퍼의 수.
			(LPDWORD)&RecvBytes,
			(LPDWORD)&Flags,
			&(PerIoData->overlapped),		// OVERLAPPED 구조체 포인터.
			NULL
		);
	}
	return 0;
}

//입출력 완료에 따른 쓰레드의 행동 정의
unsigned int __stdcall CompletionThread(LPVOID pComPort)

{
	HANDLE hCompletionPort = (HANDLE)pComPort;

	DWORD BytesTransferred;
	LPPER_HANDLE_DATA PerHandleData;
	LPPER_IO_DATA PerIoData;
	DWORD flags;

	while (1)
	{
		
		// 5. 입출력이 완료된 소켓의 정보 얻음.
		GetQueuedCompletionStatus(hCompletionPort,    // Completion Port
			&BytesTransferred,						  // 전송된 바이트수
			(LPDWORD)&PerHandleData,
			(LPOVERLAPPED*)&PerIoData,				  // OVERLAPPED 구조체 포인터.
			INFINITE
		);

		if (BytesTransferred == 0) 
		{
			printf(" ==> Client(%d:%d) disconnected...\n",PerHandleData->clntAddr.sin_addr, PerHandleData->clntAddr.sin_port);
			closesocket(PerHandleData->hClntSock);
			free(PerHandleData);
			free(PerIoData);
			continue;
		}

		PerIoData->wsaBuf.buf[BytesTransferred] = '\0';
		printf(" *** Client(%d:%d) sent : %s\n", PerHandleData->clntAddr.sin_addr, PerHandleData->clntAddr.sin_port, PerIoData->wsaBuf.buf);

		// 6. 메시지! 클라이언트로 에코.
		PerIoData->wsaBuf.len = BytesTransferred;
		WSASend(PerHandleData->hClntSock, &(PerIoData->wsaBuf), 1, NULL, 0, NULL, NULL);

		// RECEIVE AGAIN
		memset(&(PerIoData->overlapped), 0, sizeof(OVERLAPPED));
		PerIoData->wsaBuf.len = BUFSIZE;
		PerIoData->wsaBuf.buf = PerIoData->buffer;

		flags = 0;
		WSARecv(PerHandleData->hClntSock,
			&(PerIoData->wsaBuf),
			1,
			NULL,
			&flags,
			&(PerIoData->overlapped),
			NULL
		);
	}
	return 0;
}

void ErrorHandling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}