#include <sys/types.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include "rtp.h"

void rtpHeaderInit(struct RtpPacket* pkt, uint8_t csrclen, uint8_t extension, \
	uint8_t padding, uint8_t version, uint8_t payload_type, uint8_t marker, \
	uint16_t seq, uint32_t timestamp, uint32_t ssrc) {
	
	pkt->rtp_header.csrclen = csrclen;
	pkt->rtp_header.extension = extension;
	pkt->rtp_header.padding = padding;
	pkt->rtp_header.version = version;
	pkt->rtp_header.payload_type = payload_type;
	pkt->rtp_header.marker = marker;
	pkt->rtp_header.seq = seq;
	pkt->rtp_header.timestamp = timestamp;
	pkt->rtp_header.ssrc = ssrc;
}

int rtpSendPacketOverTcp(int connfd, struct RtpPacket* pkt, uint32_t data_size) {
	return 0;
}

int rtpSendPacketOverUdp(int srv_rtp_sockfd, const char* ip, int16_t port, \
	struct RtpPacket* pkt, uint32_t data_size) 
{
	struct sockaddr_in addr;
	int ret;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(port);

	pkt->rtp_header.seq = htons(pkt->rtp_header.seq);
	pkt->rtp_header.timestamp = htonl(pkt->rtp_header.timestamp);
	pkt->rtp_header.ssrc = htonl(pkt->rtp_header.ssrc);

	ret = sendto(srv_rtp_sockfd, (char*)pkt, RTP_HEADER_SIZE + data_size, \
		0, (struct sockaddr*)&addr, sizeof(addr));
	if (ret < 0) {
		perror("udp send error: ");
	}

	pkt->rtp_header.seq = ntohs(pkt->rtp_header.seq);
	pkt->rtp_header.timestamp = ntohl(pkt->rtp_header.timestamp);
	pkt->rtp_header.ssrc = ntohl(pkt->rtp_header.ssrc);

	return ret;

}
