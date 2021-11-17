// net_server.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//

#include "framework.h"
#include "net_server.h"

#define SERVERPORT	9000
#define BUFSIZE 512

// 전역 변수:
HWND hDebug, hProgress[2];
HANDLE hMainThread;
WSADATA wsa;
SOCKET listen_sock;
CRITICAL_SECTION cs;
HINSTANCE hInst;
bool UseProgress[2] = { false, false };

// 이 코드 모듈에 포함된 함수의 선언을 전달합니다:
INT_PTR CALLBACK    DlgProc(HWND, UINT, WPARAM, LPARAM);
void err_quit(const TCHAR* msg);
void err_display(const char* msg);
void DisplayText(const char* msg, ...);
int recvn(SOCKET s, char* buf, int len, int flags);
DWORD WINAPI Server(LPVOID arg);
DWORD WINAPI ProcessClient(LPVOID arg);
HWND GetProgress();
void BackProgress(HWND val);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	hInst = hInstance;
	return DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG), NULL, DlgProc);
}

INT_PTR CALLBACK DlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		InitializeCriticalSection(&cs);
		hProgress[0] = GetDlgItem(hDlg, IDC_PROGRESS1);
		hProgress[1] = GetDlgItem(hDlg, IDC_PROGRESS2);

		hDebug = GetDlgItem(hDlg, IDC_DEBUG);
		hMainThread = CreateThread(NULL, 0, Server, NULL, 0, NULL);
		SendMessage(hProgress[0], PBM_SETRANGE, 0, MAKELPARAM(0, 100));
		SendMessage(hProgress[1], PBM_SETRANGE, 0, MAKELPARAM(0, 100));
		return (INT_PTR)TRUE;

	case WM_CLOSE:
		PostQuitMessage(0);
		return (INT_PTR)TRUE;

	case WM_DESTROY:
		DeleteCriticalSection(&cs);
		CloseHandle(hMainThread);
		closesocket(listen_sock);
		WSACleanup(); 
		return (INT_PTR)TRUE;

	}
	return (INT_PTR)FALSE;
}

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

DWORD WINAPI Server(LPVOID arg)
{
	int retval;

	// 윈속 초기화
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// socket()
	listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET) err_quit(L"socket()");

	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit(L"bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit(L"listen()");

	// 데이터 통신에 사용할 변수
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
		// 접속한 클라이언트 정보 출력
		DisplayText("[TCP 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\r\n",
			inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

		// 스레드 생성
		hThread = CreateThread(NULL, 0, ProcessClient,
			(LPVOID)client_sock, 0, NULL);
		if (hThread == NULL) { closesocket(client_sock); }
		else { CloseHandle(hThread); }
	}
	return 0;
}


DWORD WINAPI ProcessClient(LPVOID arg)
{
	SOCKET client_sock = (SOCKET)arg;
	int retval;
	SOCKADDR_IN clientaddr;
	int addrlen;
	int len, lenName;
	char data[BUFSIZE], fileName[BUFSIZE];
	FILE* fp;


	// 클라이언트 정보 얻기
	addrlen = sizeof(clientaddr);
	getpeername(client_sock, (SOCKADDR*)&clientaddr, &addrlen);

	// 클라이언트와 데이터 통신
	while (1)
	{
		// (1) 파일 이름 받기
		// 데이터 받기 (고정 길이)
		retval = recvn(client_sock, (char*)&lenName, sizeof(int), 0);
		if (retval == SOCKET_ERROR)
		{
			err_display("recv()");
			break;
		}
		else if (retval == 0)
			break;

		// 데이터 받기 (가변 길이)
		retval = recvn(client_sock, fileName, lenName, 0);
		if (retval == SOCKET_ERROR)
		{
			err_display("recv()");
			break;
		}
		else if (retval == 0)
		{
			break;
		}
		fileName[lenName] = '\0';


		// (2) 파일 내용 받기
		fp = fopen(fileName, "wb");

		// 데이터 받기 (고정 길이)
		retval = recvn(client_sock, (char*)&len, sizeof(int), 0);
		if (retval == SOCKET_ERROR)
		{
			err_display("recv()");
			fclose(fp);
			break;
		}
		else if (retval == 0)
		{
			fclose(fp);
			break;
		}
		HWND progress = GetProgress();

		// 데이터 받기 (가변 길이)
		int readSize = 0, leftSize = len;
		while (leftSize > 0)
		{
			readSize = BUFSIZE;
			if (leftSize < readSize)
				readSize = leftSize;

			retval = recvn(client_sock, data, readSize, 0);
			if (retval == SOCKET_ERROR)
			{
				err_display("recv()");
				break;
			}
			else if (retval == 0)
			{
				break;
			}

			// 받은 데이터 출력
			leftSize -= retval;

			fwrite(data, sizeof(char), retval, fp);
			if (progress != NULL)
			{
				SendMessage(progress, PBM_SETPOS, (int)((len - leftSize) / (float)len * 100), 0);
			}
			//printf("전송률: %.2f %%", (float)(len - leftSize) / (float)len * 100);
		}
		DisplayText("[TCP/%s:%d] '%s'를 저장하였습니다.\r\n", inet_ntoa(clientaddr.sin_addr),
			ntohs(clientaddr.sin_port), fileName);
		BackProgress(progress);

		fclose(fp);
	}
	// closesocket()
	closesocket(client_sock);
	DisplayText("[TCP 서버] 클라이언트 종료: IP 주소=%s, 포트 번호=%d\r\n",
		inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

	return 0;
}


// 소켓 함수 오류 출력 후 종료
void err_quit(const TCHAR* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

// 소켓 함수 오류 출력
void err_display(const char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	DisplayText("[%s] %ls\r\n", msg, (LPTSTR)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

void DisplayText(const char* msg, ...)
{
	char szBuf[1024] = { 0, };

	va_list lpStart;
	va_start(lpStart, msg);


	vsprintf(szBuf, msg, lpStart);

	TCHAR szUniCode[BUFSIZE] = { 0, };
	MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, szBuf, strlen(szBuf), szUniCode, BUFSIZE);

	SendMessage(hDebug, EM_REPLACESEL, TRUE, (LPARAM)szUniCode);

	va_end(lpStart);
}

HWND GetProgress()
{
	HWND returnVal = NULL;
	EnterCriticalSection(&cs);
	if (UseProgress[0] == false)
	{
		UseProgress[0] = true;
		returnVal = hProgress[0];
	}
	else if (UseProgress[1] == false)
	{
		UseProgress[1] = true;
		returnVal = hProgress[1];
	}
	LeaveCriticalSection(&cs);
	return returnVal;
}

void BackProgress(HWND val)
{
	EnterCriticalSection(&cs);
	if (val == hProgress[0])
		UseProgress[0] = false;
	else if (val == hProgress[1])
		UseProgress[1] = false;
	LeaveCriticalSection(&cs);
}