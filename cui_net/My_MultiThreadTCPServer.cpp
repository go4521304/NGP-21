#pragma comment(lib, "ws2_32")
#include <WinSock2.h>
#include <Windows.h>
#include <stdlib.h>
#include <stdio.h>
using namespace std;

#define SERVERPORT 9000
#define BUFSIZE 512

CRITICAL_SECTION cs;

int YPos = 0;

void SetConsoleCursor(bool visible = false, int size = 10)
{
	CONSOLE_CURSOR_INFO info;
	info.dwSize = size;
	info.bVisible = visible;
	SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
}

void gotoXY(int x, int y)
{
	COORD Pos;
	Pos.X = x;
	Pos.Y = y;
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), Pos);
}

// ���� �Լ� ���� ��� �� ����
void err_quit(const char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, (LPCWSTR)msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

// ���� �Լ� ���� ���
void err_display(const char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	EnterCriticalSection(&cs);
	printf("[%s] %s", msg, (char*)lpMsgBuf);
	LeaveCriticalSection(&cs);
	LocalFree(lpMsgBuf);
}

// ����� ���� ������ ���� �Լ�
int recvn(SOCKET s, char* buf, int len, int flags)
{
	int received;
	char* ptr = buf;
	int left = len;

	while (left > 0)
	{
		received = recv(s, ptr, left, flags);
		if (received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if (received == 0)
			break;
		left -= received;
		ptr += received;
	}
	return (len - left);
}

// Ŭ���̾�Ʈ�� ������ ���
DWORD WINAPI ProcessClient(LPVOID arg)
{
	SOCKET client_sock = (SOCKET)arg;
	int retval;
	SOCKADDR_IN clientaddr;
	int addrlen;
	int len, lenName;
	char data[BUFSIZE], fileName[BUFSIZE];
	FILE* fp;

	// Ŭ���̾�Ʈ ���� ���
	addrlen = sizeof(clientaddr);
	getpeername(client_sock, (SOCKADDR*)&clientaddr, &addrlen);

	int pos;

	EnterCriticalSection(&cs);
	pos = YPos - 2;
	LeaveCriticalSection(&cs);

	// Ŭ���̾�Ʈ�� ������ ���
	while (1)
	{
		// (1) ���� �̸� �ޱ�
		// ������ �ޱ� (���� ����)
		retval = recvn(client_sock, (char*)&lenName, sizeof(int), 0);
		if (retval == SOCKET_ERROR)
		{
			EnterCriticalSection(&cs);
			gotoXY(0, pos);
			err_display("recv()");
			LeaveCriticalSection(&cs);
			break;
		}
		else if (retval == 0)
			break;

		// ������ �ޱ� (���� ����)
		retval = recvn(client_sock, fileName, lenName, 0);
		if (retval == SOCKET_ERROR)
		{
			EnterCriticalSection(&cs);
			gotoXY(0, pos);
			err_display("recv()");
			LeaveCriticalSection(&cs);
			break;
		}
		else if (retval == 0)
		{
			break;
		}
		fileName[lenName] = '\0';



		// (2) ���� ���� �ޱ�
		fp = fopen(fileName, "wb");

		// ������ �ޱ� (���� ����)
		retval = recvn(client_sock, (char*)&len, sizeof(int), 0);
		if (retval == SOCKET_ERROR)
		{
			EnterCriticalSection(&cs);
			gotoXY(0, pos);
			err_display("recv()");
			LeaveCriticalSection(&cs);
			fclose(fp);
			break;
		}
		else if (retval == 0)
		{
			fclose(fp);
			break;
		}

		// ������ �ޱ� (���� ����)
		int readSize = 0, leftSize = len;
		while (leftSize > 0)
		{

			readSize = BUFSIZE;
			if (leftSize < readSize)
				readSize = leftSize;

			retval = recvn(client_sock, data, readSize, 0);
			if (retval == SOCKET_ERROR)
			{
				EnterCriticalSection(&cs);
				gotoXY(0, pos);
				err_display("recv()");
				LeaveCriticalSection(&cs);
				break;
			}
			else if (retval == 0)
			{
				break;
			}

			// ���� ������ ���
			leftSize -= retval;

			fwrite(data, sizeof(char), retval, fp);

			EnterCriticalSection(&cs);
			gotoXY(0, pos);
			printf("���۷�: %.2f %%", (float)(len - leftSize) / (float)len * 100);
			LeaveCriticalSection(&cs);
		}

		EnterCriticalSection(&cs);
		gotoXY(0, pos);
		printf("[TCP/%s:%d] '%s'�� �����Ͽ����ϴ�.", inet_ntoa(clientaddr.sin_addr),
			ntohs(clientaddr.sin_port), fileName);
		LeaveCriticalSection(&cs);

		fclose(fp);
	}

	// closesocket()
	closesocket(client_sock);
	EnterCriticalSection(&cs);
	gotoXY(0, pos - 1);
	printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d",
		inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
	LeaveCriticalSection(&cs);

	return 0;
}

int main(int argc, char* argv[])
{
	SetConsoleCursor();

	InitializeCriticalSection(&cs);

	int retval;

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// socket()
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET) err_quit("socket()");

	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit("listen()");

	// ������ ��ſ� ����� ����
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;
	HANDLE hThread;

	while (1)
	{
		// accept()
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (SOCKADDR*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET)
		{
			err_display("accept()");
			break;
		}
		
		EnterCriticalSection(&cs);
		gotoXY(0, YPos);
		// ������ Ŭ���̾�Ʈ ���� ���
		printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d",
			inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
		YPos += 3;
		LeaveCriticalSection(&cs);

		// ������ ����
		hThread = CreateThread(NULL, 0, ProcessClient,
			(LPVOID)client_sock, 0, NULL);
		if (hThread == NULL) { closesocket(client_sock); }
		else { CloseHandle(hThread); }
	}

	// closesocket()
	closesocket(listen_sock);

	// ���� ����
	WSACleanup();

	DeleteCriticalSection(&cs);
	return 0;
}