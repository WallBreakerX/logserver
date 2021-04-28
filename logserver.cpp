/*
 * ===========================================================================================
 * = COPYRIGHT
 *          PAX Computer Technology(Shenzhen) CO., LTD PROPRIETARY INFORMATION
 *   This software is supplied under the terms of a license agreement or nondisclosure
 *   agreement with PAX Computer Technology(Shenzhen) CO., LTD and may not be copied or
 *   disclosed except in accordance with the terms in that agreement.
 *     Copyright (C) 2020-? PAX Computer Technology(Shenzhen) CO., LTD All rights reserved.
 * Description: // Detail description about the function of this module,
 *             // interfaces with the other modules, and dependencies.
 * Revision History:
 * Date                  Author                 Action
 * 2019/12/06          Zeng Tianhao             Create
 * ===========================================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <process.h> 
#include <conio.h>
#include<time.h>
#pragma comment(lib,"ws2_32.lib")

#define DEFAULTADDR	"127.0.0.1"
#define DEFAULTPORT 7070
#define SIZE		512

typedef struct {
	char filename[32];//
	char content[64];
	int content_len;
	int trans_stat;//0 1 -1
}datapkg;


struct linkinfo{
	int fd;
	int line;
	FILE* fp;
	char filename[64];
	char buffer[SIZE];
	int buffer_len;
	struct linkinfo* next;
};
typedef struct linkinfo Linkinfo;


int ThreadAlive = 1;
int LinkLen = 0;
Linkinfo* LinkHead = NULL;


void ResetLink(Linkinfo* link) {//清空结点数据（非删除）
	if (link->fp != NULL) {
		fclose(link->fp);
		link->fp = NULL;
	}
	memset(link->filename, 0, 64);
	memset(link->buffer, 0, SIZE);
	link->buffer_len = 0;
	link->line = 0;
}

int AddLink(SOCKET fd) {//创建新客户端链表节点
	Linkinfo* p, * phead = NULL;
	p = (Linkinfo*)malloc(sizeof(Linkinfo));
	if (p == NULL)
		return -1;
	p->fp = NULL;
	memset(p->filename, 0, 64);
	memset(p->buffer, 0, SIZE);
	p->buffer_len = 0;
	p->line = 0;
	p->fd = fd;

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

void DelLink(SOCKET fd) {//删除结点
	Linkinfo* plink, * pformer;
	closesocket(fd);
	fprintf(stderr, "%d Disconnected:%s\n", fd, strerror(errno));
	plink = LinkHead;
	int i;
	for (i = 0; i < LinkLen; i++) {
		if ((fd == plink->fd) && (i == 0)) {
			LinkHead = plink->next;
			ResetLink(plink);
			free(plink);
			plink = NULL;
			--LinkLen;
			break;
		}
		pformer = plink;
		plink = plink->next;
		if ((fd == plink->fd) && (plink->next == NULL)) {
			pformer->next = NULL;
			ResetLink(plink);
			free(plink);
			plink = NULL;
			--LinkLen;
			break;
		}
		else if (fd == plink->fd) {
			pformer->next = plink->next;
			ResetLink(plink);
			free(plink);
			plink = NULL;
			--LinkLen;
			break;
		}
	}
}

void EmptyLink(void) {//清空并删除链表
	Linkinfo* plink = LinkHead;
	for (int i = 0; i < LinkLen; i++) {
		Linkinfo* pformer = plink;
		plink = plink->next;
		if (pformer->fp != NULL)
			fclose(pformer->fp);
		closesocket(pformer->fd);
		free(pformer);
		pformer = NULL;
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

void Setfilename(char* filename, char* linkfilename) {
	time_t ptime;
	struct tm* p;
	time(&ptime);
	p = gmtime(&ptime);
	char tmpstr[64] = { 0 };
	memcpy(tmpstr, filename, 64);
	sprintf(filename, ".\\%s_%d-%02d-%02d-%02d_%02d_%02d.txt", tmpstr, 1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday, 8 + p->tm_hour, p->tm_min, p->tm_sec);

	memcpy(linkfilename, filename, 64);
	printf("filename=%s", linkfilename);
}


void Writetofile(Linkinfo* link) {
	for (int i = 0; i < link->buffer_len; i++) {
		if (link->buffer[i] != 0x00) {
			int ret = fwrite(&(link->buffer[i]), 1, 1, link->fp);
		}
	}
	memset(link->buffer, 0, SIZE);
	link->buffer_len = 0;
}


void LinkAddData(Linkinfo* link, char data) {
	link->buffer[link->buffer_len] = data;
	link->buffer_len += 1;
}



void serverthread(void* args) {
	Linkinfo* plink;
	datapkg *recvpkg;

	while (ThreadAlive) {
		plink = Selectfd();
		if (plink) {
			char recvstr[SIZE] = { 0 };
			if (recv(plink->fd, recvstr, sizeof(recvstr), NULL) > 0) {
				printf("recvstr= %s\n", recvstr);
				if (plink->line == 0) {
					char header[64] = { 0 }, tag[64] = { 0 };
					sscanf(recvstr, "<%[a-z]>{\"tag\":\"%[^\"]\"}", header, tag);
					printf("===== tag =====\n%s\n", tag);
					if (strcmp(header, "logheader") == 0) {
						Setfilename(tag, plink->filename);
						if (fopen_s(&plink->fp, plink->filename, "w") != 0) {
							fprintf(stderr, "Open %s failed:%s, retry.\n", plink->filename, strerror(errno));
							ResetLink(plink);
							continue;
						}
						for (int i = 0; i < SIZE; i++) {
							if (recvstr[i] == '\n' && plink->line == 0)
								plink->line = 1;
							else if (plink->line > 0) {
								if (recvstr[i] != '\n') {
									char end[64] = { 0 };
									sscanf(plink->buffer, "%*[^<]<%[a-z]>%*[^>]", end);
									if (strcmp(end, "logend") == 0) {
										printf("===== Receive over =====\n%s\n", plink->filename);
										ResetLink(plink);
										break;
									}
								}
								else
									Writetofile(plink);
								LinkAddData(plink, recvstr[i]);
							}
						}
						if (plink != NULL) {
							if (plink->line == 0) {
								ResetLink(plink);
								printf("Receive error, retry.\n");
							}
						}
					}
				}
				else {
					for (int i = 0; i < SIZE; i++) {
						if (recvstr[i] != '\n') {
							char end[64] = { 0 };
							sscanf(plink->buffer, "%*[^<]<%6[a-z]>%*[^>]", end);
							if (strcmp(end, "logend") == 0) {
								printf("===== Receive over =====\n%s\n", plink->filename);
								ResetLink(plink);
								break;
							}
						}
						else 
							Writetofile(plink);
						LinkAddData(plink, recvstr[i]);
					}
				}
			}
			else {
				DelLink(plink->fd);//删除节点并关闭文件
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
		printf(".\\logserver (Default IP:172.16.24.17 Port:12345)\nor\n.\\logserver IP PORT\n");
		return 0;
	}
	printf("==================SERVER==================\n");
	printf("IP:%s			PORT:%d\n", ADDR, PORT);
	printf("==========================================\n\n");
	
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
		printf("Socket error !\n");
		return 0;
	}
	char allow = 1;
	setsockopt(slisten, SOL_SOCKET, SO_REUSEADDR, &allow, sizeof(char));
	unsigned long ul = 1;
	if (ioctlsocket(slisten, FIONBIO, (unsigned long*)& ul) == SOCKET_ERROR) {
		printf("Configure socket failed\n");
		return 0;
	}

	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(PORT);
	sin.sin_addr.S_un.S_addr = inet_addr(ADDR);

	if (bind(slisten, (LPSOCKADDR)& sin, sizeof(sin)) == SOCKET_ERROR)
	{
		closesocket(slisten);
		printf("Bind error !\n");
		return 0;
	}

	if (listen(slisten, 64) == SOCKET_ERROR)
	{
		closesocket(slisten);
		printf("Listen error !\n");
		return 0;
	}

	int nAddrlen = sizeof(sin);

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
		SOCKET client = accept(slisten, (SOCKADDR*)& sin, &nAddrlen);
		if (client == SOCKET_ERROR)
			continue;
		if (client == INVALID_SOCKET)
		{
			printf("Accept error !\n");
			continue;
		}
		printf("\nStart receiving files...\n");
		AddLink(client);
	}

	EmptyLink();
	closesocket(slisten);
	WSACleanup();

	return 0;
}