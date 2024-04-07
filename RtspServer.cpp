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

#include "rtp.h"

#pragma warning( disable : 4996 )

#define SERVER_PORT      8554

#define SERVER_RTP_PORT  55532
#define SERVER_RTCP_PORT 55533

#define H264_FILENAME "../../../data/test.h264"


int createTcpSocket() {
	int on = 1;

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;

	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
	return fd;
}

int createUdpSocket() {
	int on = 1;

	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return -1;

	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
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
		"a=rtpmap:96 H264/90000\r\n"
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

int startCode3(uint8_t *frame_buf) {
	//return frame_buf[0] == 0 && frame_buf[1] == 0 \
		&& frame_buf[2] == 1;
	if (frame_buf[0] == 0 && frame_buf[1] == 0 \
		&& frame_buf[2] == 1)
		return 1;
	else
		return 0;
}

int startCode4(uint8_t *frame_buf) {
	//return frame_buf[0] == 0 && frame_buf[1] == 0 \
		&& frame_buf[2] == 0 && frame_buf[3] == 1;
	if (frame_buf[0] == 0 && frame_buf[1] == 0 \
		&& frame_buf[2] == 0 && frame_buf[3] == 1)
		return 1;
	else
		return 0;
}

int readPacktFromH264(FILE *fp, uint8_t *frame_buf, int buf_size) {
	int start_code;

	int nr = fread(frame_buf, 1, buf_size, fp);
	if (nr <= 0) {
		printf("Fread failed.\n");
		return -1;
	}
	
	start_code = startCode3(frame_buf) ? 3 : \
		(startCode4(frame_buf) ? 4 : -1);
	if (start_code == -1)
		return -1;

	// find next start code
	int frame_end = -1;
	for (int i = start_code; i < nr - 3; i++) {
		if (startCode3(frame_buf + i) || startCode4(frame_buf + i)) {
			frame_end = i;
			break;
		}
	}

	if (frame_end > 0)
		fseek(fp, frame_end - nr, SEEK_CUR);
	return frame_end;
}

int rtpSendH264Frame(int rtp_fd, const char *client_ip, int client_rtp_port, \
	struct RtpPacket *rtp_pkt, uint8_t *frame_buf, int frame_size) {
	uint8_t nalu_type;		// nalu第一个字节
	int ret = -1;
	int send_bytes = 0;

	nalu_type = frame_buf[0];
	printf("frameSize = %d\n", frame_size);
	
	if (frame_size <= RTP_MAX_PKT_SIZE) {
		// nalu长度小于最大rtp包大小； 单一NALU单元模式
		memcpy(rtp_pkt->payload, frame_buf, frame_size);
		ret = rtpSendPacketOverUdp(rtp_fd, client_ip, client_rtp_port, \
				rtp_pkt, frame_size);
		if (ret < 0)
			return -1;

		rtp_pkt->rtp_header.seq++;
		send_bytes += ret;
		// 如果是SPS，PPS，则无需加时间戳
		if ((nalu_type & 0x1F) == 7 || (nalu_type & 0x1F) == 8)
			goto out;
	}
	else {
		// nalu长度大于最大rtp包大小； 分片模式

		//* 0  1  2
		//* 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3
		//*+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
		//*|FU indicator  |FU header      | FU payload ... | 
		//*+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
		int pkt_num = frame_size / RTP_MAX_PKT_SIZE;
		int remain_pkt_size = frame_size % RTP_MAX_PKT_SIZE;
		int i, pos = 1;

		// 发送分包
		for (i = 0; i < pkt_num; i++) {
			rtp_pkt->payload[0] = (nalu_type & 0x60) | 28;
			rtp_pkt->payload[1] = nalu_type & 0x1F;

			if (i == 0)
				rtp_pkt->payload[1] |= 0x80;
			else if (i == pkt_num - 1 && remain_pkt_size == 0)
				rtp_pkt->payload[1] |= 0x40;

			memcpy(rtp_pkt->payload + 2, frame_buf + pos, RTP_MAX_PKT_SIZE);
			ret = rtpSendPacketOverUdp(rtp_fd, client_ip, client_rtp_port, rtp_pkt, RTP_MAX_PKT_SIZE + 2);
			if (ret < 0)
				return -1;

			rtp_pkt->rtp_header.seq++;
			send_bytes += ret;
			pos += RTP_MAX_PKT_SIZE;
		}

		// 发送最后一个包
		if (remain_pkt_size > 0) {
			rtp_pkt->payload[0] = (nalu_type & 0x60) | 28;
			rtp_pkt->payload[1] = nalu_type & 0x1F;
			rtp_pkt->payload[1] |= 0x40;

			memcpy(rtp_pkt->payload + 2, frame_buf + pos, remain_pkt_size);
			ret = rtpSendPacketOverUdp(rtp_fd, client_ip, client_rtp_port, rtp_pkt, remain_pkt_size + 2);
			if (ret < 0)
				return -1;

			rtp_pkt->rtp_header.seq++;
			send_bytes += ret;
		}
	}

	rtp_pkt->rtp_header.timestamp += 90000 / 25;

out:
	return send_bytes;
}

static void doClient(int connfd, const char* client_ip, const int client_port) {
	char sbuf[4096], rbuf[4096];
	int recv_len;
	char method[50];
	char url[256];
	char version[40];
	int cseq;
	int ret;

	int client_rtp_port, client_rtcp_port;
	int rtp_fd = -1, rtcp_fd = -1;

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

			rtp_fd = createUdpSocket();
			rtcp_fd = createUdpSocket();
			if (rtp_fd < 0 || rtcp_fd < 0) {
				printf("Failed to create udp socket\n");
				break;
			}

			if (bindSockAddr(rtp_fd, "0.0.0.0", SERVER_RTP_PORT) < 0 || \
				bindSockAddr(rtcp_fd, "0.0.0.0",  SERVER_RTCP_PORT) < 0) {
				printf("Failed to bind udp socket\n");
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

			// open h264 file
			FILE* fp = fopen(H264_FILENAME, "rb");
			if (!fp) {
				printf("Open %s failed.\n", H264_FILENAME);
				break;
			}

			int start_code;
			uint8_t* frame_buf = (uint8_t *)malloc(500000);
			int frame_bufsz;	// frame_buf中有效数据长度
			int frame_size;		// 单个frame的大小
			struct RtpPacket* rtp_pkt = (struct RtpPacket*)malloc(500000);
			rtpHeaderInit(rtp_pkt, 0, 0, 0, RTP_VERSION, RTP_PAYLOAD_TYPE_H264, 0, \
				0, 0, 0x88923423);

			while (true){
				frame_bufsz = readPacktFromH264(fp, frame_buf, 500000);
				if (frame_bufsz <= 0) {
					printf("读取 %s 结束， pkt_buf_size = %d\n", H264_FILENAME, frame_buf);
					break;
				}

				// 判断start code是 001 or 0001
				if (startCode3(frame_buf))
					start_code = 3;
				else
					start_code = 4;

				frame_size = frame_bufsz - start_code;
				// rtp send pkt
				ret = rtpSendH264Frame(rtp_fd, client_ip, client_rtp_port, \
					rtp_pkt, frame_buf + start_code, frame_size);
				if (ret < 0) {
					printf("RTP send failed\n");
				}
				
				Sleep(40);
			}

			free(frame_buf);
			free(rtp_pkt);
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
