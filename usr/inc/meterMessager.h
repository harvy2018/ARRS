/***************************************************************************************
File name: meterMessager.h
Date: 2016-11-11
Author: Guoku Yan
Description:
     This file contains  all the definition and API prototype for meterMessager.c
***************************************************************************************/
#ifndef _METER_MESSAGE_H
#define _METER_MESSAGE_H

#include "taxControlInterface.h"
#include "gasolineDataStore_Retrieve.h"

#define METER_DEBUG

#define METER_MSG_PREAMBLE  "dx"

#define  METER_MSG_MAX_LEN  40
#define  GunNumPerMeter 8    //每个报税口下最多支持的油枪数
#define MeterProtocolRev  1

#define I2C_LOCAL_BUFFER_SIZE     64


typedef enum _MeterI2cComResp
{
    METER_I2C_COM_RESPONSE_OK = 0x55,
    METER_I2C_COM_RESPONSE_FAIL = 0xcc,
    METER_I2C_COM_RESPONSE_TIMEOUT = 0xbb,
    METER_I2C_COM_RESPONSE_MAX,
}MeterI2cComResp;



typedef enum _MeterAckRes
{
    METER_ACK_SUCCESS=0,
    METER_ACK_FAIL,
    METER_ACK_MAX
}MeterAckRes;

typedef enum _MeterMsgId
{
    METER_MSG_STATUS=0x2e,
    METER_MSG_TRANSACTION=0x5c,
    METER_MSG_MAX
}MeterMsgId;

typedef enum _MeterCmdId
{
    METER_CMD_STATUS=0x35,
    METER_CMD_MAX
}MeterCmdId;


typedef enum _MeterRespId
{
    METER_RESP_STATUS=0x47,
    METER_RESP_MAX    
}MeterRespId;

typedef enum _MeterAckId
{
    METER_ACK_ID=0x1b,
    METER_ACK_ID_MAX
}MeterAckId;



typedef enum _MeterMessageRcvState
{
    METER_MSG_RCV_EMPTY,
    METER_MSG_RCV_PREHEAD,
    METER_MSG_RCV_POSTHEAD,
    METER_MSG_RCV_COMPLETE,
    METER_MSG_RCV_MAX
}MeterMessageRcvState;


typedef enum _MeterModeState
{    
	METER_MODE_OFF=0,
	METER_MODE_ON,
    METER_MODE_MAX
	
}MeterModeState;

#pragma pack(1)

typedef struct _MeterHeader
{
    unsigned char preamble[2];
    unsigned char len;
    unsigned char version;
    unsigned char msg;
    unsigned char seqNo;
}MeterHeader;

typedef struct _MeterCmd
{
    MeterHeader   head;
    unsigned char port;
    unsigned char cc;
}MeterCmd;

typedef struct _MeterMsgStatus
{
    MeterHeader   head;
    unsigned char mode;
    unsigned char port;
    unsigned char gunNo;
    unsigned char status;
    unsigned char cc;
}MeterMsgStatus;

typedef struct _MeterMsgTransaction
{
    MeterHeader    head;
    unsigned char  port;
    unsigned char  gunNo;//计量枪号;    
    unsigned char  price[2];
    unsigned char  volume[4];
    unsigned char  amount[4];
   
    unsigned char   T_Type;//交易类型
    unsigned char   C_Type;//卡类 
    unsigned char   ASN[10];//加油卡号  46
    unsigned char   G_CODE[2];//油品代码 ;
    unsigned char   UNIT;//结算单位/方式 
    unsigned char   DS;//扣款来源	
    unsigned int    POS_TTC;
    unsigned int    T_MAC;
        
    unsigned char  cc;
    
}MeterMsgTransaction;



typedef struct MeterAck
{
    MeterHeader head;
    unsigned char res;
    unsigned char cc;
}MeterAck;

typedef struct _MeterResp
{
    MeterHeader   head;
    unsigned char mode;
    unsigned char port;
    unsigned char mark;
    unsigned char status;
    unsigned char cc;
}MeterResp;


typedef struct _MeterMsgRcvDes
{
    unsigned int waitCount;
    MeterMessageRcvState state;
    MeterHeader head;
    unsigned char content[METER_MSG_MAX_LEN];
    unsigned char cc;
}MeterMsgRcvDes;


typedef struct _MeterTransactionData
{   
    unsigned char DataCnt;
    unsigned char SplitFailCnt;
    
    unsigned short price[METER_MSG_MAX_LEN];
    unsigned int volume[METER_MSG_MAX_LEN];
    unsigned int amount[METER_MSG_MAX_LEN];
    unsigned int timestamp[METER_MSG_MAX_LEN];
}MeterTransactionData;


typedef struct _MeterTransactionDataGroup
{
	MeterTransactionData MeterTraDataGroup[TAX_CONTROL_DEVICE_GUNNO_MAX];                         
	
}MeterTransactionDataGroup;

typedef struct _MeterMsgTransactionMgr
{
	MeterMsgTransaction MeterMsgTRSMgr[GunNumPerMeter];
//	unsigned char HaveMessageNeedReport[GunNumPerMeter];
}MeterMsgTransactionMgr;


typedef struct _GunState_Meter
{
	MeterResp gunst;
	u8 waitcnt;
	u8 preMark;
	u8 NeedReport;
	
	
}GunState_Meter;

//extern GunStatusManager gunstatusmgr[TAX_CONTROL_DEVICE_MAX_NUM];


errorCode meterSendQueryCmd(unsigned char  port);
void meterMessagerTask();
errorCode    dbgSendGunStatusReport(unsigned char port, unsigned char gunNo, GunWorkingState status);
errorCode dbgSendGunTransactionReport(unsigned char port,unsigned char gunNo);
u8 MeterDataCmpTaxData(u8 port,u8 gunNo,u32 amount,u32 volume,u32 price,TaxOnceTransactionRecord *pRecord);
void meterQueryTask(u8 taxport);
void setMeterMode(MeterModeState state);
MeterModeState getMeterMode(void);

MeterTransactionDataGroup* getMeterDataGroup(void);
u8 getGunPickNum(u8 TaxPort);

GunStatusManager *getGunStatusManager(void);

void setMeter422Mode(u8 state);
u8 getMeter422Mode(void);
void messengerI2CSlaveHandler(void);
	
#endif



