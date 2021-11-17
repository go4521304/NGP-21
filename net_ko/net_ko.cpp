// net_ko.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//

#include "framework.h"
#include "net_ko.h"

#define SERVERIP	"127.0.0.1"
#define SERVERPORT	9000
#define BUFSIZE 512

// 전역 변수:
HWND hEdit, hDebug, hFileBtn, hSendBtn, hProgress;

TCHAR fileName[BUFSIZE];
WSADATA wsa;
SOCKET sock;
HANDLE hThread;
HANDLE hSendEvent;

// 이 코드 모듈에 포함된 함수의 선언을 전달합니다:
INT_PTR CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
DWORD WINAPI ProcessClient(LPVOID arg);
void err_quit(const TCHAR* msg);
void err_display(const char* msg);
void DisplayText(const char* msg, ...);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);


	return  DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG), NULL, DlgProc);
}


INT_PTR CALLBACK DlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		hEdit = GetDlgItem(hDlg, IDC_EDIT);
		hDebug = GetDlgItem(hDlg, IDC_DEBUG);
		hFileBtn = GetDlgItem(hDlg, IDC_FILE);
		hSendBtn = GetDlgItem(hDlg, IDC_SEND);
		hProgress = GetDlgItem(hDlg, IDC_PROGRESS);
		SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

		hSendEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (hSendEvent == NULL)
			PostQuitMessage(0);

		hThread = CreateThread(NULL, 0, ProcessClient, NULL, 0, NULL);
		if (hThread == NULL)
			PostQuitMessage(0);

		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_FILE)
		{
			OPENFILENAME OFN;
			memset(&OFN, 0, sizeof(OPENFILENAME));
			OFN.lStructSize = sizeof(OPENFILENAME);
			OFN.hwndOwner = hDlg;
			OFN.lpstrFilter = L"All Files(*.*)\0*.*\0";

			OFN.lpstrFile = fileName;
			OFN.nMaxFile = BUFSIZE;

			if (0 != GetOpenFileName(&OFN))
			{
				SetWindowText(hEdit, fileName);
				EnableWindow(hSendBtn, TRUE);
			}

			return (INT_PTR)TRUE;
		}
		if (LOWORD(wParam) == IDC_SEND)
		{
			SetEvent(hSendEvent);
			SendMessage(hProgress, PBM_SETPOS, 0, 0);
		}
		break;

	case WM_CLOSE:
		PostQuitMessage(0);
		return (INT_PTR)TRUE;

	case WM_DESTROY:
		CloseHandle(hSendEvent);
		CloseHandle(hThread);
		closesocket(sock);
		WSACleanup();
		return (INT_PTR)TRUE;

	}
	return (INT_PTR)FALSE;
}

DWORD WINAPI ProcessClient(LPVOID arg)
{
	int retval;
	// 윈속 초기화
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// socket()
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) err_quit(L"socket()");

	// connet
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(SERVERIP);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = connect(sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit(L"connet()");
	SendMessage(hDebug, EM_REPLACESEL, TRUE, (LPARAM)L"서버에 접속 했습니다.\r\n");


	// 데이터 통신에 사용할 변수
	int len;
	char data[BUFSIZE];

	char file[BUFSIZE];
	int lenName;
	FILE* fp;

	while (1)
	{
		WaitForSingleObject(hSendEvent, INFINITE);
		EnableWindow(hSendBtn, FALSE);

		GetWindowText(hEdit, fileName, BUFSIZE);

		WideCharToMultiByte(CP_ACP, 0, fileName, BUFSIZE, file, BUFSIZE, NULL, NULL);

		fp = fopen(file, "rb");
		if (fp == NULL)
		{
			DisplayText("읽기 오류\r\n");
			continue;
		}
		// 파일 크기 받아오고 메모리에 저장
		fseek(fp, 0, SEEK_END);
		len = ftell(fp);

		// 서버와 데이터 통신

		// (1) 파일 이름 보내기
		// 경로로 부터 파일 이름 알아내기
		char* sendFile = strrchr(file, '\\');
		sendFile += 1;
		if (sendFile == NULL)
		{
			DisplayText("파일 이름 찾기 오류\r\n");
			continue;
		}
		lenName = strlen(sendFile);

		// 데이터 보내기 (고정길이)
		retval = send(sock, (char*)&lenName, sizeof(int), 0);
		if (retval == SOCKET_ERROR)
		{
			err_display("send()");
			continue;
		}

		// 데이터 보내기
		retval = send(sock, sendFile, lenName, 0);
		if (retval == SOCKET_ERROR)
		{
			err_display("send()");
			continue;
		}
		DisplayText("[TCP 클라이언트] %d+%d바이트를 보냈습니다.\r\n", sizeof(int), retval);


		// (2) 파일 내용 보내기
		// 데이터 보내기 (고정 길이)
		retval = send(sock, (char*)&len, sizeof(int), 0);
		if (retval == SOCKET_ERROR)
		{
			err_display("send()");
			continue;
		}

		// 데이터 보내기
		fseek(fp, 0, SEEK_SET);

		int readSize = 0, leftSize = len;
		while (leftSize > 0)
		{
			readSize = BUFSIZE;
			if (leftSize < readSize)
				readSize = leftSize;


			fread(data, sizeof(char), readSize, fp);
			retval = send(sock, data, readSize, 0);
			if (retval == SOCKET_ERROR)
			{
				err_display("send()");
				break;
			}

			leftSize -= retval;

			SendMessage(hProgress, PBM_SETPOS, (int)((len - leftSize) / (float)len * 100), 0);
		}
		DisplayText("[TCP 클라이언트] %d+%d바이트를 보냈습니다.\r\n", sizeof(int), len);

		ResetEvent(hSendBtn);
		fclose(fp);
	}
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