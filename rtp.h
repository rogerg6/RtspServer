
#ifndef _RTP_H
#define _RTP_H

#include <stdint.h>

#define  RTP_VERSION 2
#define RTP_HEADER_SIZE 12
#define RTP_MAX_PKT_SIZE 1400

#define  RTP_PAYLOAD_TYPE_H264 96

/*
 * 网络序 big endian
 *   0               1               2               3
 *   7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  | V |P|   CC    |M|     PT      |    sequence number            |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                     timestamp                                 |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |     synchronization source identifier (SSRC)                  |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |     contributing source identifier (CSRC)                     |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct RtpHeader {
	/* byte 0 */
	uint8_t csrclen : 4;		// CSRC计数器，4bit，指示CSRC标识符的个数
	uint8_t extension : 1;		// 1bit. if 1, then在rtp报头后跟一个扩展报头
	uint8_t padding : 1;		// 1bit，填充标志。if 1，then在该报文尾部填充一个或多个额外的八位组，他们不是有效载荷的一部风
	uint8_t version : 2;		// 2bit, rtp协议版本号，当前版本为2

	/* byte1 */
	uint8_t payload_type : 7;	// 7bit, 有效载荷类型。用于说明rtp载荷类型，如GSM音频，JPEG图片等
	uint8_t marker : 1;			// 1bit, 标记。不同有效载荷有不同的含义。video：标记一帧结束；audio：标记会话开始

	/* byte 2，3 */
	uint16_t seq;				// 16bit, 用于标识发送者发送的rtp报文的序列号，每发一个序列号+1。接收者通过序列号来检测报文丢失情况，重新排序报文，恢复数据

	/* byte 4-7 */
	uint32_t timestamp;			// 32bit, 时间戳反应该rtp报文的第一个八位组的采样时刻。接收者使用时间戳来计算延迟和延迟抖动，并进行同步控制

	/* byte 8-11 */
	uint32_t ssrc;				// 32bit, 用于标识同步信源。该标识符是随机选择的，参加同一个视频会议的两个同步信源不能有相同的ssrc

	/* 
	* 标准的RTP header 还可能会有0-15个特约信源(CSRC)标识符
	* 每个CSRC标识符占32bit，可以有0-15个。每个CSRC标识了包含在该RTP报文有效载荷中的所有特约信源
	**/
};

struct RtpPacket {
	struct RtpHeader rtp_header;	// rtp头
	uint8_t payload[0];				// rtp载荷
};

void rtpHeaderInit(struct RtpPacket* pkt, uint8_t csrclen, uint8_t extension, \
	uint8_t padding, uint8_t version, uint8_t payload_type, uint8_t marker, \
	uint16_t seq, uint32_t timestamp, uint32_t ssrc);

int rtpSendPacketOverTcp(int connfd, struct RtpPacket* pkt, uint32_t data_size);
int rtpSendPacketOverUdp(int srv_rtp_sockfd, const char *ip, int16_t port, \
	struct RtpPacket* pkt, uint32_t data_size);



#endif