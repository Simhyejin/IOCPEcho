#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <process.h>

#define BUFSIZE 1024

typedef struct //��������������üȭ.
{
	SOCKET hClntSock;
	SOCKADDR_IN clntAddr;

} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

typedef struct // �����ǹ�������������üȭ.
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
	HANDLE hCompletionPort;			 // ������� CompletionPort�� ���޵� Handle

	SYSTEM_INFO SystemInfo;
	SOCKADDR_IN servAddr;

	LPPER_IO_DATA PerIoData;		 //������ ����ü�� ����� �� ������ ��������
	LPPER_HANDLE_DATA PerHandleData; //���������� ����� ����ü ���⼭�� �����ڵ�� �ּҸ��������ִ�.

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

	//1. Completion Port ����.
	hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	

	//2. Thread Pool ����
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

		//3. ���ϰ� CompletionPort�� ����.
		CreateIoCompletionPort((HANDLE)hClntSock, hCompletionPort, (DWORD)PerHandleData, 0);
		
		PerIoData = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
		memset(&(PerIoData->overlapped), 0, sizeof(OVERLAPPED));
		PerIoData->wsaBuf.len = BUFSIZE;
		PerIoData->wsaBuf.buf = PerIoData->buffer;
		
		Flags = 0;

		//4. ��ø�� �������Է�.
		WSARecv(PerHandleData->hClntSock,	// ������ �Է¼���.
			&(PerIoData->wsaBuf),			// ������ �Է� ����������.
			1,								// ������ �Է� ������ ��.
			(LPDWORD)&RecvBytes,
			(LPDWORD)&Flags,
			&(PerIoData->overlapped),		// OVERLAPPED ����ü ������.
			NULL
		);
	}
	return 0;
}

//����� �Ϸῡ ���� �������� �ൿ ����
unsigned int __stdcall CompletionThread(LPVOID pComPort)

{
	HANDLE hCompletionPort = (HANDLE)pComPort;

	DWORD BytesTransferred;
	LPPER_HANDLE_DATA PerHandleData;
	LPPER_IO_DATA PerIoData;
	DWORD flags;

	while (1)
	{
		
		// 5. ������� �Ϸ�� ������ ���� ����.
		GetQueuedCompletionStatus(hCompletionPort,    // Completion Port
			&BytesTransferred,						  // ���۵� ����Ʈ��
			(LPDWORD)&PerHandleData,
			(LPOVERLAPPED*)&PerIoData,				  // OVERLAPPED ����ü ������.
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

		// 6. �޽���! Ŭ���̾�Ʈ�� ����.
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