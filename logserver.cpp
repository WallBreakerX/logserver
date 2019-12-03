#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <process.h> 
#include <conio.h>
#pragma comment(lib,"ws2_32.lib")

#define DEFAULTADDR	"127.0.0.1"
#define DEFAULTPORT 8080


typedef struct {
	char filename[32];
	char content[64];
	int content_len;
	int trans_stat;
}datapkg;


struct linkinfo{
	int fd;
	FILE* fp;
	char filename[32];
	struct linkinfo* next;
};
typedef struct linkinfo Linkinfo;

int ThreadAlive = 1;
int LinkLen = 0;
Linkinfo* LinkHead = NULL;

int AddLink(SOCKET fd) {//创建新用户链表节点
	Linkinfo* p, * pt, * phead = NULL;
	p = (Linkinfo*)malloc(sizeof(Linkinfo));//为链表创建空间 
	if (p == NULL)
		return -1;
	p->fd = fd;
	p->fp = NULL;
	p->next = NULL;
	if (LinkHead == NULL) {
		LinkHead = p;
	}
	else {
		phead = LinkHead;
		for (int i = 0; i < (LinkLen - 1); i++)
			phead = phead->next;
		phead->next = p;
	}
	++LinkLen;

	return 0;
}
void DelLink(SOCKET fd) {
	Linkinfo* plink, * pformer;
	closesocket(fd);
	plink = LinkHead;
	int i;
	for (i = 0; i < LinkLen; i++) {
		if ((fd == plink->fd) && (i == 0)) {
			if (plink->fp != NULL) {
				fclose(plink->fp);
				plink->fp = NULL;
			}
			LinkHead = plink->next;
			free(plink);
			--LinkLen;
			return;
		}
		pformer = plink;
		plink = plink->next;
		if ((fd == plink->fd) && (plink->next == NULL)) {
			if (plink->fp != NULL) {
				fclose(plink->fp);
				plink->fp = NULL;
			}
			free(plink);
			plink = NULL;
			--LinkLen;
			return;
		}
		else if (fd == plink->fd) {
			if (plink->fp != NULL) {
				fclose(plink->fp);
				plink->fp = NULL;
			}
			pformer->next = plink->next;
			free(plink);
			--LinkLen;
			return;
		}
	}
}

void EmptyLink(void) {
	Linkinfo* plink = LinkHead;
	for (int i = 0; i < LinkLen; i++) {
		Linkinfo* pformer = plink;
		plink = plink->next;
		fclose(pformer->fp);
		closesocket(pformer->fd);
		free(pformer);
	}
	LinkHead = NULL;

	return;
}
Linkinfo* Selectfd(void) {//select函数
	struct timeval timecnt;
	timecnt.tv_usec = 0;//初始化select函数等待时间，置0
	timecnt.tv_sec = 0;

	Linkinfo* plink = LinkHead;
	int max_fd = 0;
	fd_set FDlist;
	FD_ZERO(&FDlist);
	for (int i = 0; i < LinkLen; i++) {
		FD_SET(plink->fd, &FDlist);
		max_fd = plink->fd > max_fd ? plink->fd : max_fd;
		plink = plink->next;
	}
	if (select(max_fd + 1, &FDlist, NULL, NULL, &timecnt) > 0) {
		plink = LinkHead;
		for (int i = 0; i < LinkLen; i++) {
			if (FD_ISSET(plink->fd, &FDlist)) {
				return plink;
			}
			plink = plink->next;
		}
	}

	return NULL;
}

void serverthread(void* args) {
	Linkinfo* plink;
	datapkg *recvpkg;

	while (ThreadAlive) {
		plink = Selectfd();
		if (plink) {
			char recvstr[sizeof(datapkg)] = { 0 };
			if (recv(plink->fd, recvstr, sizeof(datapkg), NULL) > 0) {
				recvpkg = (datapkg*)recvstr;
				if (recvpkg->trans_stat == 0) {
					char path[64];
					sprintf(path, ".\\%s", recvpkg->filename);
					if (fopen_s(&plink->fp, path, "w+") != 0) {
						printf("Open file failed\n");
						DelLink(plink->fd);//删除节点
						continue;
					}
					strcpy(plink->filename, recvpkg->filename);
					continue;
				}
				else if (recvpkg->trans_stat == 1) {
					fwrite(recvpkg->content, recvpkg->content_len, 1, plink->fp);
					continue;
				}
				else if (recvpkg->trans_stat == -1) {
					printf("Finished\nFilepath: .\\%s\n", plink->filename);
					DelLink(plink->fd);//删除节点并关闭文件
					continue;
				}
			}
			else {
				fprintf(stderr, "\nDisconnected :%s", strerror(errno));
				DelLink(plink->fd);//删除节点并关闭文件
				continue;
			}
		}
	}

	return;
}


int main(int argc, char* argv[])
{
	char ADDR[32] = {0};
	int PORT = 0;
	if (argc == 1) {
		strcpy(ADDR, DEFAULTADDR);
		PORT = DEFAULTPORT;
	}
	else if (argc == 3) {
		strcpy(ADDR, argv[1]);
		PORT = atoi(argv[2]);
	}
	else {
		printf(".\\logserver (default IP:127.0.0.1 Port:8080)\nor\n.\\logserver IP PORT\n");
		return 0;
	}
	printf("==========SERVER==========\n");
	printf("IP:%s\tPORT:%d\n", ADDR, PORT);
	
	fd_set clientset;
	FD_ZERO(&clientset);
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
	unsigned long ul = 1;
	if (ioctlsocket(slisten, FIONBIO, (unsigned long*)& ul) == SOCKET_ERROR) {
		printf("Configure Socket failed\n");
		return 0;
	}

	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(PORT);
	sin.sin_addr.S_un.S_addr = inet_addr(ADDR);

	if (bind(slisten, (LPSOCKADDR)& sin, sizeof(sin)) == SOCKET_ERROR)
	{
		closesocket(slisten);
		printf("bind error !");
		return 0;
	}

	if (listen(slisten, 64) == SOCKET_ERROR)
	{
		closesocket(slisten);
		printf("listen error !");
		return 0;
	}

	SOCKET sClient;
	sockaddr_in remoteAddr;
	int nAddrlen = sizeof(remoteAddr);

	if (_beginthread(serverthread, 0, NULL) == NULL) {
		printf("Create thread failed\n");
		return 0;
	}

	while (true)
	{
		if (_kbhit()) {
			int key = _getch();
			if (key == 27) {
				ThreadAlive = 0;
				break; 
			}
		}
		SOCKET client = accept(slisten, (SOCKADDR*)& remoteAddr, &nAddrlen);
		if (client == SOCKET_ERROR)
			continue;
		if (client == INVALID_SOCKET)
		{
			printf("accept error !");
			continue;
		}
		printf("\nStart Receiving files...\n");
		AddLink(client);
	}

	closesocket(slisten);
	EmptyLink();
	WSACleanup();

	return 0;
}