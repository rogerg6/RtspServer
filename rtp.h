
#ifndef _RTP_H
#define _RTP_H

#include <stdint.h>

#define  RTP_VERSION 2
#define RTP_HEADER_SIZE 12
#define RTP_MAX_PKT_SIZE 1400

#define  RTP_PAYLOAD_TYPE_H264 96

/*
 * ������ big endian
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
	uint8_t csrclen : 4;		// CSRC��������4bit��ָʾCSRC��ʶ���ĸ���
	uint8_t extension : 1;		// 1bit. if 1, then��rtp��ͷ���һ����չ��ͷ
	uint8_t padding : 1;		// 1bit������־��if 1��then�ڸñ���β�����һ����������İ�λ�飬���ǲ�����Ч�غɵ�һ����
	uint8_t version : 2;		// 2bit, rtpЭ��汾�ţ���ǰ�汾Ϊ2

	/* byte1 */
	uint8_t payload_type : 7;	// 7bit, ��Ч�غ����͡�����˵��rtp�غ����ͣ���GSM��Ƶ��JPEGͼƬ��
	uint8_t marker : 1;			// 1bit, ��ǡ���ͬ��Ч�غ��в�ͬ�ĺ��塣video�����һ֡������audio����ǻỰ��ʼ

	/* byte 2��3 */
	uint16_t seq;				// 16bit, ���ڱ�ʶ�����߷��͵�rtp���ĵ����кţ�ÿ��һ�����к�+1��������ͨ�����к�����ⱨ�Ķ�ʧ��������������ģ��ָ�����

	/* byte 4-7 */
	uint32_t timestamp;			// 32bit, ʱ�����Ӧ��rtp���ĵĵ�һ����λ��Ĳ���ʱ�̡�������ʹ��ʱ����������ӳٺ��ӳٶ�����������ͬ������

	/* byte 8-11 */
	uint32_t ssrc;				// 32bit, ���ڱ�ʶͬ����Դ���ñ�ʶ�������ѡ��ģ��μ�ͬһ����Ƶ���������ͬ����Դ��������ͬ��ssrc

	/* 
	* ��׼��RTP header �����ܻ���0-15����Լ��Դ(CSRC)��ʶ��
	* ÿ��CSRC��ʶ��ռ32bit��������0-15����ÿ��CSRC��ʶ�˰����ڸ�RTP������Ч�غ��е�������Լ��Դ
	**/
};

struct RtpPacket {
	struct RtpHeader rtp_header;	// rtpͷ
	uint8_t payload[0];				// rtp�غ�
};

void rtpHeaderInit(struct RtpPacket* pkt, uint8_t csrclen, uint8_t extension, \
	uint8_t padding, uint8_t version, uint8_t payload_type, uint8_t marker, \
	uint16_t seq, uint32_t timestamp, uint32_t ssrc);

int rtpSendPacketOverTcp(int connfd, struct RtpPacket* pkt, uint32_t data_size);
int rtpSendPacketOverUdp(int srv_rtp_sockfd, const char *ip, int16_t port, \
	struct RtpPacket* pkt, uint32_t data_size);



#endif