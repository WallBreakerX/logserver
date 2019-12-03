#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <process.h> 
#pragma comment(lib,"ws2_32.lib")

typedef struct {
	char filename[32];
	char content[64];
	int content_len;
	int trans_stat;
}datapkg;


typedef struct {
	int fd;
	FILE* fp;
	struct Linkinfo* next;
}Linkinfo;

int LinkLen = 0;
Linkinfo* LindHead = NULL;


void serverthread(void* arg) {
	printf("传输中...\n");
	SOCKET* clientsocket = (SOCKET*)arg;

	datapkg* recvpkg = NULL;
	FILE* fp = NULL;
	char filename[64];
	int i;
	int recvTimeout = 2 * 1000;
	setsockopt(*clientsocket, SOL_SOCKET, SO_RCVTIMEO, (char*)& recvTimeout, sizeof(int));

	while (TRUE) {
		char recvdata[sizeof(datapkg)] = { 0 };
		if (recv(*clientsocket, recvdata, sizeof(datapkg), NULL) == 0) {
			closesocket(*clientsocket);
			free(clientsocket);
			if (fp != NULL)
				fclose(fp);
			printf("接收超时，请客户端重新发起连接\n");
			return;
		}

		recvpkg = (datapkg*)recvdata;
		if (recvpkg->trans_stat == 0) {//第一次发送
			sprintf_s(filename, "./%s", recvpkg->filename);
			errno_t err;
			fopen_s(&fp, filename, "w");
		}
		else if (recvpkg->trans_stat == 1) {//发送中
			fwrite(recvpkg->content, recvpkg->content_len, 1, fp);
		}
		else if (recvpkg->trans_stat == -1) {//发送结束
			//fwrite(recvpkg->content, recvpkg->content_len, 1, fp);
			fclose(fp);
			break;
		}
	}
	closesocket(*clientsocket);
	free(clientsocket);
	printf("\n传输完毕\nlog文件路径: %s\n", filename);

	return;
}


int main(int argc, char* argv[])
{
	if (argc != 3) {
		printf(".\\logserver IP PORT\n");
		return 0;
	}
	printf("==========SERVER==========\n");
	fd_set fd;
	FD_ZERO(&fd);
	char* ADDR = argv[1];
	int PORT = atoi(argv[2]);

	WORD sockVersion = MAKEWORD(2, 2);
	WSADATA wsaData;
	if (WSAStartup(sockVersion, &wsaData) != 0)
	{
		return 0;
	}

	SOCKET slisten = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (slisten == INVALID_SOCKET)
	{
		printf("socket error !");
		return 0;
	}
	char allow = 1;
	setsockopt(slisten, SOL_SOCKET, SO_REUSEADDR, &allow, sizeof(char));

	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(PORT);
	sin.sin_addr.S_un.S_addr = inet_addr(ADDR);

	if (bind(slisten, (LPSOCKADDR)& sin, sizeof(sin)) == SOCKET_ERROR)
	{
		printf("bind error !");
	}

	if (listen(slisten, 5) == SOCKET_ERROR)
	{
		printf("listen error !");
		return 0;
	}

	SOCKET sClient;
	sockaddr_in remoteAddr;
	int nAddrlen = sizeof(remoteAddr);

	while (true)
	{
		SOCKET* client = (SOCKET*)malloc(sizeof(SOCKET));
		*client = accept(slisten, (SOCKADDR*)& remoteAddr, &nAddrlen);
		if (*client == INVALID_SOCKET)
		{
			free(client);
			printf("accept error !");
			continue;
		}
		printf("客户端接入\n");

		if (_beginthread(serverthread, 0, client) == NULL) {
			printf("Create thread failed\n");
			free(client);
			continue;
		}
	}
	closesocket(slisten);
	WSACleanup();

	return 0;
}