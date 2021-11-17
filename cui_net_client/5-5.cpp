#pragma comment(lib, "ws2_32")
#include <WinSock2.h>
#include <stdlib.h>
#include <stdio.h>

#define SERVERIP	"127.0.0.1"
#define SERVERPORT	9000
#define BUFSIZE		512

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
	printf("[%s] %s", msg, (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

int main(int argc, char* argv[])
{
	int retval;

	// ���� �ʱ�ȭ
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


	// ������ ��ſ� ����� ����
	char Path[BUFSIZE];
	int len;
	char data[BUFSIZE];
	FILE* fp;

	char* FileName;
	int lenName;

	printf("���� ������ �Է����ּ���\n");
	scanf("%s", Path);

	fp = fopen(Path, "rb");
	if (fp == NULL)
	{
		printf("�б� ����\n");
		return 1;
	}

	// ���� ũ�� �޾ƿ��� �޸𸮿� ����
	fseek(fp, 0, SEEK_END);
	len = ftell(fp);


	// ������ ������ ���
	
	// (1) ���� �̸� ������
	// ��η� ���� ���� �̸� �˾Ƴ���
	FileName = strrchr(Path, '\\');
	FileName += 1;
	if (FileName == NULL)
	{
		printf("���� �̸� ã�� ����");
		closesocket(sock);
		WSACleanup();
		fclose(fp);
		exit(1);
	}
	lenName = strlen(FileName);

	// ������ ������ (��������)
	retval = send(sock, (char*)&lenName, sizeof(int), 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		closesocket(sock);
		WSACleanup();
		fclose(fp);
		exit(1);
	}

	// ������ ������
	retval = send(sock, FileName, lenName, 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		closesocket(sock);
		WSACleanup();
		fclose(fp);
		exit(1);
	}
	printf("[TCP Ŭ���̾�Ʈ] %d+%d����Ʈ�� ���½��ϴ�.\n", sizeof(int), retval);



	// (2) ���� ���� ������
	// ������ ������ (���� ����)
	retval = send(sock, (char*)&len, sizeof(int), 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("send()");
		closesocket(sock);
		WSACleanup();
		fclose(fp);
		exit(1);
	}

	// ������ ������
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
	
	printf("[TCP Ŭ���̾�Ʈ] %d+%d����Ʈ�� ���½��ϴ�.\n", sizeof(int), len);

	// closesocket
	closesocket(sock);

	// ���� ����
	WSACleanup();

	fclose(fp);
	return 0;
}