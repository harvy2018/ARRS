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
    unsigned short  frameLen; //BCD码   
}CPCCHead;

typedef struct _CPCCMessage
{
    CPCCHead head;                      // header
    unsigned char  content[CPCC_PROTOCOL_MAX_SIZE];       // message content
    unsigned short cc;                      // crc 16
}CPCCMessage;

typedef struct _CPCCContentCardInsert
{
    unsigned char st;//状态字：1：卡插入  2：抬枪或加油中
    unsigned char nzn;//枪号
    unsigned char len;//卡信息数据长度
    unsigned char asn[10];//卡应用号
    unsigned short cardSt;//卡状态
    unsigned int    bal;//余额
  //unsigned char *pCardInfo;//卡片信息，长度不定
}CPCCContentCardInsert;


typedef struct _CPCCContentFueling
{
    unsigned char st;//状态字：1：卡插入  2：抬枪或加油中
    unsigned char nzn;//枪号
    unsigned char punit;//结算单位或方式
    unsigned char AMN[3];
    unsigned char VOL[3];
    unsigned short PRC;
}CPCCContentFueling;

typedef struct _CPCCTransactionInfo
{
    unsigned char Handle;
    unsigned int POS_TTC;
	
	unsigned char T_TYPE;//交易类型
	unsigned char TIME[7];
	unsigned char ASN[10];//卡应用号
	unsigned int  BAL;//卡余额	 
    unsigned char AMN[3];//交易金额
	unsigned short CTC;//卡交易序号
	unsigned int  TAC;//电子签名
	unsigned int  GMAC;//解灰认证码
	unsigned int  PSAM_TAC;//灰锁签名
	unsigned char PSAM_ASN[10];//PASM应用号
	unsigned char PSAM_TID[6];//PASM卡编号
	unsigned int  PSAM_TTC;//由PSAM卡产生的终端交易序号，签名以此为准
	unsigned char DS;//扣款来源
	unsigned char UNIT;//结算单位/方式 
	unsigned char C_TYPE;//卡类 
	unsigned char VER;//版本 
	unsigned char NZN;//枪号
	unsigned char G_CODE[2];//油品代码 
	unsigned char VOL[3];//交易升数
	unsigned short PRC;//油品单价
	unsigned char EMP;//员工编号  
	unsigned int V_TOT;//升累计
	unsigned char RFU[11];//预留
	unsigned int T_MAC;//终端数据认证码
	
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
