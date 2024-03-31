// RtspServer.cpp: 定义应用程序的入口点。
//

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <string>
#pragma comment(lib, "ws2_32.lib")
#include <stdint.h>

#pragma warning( disable : 4996 )

#define SERVER_PORT      8554

#define SERVER_RTP_PORT  55532
#define SERVER_RTCP_PORT 55533


int createTcpSocket() {
	int on = 1;

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;

	setsockopt(fd, 0xffff, SO_REUSEADDR, (const char*)&on, sizeof(on));
	return fd;
}

int bindSockAddr(int fd, const char* ip, const int port) {
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);				// host order -> net order
	addr.sin_addr.s_addr = inet_addr(ip);		// xxx.xxx.xxx.xxx -> longnumber ip

	if (bind(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr)) < 0)
		return -1;
	return 0;
}

int acceptClient(int fd, char *client_ip, int *client_port) {
	int connfd;
	socklen_t len = 0;
	struct sockaddr_in addr;

	len = sizeof(addr);
	memset(&addr, 0, len);

	connfd = accept(fd, (struct sockaddr*)&addr, &len);
	if (connfd < 0)
		return -1;

	strcpy(client_ip, inet_ntoa(addr.sin_addr));
	*client_port = ntohs(addr.sin_port);

	return connfd;
}

static int handle_OPTIONS(char* sbuf, int cseq) {
	sprintf(sbuf, "RTSP/1.0 200 OK\r\n"
        "CSeq: %d\r\n"
        "Public: OPTIONS, DESCRIBE, SETUP, PLAY\r\n\r\n", cseq);
	return 0;
}

static int handle_DESCRIBE(char *sbuf, int cseq, const char *url) {
	char sdp[4096];
	char local_ip[100];

	sscanf(url, "rtsp://%[^:]:", local_ip);

	sprintf(sdp, "v=0\r\n"
		"o=- 9%ld 1 IN IP4 %s\r\n"
		"t=0 0\r\n"
		"a=control:*\r\n"
		"m=video 0 RTP/AVP 96\r\n"
		"a=rtmap:96 H264/90000\r\n"
		"a=control:track0\r\n",
		time(NULL), local_ip);

	sprintf(sbuf, "RTSP/1.0 200 OK\r\n"
		"CSeq: %d\r\n"
		"Content-Base: %s\r\n"
		"Content-Type: application/sdp\r\n"
        "Content-Length: %d\r\n"
		"\r\n"
		"%s",
		cseq, url, strlen(sdp), sdp);

	return 0;
}

static int handle_SETUP(char *sbuf, int cseq, int client_rtp_port) {
	sprintf(sbuf, "RTSP/1.0 200 OK\r\n"
		"CSeq: %d\r\n"
		"Transport: RTP/AVP;client_port=%d-%d;server_port=%d-%d\r\n"
		"Session: 66334873\r\n"
		"\r\n" ,
		cseq, client_rtp_port, client_rtp_port+1, SERVER_RTP_PORT, SERVER_RTCP_PORT);
	return 0;
}


static int handle_PLAY(char *sbuf, int cseq) {
	sprintf(sbuf, "RTSP/1.0 200 OK\r\n"
		"CSeq: %d\r\n"
		"Range: npt=0.000-\r\n"
		"Session: 66334873; timeout=10\r\n"
		"\r\n", cseq);
	return 0;
}

static void doClient(int connfd, const char* client_ip, const int client_port) {
	char sbuf[4096], rbuf[4096];
	int recv_len;
	char method[50];
	char url[256];
	char version[40];
	int cseq;

	int client_rtp_port, client_rtcp_port;

	while (true) {
		recv_len = recv(connfd, rbuf, sizeof(rbuf) - 1, 0);
		if ( recv_len <= 0) {
			break;
		}
		rbuf[recv_len] = '\0';

		puts(">>>>>>>>>>>>>>>>>>>>>");
		printf("%s rbuf: %s", __FUNCTION__, rbuf);

		// parse request
		const char *delim = "\n";
		char* line = strtok(rbuf, delim);
		while (line) {
			if (strstr(line, "OPTIONS") || \
				strstr(line, "DESCRIBE") || \
				strstr(line, "SETUP") || \
				strstr(line, "PLAY")) {
				if (sscanf(line, "%s %s %s", method, url, version) != 3) {
					break;
				}
			}
			else if (strstr(line, "CSeq")) {
				if (sscanf(line, "CSeq: %d\r\n", &cseq) != 1) {
					break;
				}
			}
			else if (strncmp(line, "Transport:", strlen("Transport")) == 0) {
				if (sscanf(line, "Transport: RTP/AVP/UDP;unicast;client_port=%d-%d\r\n", \
						&client_rtp_port, &client_rtcp_port) != 2) {
					printf("Parse transport error.\n");
				}
			}
			line = strtok(NULL, delim);
		}

		// handle request
		if (strcmp(method, "OPTIONS") == 0) {
			if (handle_OPTIONS(sbuf, cseq)) {
				printf("Handle cmd OPTIONS error.\n");
				break;
			}
		}
		else if (strcmp(method, "DESCRIBE") == 0) {
			if (handle_DESCRIBE(sbuf, cseq, url)) {
				printf("Handle cmd DESCRIBE error.\n");
				break;
			}
		}
		else if (strcmp(method, "SETUP") == 0) {
			if (handle_SETUP(sbuf, cseq, client_rtp_port)) {
				printf("Handle cmd SETUP error.\n");
				break;
			}
		}
		else if (strcmp(method, "PLAY") == 0) {
			if (handle_PLAY(sbuf, cseq)) {
				printf("Handle cmd PLAY error.\n");
				break;
			}
		}
		else {
			printf("未定义的method： %s\n", method);
			break;
		}

		puts("<<<<<<<<<<<<<<<<<<<<<");
		printf("%s sbuf: %s", __FUNCTION__, sbuf);
		send(connfd, sbuf, strlen(sbuf), 0);

		// play
		if (strcmp(method, "PLAY") == 0) {
			printf("Start play!\n");
			printf("client ip: %s\n", client_ip);
			printf("client port: %d\n", client_port);
			while (true)
				Sleep(1);
			break;
		}

		memset(method, 0, sizeof(method) / sizeof(char));
		memset(url, 0, sizeof(url) / sizeof(char));
		cseq = 0;
	}
	
	closesocket(connfd);		// 关闭连接
}

int main()
{
	// 创建winsocket & listen
	WSADATA wsa_data;
	if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
		printf("Server socket start error.\n");
		return -1;
	}

	int svrfd = -1;
	svrfd = createTcpSocket();
	if (svrfd == -1) {
		printf("Create server socket failed.\n");
		return -1;
	}

	if (bindSockAddr(svrfd, "0.0.0.0", SERVER_PORT) == -1) {
		printf("Bind socket to addr failed.\n");
		return -1;
	}

	if (listen(svrfd, 10) < 0) {
		printf("Listen failed.\n");
		return -1;
	}
	printf("%s rtsp://127.0.0.1:%d\n", __FUNCTION__, SERVER_PORT);

	while (1) {
		int conn_fd;
		char client_ip[40];
		int client_port;

		if ((conn_fd = acceptClient(svrfd, client_ip, &client_port)) < 0) {
			printf("Accept client failed.\n");
			return -1;
		}
		printf("Accept client: ip=%s, port=%d\n", client_ip, client_port);

		doClient(conn_fd, client_ip, client_port);
	}

	closesocket(svrfd);


	// 处理client request
	return 0;
}
