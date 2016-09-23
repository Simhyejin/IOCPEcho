#include "stdafx.h"
#include "Winsock2.h"
#include "NTSecApi.h"
#include "iostream"
using namespace std;

#pragma comment(lib, "Ws2_32.lib")

SOCKET GetListenSocket(short shPortNo, int nBacklog = SOMAXCONN)
{
	SOCKET hsoListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (hsoListen == INVALID_SOCKET)
	{
		cout << "socket failed, code : " << WSAGetLastError() << endl;
		return INVALID_SOCKET;
	}

	SOCKADDR_IN	sa;
	memset(&sa, 0, sizeof(SOCKADDR_IN));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(shPortNo);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	LONG lSockRet = bind(hsoListen, (PSOCKADDR)&sa, sizeof(SOCKADDR_IN));
	if (lSockRet == SOCKET_ERROR)
	{
		cout << "bind failed, code : " << WSAGetLastError() << endl;
		closesocket(hsoListen);
		return INVALID_SOCKET;
	}

	lSockRet = listen(hsoListen, nBacklog);
	if (lSockRet == SOCKET_ERROR)
	{
		cout << "listen failed, code : " << WSAGetLastError() << endl;
		closesocket(hsoListen);
		return INVALID_SOCKET;
	}

	return hsoListen;
}

DWORD g_dwTheadId = 0;
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	PostThreadMessage(g_dwTheadId, WM_QUIT, 0, 0);
	return TRUE;
}



struct SOCK_ITEM : OVERLAPPED
{
	SOCKET	_sock;
	char	_buff[512];

	SOCK_ITEM(SOCKET sock)
	{
		memset(this, 0, sizeof(*this));
		_sock = sock;
	}
};
typedef SOCK_ITEM* PSOCK_ITEM;


#define MAX_CLI_CNT	63
void _tmain()
{
	WSADATA	wsd;
	if (WSAStartup(MAKEWORD(2, 2), &wsd))
	{
		cout << "WSAStartup failed..." << endl;
		return;
	}
	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE))
	{
		cout << "SetConsoleCtrlHandler failed, code : " << GetLastError() << endl;
		return;
	}
	g_dwTheadId = GetCurrentThreadId();

	//Listen Socket 생성
	SOCKET hsoListen = GetListenSocket(9001, MAX_CLI_CNT);
	if (hsoListen == INVALID_SOCKET)
	{
		WSACleanup();
		return;
	}
	cout << " ==> Waiting for client's connection......" << endl;

	WSAEVENT hevListen = WSACreateEvent();

	//int wsaeventselect(socket   s, wsaevent heventobject, long	lnetworkevents)
	WSAEventSelect(hsoListen, hevListen, FD_ACCEPT);

	int			nSockCnt = 1;
	SOCKET		arSocks[MAX_CLI_CNT + 1];
	PSOCK_ITEM	arItems[MAX_CLI_CNT + 1];
	memset(arSocks, 0xFF, (MAX_CLI_CNT + 1) * sizeof(SOCKET));
	memset(arItems, 0, (MAX_CLI_CNT + 1) * sizeof(PSOCK_ITEM));
	arSocks[0] = (SOCKET)hevListen;

	while (true)
	{

		//DWORD WINAPI MsgWaitForMultipleObjects(DWORD  nCount, const HANDLE *pHandles, BOOL   bWaitAll, DWORD  dwMilliseconds, DWORD  dwWakeMask)
		DWORD dwWaitRet = MsgWaitForMultipleObjects
		(
			nSockCnt, (PHANDLE)arSocks, FALSE, INFINITE, QS_ALLPOSTMESSAGE
		);

		if (dwWaitRet == WAIT_FAILED)
		{
			cout << "MsgWaitForMultipleObjects failed : " << GetLastError() << endl;
			break;
		}

		//스레드 메시지가 도착
		if (dwWaitRet == WAIT_OBJECT_0 + nSockCnt)
			break;

		DWORD		dwFlags = 0;
		LONG		nErrCode = ERROR_SUCCESS;
		PSOCK_ITEM	pSI = NULL;

		// Listen socket 상태 변화
		if (dwWaitRet == WAIT_OBJECT_0)
		{
			WSANETWORKEVENTS ne;

			//int WSAEnumNetworkEvents(_IN_ SOCKET  s, _IN_ WSAEVENT  hEventObject, _OUT_ LPWSANETWORKEVENTS  lpNetworkEvents)
			WSAEnumNetworkEvents(hsoListen, hevListen, &ne);
			if (ne.lNetworkEvents & FD_ACCEPT)
			{
				nErrCode = ne.iErrorCode[FD_ACCEPT_BIT];
				if (nErrCode != 0)
				{
					cout << " ==> Error occurred, code = " << nErrCode << endl;
					break;
				}

				SOCKET sock = accept(hsoListen, NULL, NULL);
				if (sock == INVALID_SOCKET)
				{
					nErrCode = WSAGetLastError();
					if (nErrCode != WSAEINTR)		//Interrupted function call
						cout << " ==> Error occurred, code = " << nErrCode << endl; 

					break;
				}

				pSI = new SOCK_ITEM(sock);
				arSocks[nSockCnt] = sock;
				arItems[nSockCnt] = pSI;
				nSockCnt++;
				cout << " ==> New client " << sock << " connected" << endl;
			}
		}
		else //accept되 소켓들 중 하나에서 상태변화
		{
			int nIndex = (int)(dwWaitRet - WAIT_OBJECT_0);
			SOCKET sock = arSocks[nIndex];
			pSI = arItems[nIndex];

			DWORD dwTrBytes = 0;

			/*BOOL WSAAPI WSAGetOverlappedResult(
				_In_  SOCKET          s,
				_In_  LPWSAOVERLAPPED lpOverlapped,
				_Out_ LPDWORD         lpcbTransfer,
				_In_  BOOL            fWait,
				_Out_ LPDWORD         lpdwFlags
			);*/
			if (!WSAGetOverlappedResult(sock, pSI, &dwTrBytes, FALSE, &dwFlags))
			{
				nErrCode = WSAGetLastError();
				goto $LABEL_CLOSE;
			}
			else
			{
				if (dwTrBytes == 0)		
					goto $LABEL_CLOSE;

				pSI->_buff[dwTrBytes] = 0;
				cout << " *** Client(" << pSI->_sock << ") sent : " << pSI->_buff << endl;

				int lSockRet = send(pSI->_sock, pSI->_buff, dwTrBytes, 0);
				if (lSockRet == SOCKET_ERROR)
				{
					nErrCode = WSAGetLastError();
					goto $LABEL_CLOSE;
				}
			}
		}

		WSABUF wb;
		wb.buf = pSI->_buff, wb.len = sizeof(pSI->_buff);

		//int WSARecv(
		//  __in     SOCKET s,
		//	__inout  LPWSABUF lpBuffers,
		//	__in     DWORD dwBufferCount,
		//	__out    LPDWORD lpNumberOfBytesRecvd,
		//	__inout  LPDWORD lpFlags,
		//	__in     LPWSAOVERLAPPED lpOverlapped,
		//	__in     LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
		//	);
		int nSockRet = WSARecv(pSI->_sock, &wb, 1, NULL, &dwFlags, pSI, NULL);
		if (nSockRet == SOCKET_ERROR)
		{
			nErrCode = WSAGetLastError();
			if (nErrCode != WSA_IO_PENDING)
				goto $LABEL_CLOSE;
		}
		continue;

	$LABEL_CLOSE:	//소켓관련 에러가 발생하거나 연결이 끊겼을 때 처리
		int nIndex = (int)(dwWaitRet - WAIT_OBJECT_0);
		for (int i = nIndex; i < nSockCnt-1; i++)
		{
			arSocks[i] = arSocks[i + 1];
			arItems[i] = arItems[i + 1];
		}
		arSocks[nSockCnt-1] = INVALID_SOCKET;
		arItems[nSockCnt-1] = NULL;
		nSockCnt--;

		if (nErrCode != WSAECONNABORTED)
		{
			if (nErrCode == ERROR_SUCCESS || nErrCode == WSAECONNRESET)
				cout << " ==> Client " << pSI->_sock << " disconnected..." << endl;
			else
				cout << " ==> Error occurred, code = " << nErrCode << endl;
			closesocket(pSI->_sock);
			delete pSI;
		}
	}

	if (hsoListen != INVALID_SOCKET)
		closesocket(hsoListen);
	WSACloseEvent(hevListen);
	for (int i = 1; i < nSockCnt ; i++)
	{
		if (arSocks[i] != INVALID_SOCKET)
			closesocket(arSocks[i]);
		if (arItems[i] != NULL)
			delete arItems[i];
	}

	cout << "==== Server terminates... ==========================" << endl;

	WSACleanup();
}
