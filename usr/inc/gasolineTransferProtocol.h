/***************************************************************************************
File name: gasolineTransferProtocol.h
Date: 2016-9-14
Author: GuoxiZhang/Wangwei/GuokuYan
Description:
     This file contain all the public interface and default definition declaration for the gasolineTransferProtocol.c. 
***************************************************************************************/
#ifndef _GASOLINE_TRANSFER_PROTOCAL_H
#define _GASOLINE_TRANSFER_PROTOCAL_H

#include "stm32f10x.h"
#include "errorCode.h"
#include "board.h"
#define GASOLINE_MSG_PREAMBLE           "PLCM"

#if Concentrator_OS==0
    #define GASOLINE_PROTOCOL_VERSION   0x0100
#else
    #define GASOLINE_PROTOCOL_VERSION   0x0201
#endif

typedef enum _GasolineAppUploadingState
{
    GASOLINE_UPLOAD_STATE_IDLE,
    GASOLINE_UPLOAD_STATE_FW_INFO,
    GASOLINE_UPLOAD_STATE_FW_INFO_WAITING_RESP,
    GASOLINE_UPLOAD_STATE_CLOCK,
    GASOLINE_UPLOAD_STATE_CLOCK_WAITING_RESP,
    GASOLINE_UPLOAD_STATE_NORMAL,
    GASOLINE_UPLOAD_STATE_TRANSACTION_WAITING_RESP,
    GASOLINE_UPLOAD_STATE_HEART_WAITING_RESP,
    GASOLINE_UPLOAD_STATE_LOG_WAITING_RESP,
	GASOLINE_UPLOAD_STATE_DELAY,
    GASOLINE_UPLOAD_MAX
}GasolineAppUploadingState;


// This enum defines the transfer hosted by this device, so, name "CmdId"
typedef enum    _GasolineTransferCmdId
{
    GASOLINE_CMD_TAXINFO = 0x0083,
    CASOLINE_CMD_CURRENT_METER_TRANSACTION_DATA = 0x0085,
    CASOLINE_CMD_CURRENT_TRANSACTION_DATA = 0x0086,
    GASOLINE_CMD_DAILY_TRANSACTION_DATA = 0x0087,
    GASOLINE_CMD_MONTHLY_TRANSACTION_DATA = 0x0088,
    GASOLINE_CMD_TOTAL_TRANSACTION_DATA = 0x0089,
    GASOLINE_CMD_ABNORMAL_TRANSACTION_DATA = 0x0060,
    GASOLINE_CMD_TEMPERATURE_DATA = 0x0061,
    GASOLINE_CMD_FIRMWARE_VERSION = 0x0062,
    GASOLINE_CMD_CLOCK_SYNC = 0x0063,
    GASOLINE_CMD_LOG_UPLOAD = 0x0064,
    GASOLINE_CMD_MAX
}GasolineTransferCmdId;

// This enum deifnes the response expected for above command, so name "RespId"
typedef enum    _GasolineTransferRespId
{
    GASOLINE_RESP_TAXINFO = 0x8083,
    CASOLINE_RESP_CURRENT_METER_TRANSACTION_DATA = 0x8085,
    GASOLINE_RESP_CURRENT_TRANSACTION_DATA = 0x8086,
    GASOLINE_RESP_DAILY_TRANSACTION_DATA = 0x8087,
    GASOLINE_RESP_MONTHLY_TRANSACTION_DATA = 0x8088,
    GASOLINE_RESP_TOTAL_TRANSACTION_DATA = 0x8089,
    GASOLINE_RESP_ABNORMAL_TRANSACTION_DATA = 0x8060,
    GASOLINE_RESP_HEARTBEAT = 0x8061,
    GASOLINE_RESP_FIRMWARE_VERSION = 0x8062,
    GASOLINE_RESP_CLOCK_SYNC = 0x8063,
    GASOLINE_RESP_LOG_UPLOAD = 0x8064, 
    GASOLINE_RESP_MAX
    
}GasolineTransferRespId;

// This enum defines the transfer hosted by the concentrator, so, name "MsgId" 
typedef enum _GasLineTransferMsgId
{
    GASOLINE_MSG_FW_UPGRADE =0x7064,
    GASOLINE_MSG_FW_TRANSFER = 0x7065,
    GASOLINE_MSG_BOARDINFO = 0x7066,
    GASOLINE_MSG_PLC_MODULATION_CONFIG = 0x7067,
    GASOLINE_MSG_PLC_MODULATION_REQUEST = 0x7068,
    GASOLINE_MSG_CONTROL = 0x7998,
    GASOLINE_MSG_MAX
} GasLineTransferMsgId;

// This enum defines the ack yet to response to the above "message",so, name "AckId"
typedef enum _GasLineTransferAckId
{
    GASOLINE_ACK_FW_UPGRADE =0x1064,
    GASOLINE_ACK_FW_TRANSFER = 0x1065,
    GASOLINE_ACK_BOARDINFO = 0x1066,
    GASOLINE_ACK_PLC_MODULATION_CONFIG = 0x1067,
    GASOLINE_ACK_PLC_MODULATION_REQUEST = 0x1068,
    GASOLINE_ACK_MAX
} GasLineTransferAckId;

typedef enum _GasoLineTransferRespRes
{
	 P_OK=0,
	 CRC_ERR,
	 MEM_ERR,
	 S_Num_ERR,
	 DataLen_ERR	 
}GasoLineTransferRespRes;

#pragma pack(4)

typedef struct _GasolineAppUploadingStateDes
{
    GasolineAppUploadingState state;
    u32 stateTimeoutCount;                  // This count used for counting timeout for response
    u32 stateRetryCount;                      //  Count how many times this state retried.
    u32 stateTotalCount;                     // Count how long this state takes totally
}GasolineAppUploadingStateDes;

typedef struct _GasolineTransferCmdHeader
{
    u8  preamble[4];        // Preamble code
    u16 len;                      // the total length (header + body + cc)
    u16 msgId;                 // Command,responce,message and Ack IDs
    u32 sequenceNo;       // Sequence number, incremented by 1 for the host side
    u16 protocolVersion; // Protocol version
    u8  frame[2];              // Frame Number
    u32 boardId[4];          // Board unique ID
}GasolineTransferCmdHeader;

typedef struct _GasolineTransferPacket
{
    GasolineTransferCmdHeader head;       // Packet header
    u8  *payLoad;                               // Pointer to the payload, variable length 
    u32 cc;                                          // crc 32 (header + payload)
}GasolineTransferPacket;

#pragma pack(1)
typedef struct _FWUpGradeInfo
{
    u8 FW_Rev[4]; //固件版本号;
    u32 Size_Ping;//固件大小
    u32 Size_Pong;//固件大小
    u32 CRC_Ping;
    u32 CRC_Pong;
    u32 mode;
}FWUpgradeInfo;

typedef struct _FWUpgradeProgressInfo
{
    u32 totalPacketNo;
    u32 receivedPacketNo;
}FWUpgradeProgressInfo;

typedef struct _FWInfo
{
    u8 MagicCode[4];
    u8 MajorVersion[2];
    u8 MinorVersion[2];
    u32 Size;
    u32 Crc;
}FWInfo;


typedef struct _ARRSInfo
{
    u8 FW_Ver[4];
    u32 HWID[4];
    u8 ARRS_WorkMode;
/*
    ARRS_WorkMode  自动抄报设备采集模式：
0： 自由税控抄报
1： 摘挂枪信号控制的税控抄报
2： 计量协议控制的税控抄报
*/
    
}ARRSInfo;



// Send application packet data
errorCode gasTransferAPPMessageSend(u16 msgId, u8 fs,  u8 fc, u8  *pData, u16 len);

errorCode gasolineAppUploadingTask();

void gasolineTrasferAppProtocolInitialize();

#endif
