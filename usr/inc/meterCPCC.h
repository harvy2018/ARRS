/***************************************************************************************
File name: meterCPCC.h
Date: 2017_3_27
Author: Guoku Yan
Description:
     This file define  all the details and APIs for CPCC protocol, see detail in meterCPCC.c
***************************************************************************************/
#ifndef _CPCC_CPCC_H_
#define  _CPCC_CPCC_H_
#include "taxControlInterface.h"
#include "meterMessager.h"

#define CPCC_PROTOCOL_MIN_PACK_SIZE   9  
#define CPCC_PROTOCOL_MAX_SIZE    256  
#define CPCC_PROTOCOL_WAITCNT    20  


#define CPCC_PROTOCOL_PREAMABLE 0xFA

#define CPCC_PROTOCOL_CMD_COMMON_QUERY   0x30
#define CPCC_PROTOCOL_CMD_REALTIME_INFO  0x31
#define CPCC_PROTOCOL_CMD_TRANSACTION    0x32

#define PORTtoCB 0


#pragma pack(1)
typedef struct _CPCCHead
{
    unsigned char   preamable;
    unsigned char   dstAddr;
    unsigned char   srcAddr;
    unsigned char   frameNo;
    unsigned short  frameLen; //BCD��   
}CPCCHead;

typedef struct _CPCCMessage
{
    CPCCHead head;                      // header
    unsigned char  content[CPCC_PROTOCOL_MAX_SIZE];       // message content
    unsigned short cc;                      // crc 16
}CPCCMessage;

typedef struct _CPCCContentCardInsert
{
    unsigned char st;//״̬�֣�1��������  2��̧ǹ�������
    unsigned char nzn;//ǹ��
    unsigned char len;//����Ϣ���ݳ���
    unsigned char asn[10];//��Ӧ�ú�
    unsigned short cardSt;//��״̬
    unsigned int    bal;//���
  //unsigned char *pCardInfo;//��Ƭ��Ϣ�����Ȳ���
}CPCCContentCardInsert;


typedef struct _CPCCContentFueling
{
    unsigned char st;//״̬�֣�1��������  2��̧ǹ�������
    unsigned char nzn;//ǹ��
    unsigned char punit;//���㵥λ��ʽ
    unsigned char AMN[3];
    unsigned char VOL[3];
    unsigned short PRC;
}CPCCContentFueling;

typedef struct _CPCCTransactionInfo
{
    unsigned char Handle;
    unsigned int POS_TTC;
	
	unsigned char T_TYPE;//��������
	unsigned char TIME[7];
	unsigned char ASN[10];//��Ӧ�ú�
	unsigned int  BAL;//�����	 
    unsigned char AMN[3];//���׽��
	unsigned short CTC;//���������
	unsigned int  TAC;//����ǩ��
	unsigned int  GMAC;//�����֤��
	unsigned int  PSAM_TAC;//����ǩ��
	unsigned char PSAM_ASN[10];//PASMӦ�ú�
	unsigned char PSAM_TID[6];//PASM�����
	unsigned int  PSAM_TTC;//��PSAM���������ն˽�����ţ�ǩ���Դ�Ϊ׼
	unsigned char DS;//�ۿ���Դ
	unsigned char UNIT;//���㵥λ/��ʽ 
	unsigned char C_TYPE;//���� 
	unsigned char VER;//�汾 
	unsigned char NZN;//ǹ��
	unsigned char G_CODE[2];//��Ʒ���� 
	unsigned char VOL[3];//��������
	unsigned short PRC;//��Ʒ����
	unsigned char EMP;//Ա�����  
	unsigned int V_TOT;//���ۼ�
	unsigned char RFU[11];//Ԥ��
	unsigned int T_MAC;//�ն�������֤��
	
}CPCCTransactionInfo;



typedef enum _CPCCMessageRcvState
{
    CPCC_MSG_RCV_EMPTY,
	CPCC_MSG_RCV_HEAD,
	CPCC_MSG_RCV_NOHEAD,
    CPCC_MSG_RCV_COMPLETE,
	CPCC_MSG_RCV_ERR,
    CPCC_MSG_RCV_MAX
}CPCCMessageRcvState;

typedef struct _CPCCMsgRcvDes
{
	unsigned int waitCount;
	unsigned short DataLen; //hex
	unsigned short HaveGotLen;
	GunWorkingState gun[GunNumPerMeter];
    CPCCMessageRcvState state;
    CPCCMessage CPCCMsg;
}CPCCMsgRcvDes;



void CPCCMessagerTask(void);
void GunStateMgrInit(void);
GunState_Meter *getGunStateMgrPtr(u8 port);


#endif
