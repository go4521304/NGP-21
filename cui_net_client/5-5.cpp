#pragma comment(lib, "ws2_32")
#include <WinSock2.h>
#include <stdlib.h>
#include <stdio.h>

#define SERVERIP	"127.0.0.1"
#define SERVERPORT	9000
#define BUFSIZE		512

// 소켓 함수 오류 출력 후 종료
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

// 소켓 함수 오류 출력
void err_display(const char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

int main(int argc, char* argv[])
{
	int retval;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// socket()
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) err_quit("socket()");

	// connet
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(SERVERIP);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = connect(sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("connet()");


	// 데이터 통신에 사용할 변수
	char Path[BUFSIZE];
	int len;
	char data[BUFSIZE];
	FILE* fp;

	char* FileName;
	int lenName;

	printf("보낼 파일을 입력해주세요\n");
	scanf("%s", Path);

	fp = fopen(Path, "rb");
	if (fp == NULL)
	{
		printf("읽기 오류\n");
		return 1;
	}

	// 파일 크기 받아오고 메모리에 저장
	fseek(fp, 0, SEEK_END);
	len = ftell(fp);


	// 서버와 데이터 통신
	
	// (1) 파일 이름 보내기
	// 경로로 부터 파일 이름 알아내기
	FileName = strrchr(Path, '\\');
	FileName += 1;
	if (FileName == NULL)
	{
		printf("파일 이름 찾기 오류");
		closesocket(sock);
		WSACleanup();
		fclose(fp);
		exit(1);
	}
	lenName = strlen(FileName);

	// 데이터 보내기 (고정길이)
	retval = send(sock, (char*)&lenName, sizeof(int), 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		closesocket(sock);
		WSACleanup();
		fclose(fp);
		exit(1);
	}

	// 데이터 보내기
	retval = send(sock, FileName, lenName, 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		closesocket(sock);
		WSACleanup();
		fclose(fp);
		exit(1);
	}
	printf("[TCP 클라이언트] %d+%d바이트를 보냈습니다.\n", sizeof(int), retval);



	// (2) 파일 내용 보내기
	// 데이터 보내기 (고정 길이)
	retval = send(sock, (char*)&len, sizeof(int), 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		closesocket(sock);
		WSACleanup();
		fclose(fp);
		exit(1);
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
			closesocket(sock);
			WSACleanup();
			fclose(fp);
			break;
		}

		leftSize -= retval;
	}
	
	printf("[TCP 클라이언트] %d+%d바이트를 보냈습니다.\n", sizeof(int), len);

	// closesocket
	closesocket(sock);

	// 윈속 종료
	WSACleanup();

	fclose(fp);
	return 0;
}