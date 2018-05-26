/***************************************************************************************
File name: log.h
Date: 2016-10-25
Author: Guoku Yan
Description:
     This file defines log related APIs and Marco.
***************************************************************************************/
#ifndef _LOG
#define _LOG

#include "stm32f10x.h"

#define REMOTE_LOG_SIZE     2048


//LOG用到的数据结构

#pragma pack(4)

typedef struct _GunInfo
{
    u8   MapStatus;//枪映射状态
    u8   MapTO;//计量模式下此为计量枪号；枪信号模式下此为gPX值
    u32  Trycnt;
    u32  Gotcnt;
    u32  Pickcnt;
	  
}GunInfo;


typedef struct _PortInfo
{
    u8   port;
    u8	 bState;
    u8   MeterPortStatus;
    u16  MeterDataRemain;
    u8   MeterResetCnt;
    u8   TotalGun;
    
    GunInfo *guninfo;


}PortInfo;

typedef struct _DataStorageInfo
{
	u16  Unupload; 
	u32  UploadCnt;     
	u32  Wptr;
	u32  Rptr;

	
}DataStorageInfo;
/*
typedef struct _PortSampleInfo
{
    u8   port;
    u8   *GunNo;//[TotalGunNum];// TotalGunNum为此PORT下的总枪数,动态分配
    u32  *Trycnt;//[TotalGunNum];
    u32  *Gotcnt;//[TotalGunNum];
    u32  *Pickcnt;//[TotalGunNum];

}PortSampleInfo;


typedef struct _MapInfo
{
	u8  port;
	u8  TotalGun;
	u8  MappedGunNum;
	u8  *TGunTOMGun;//[TotalGunNum];//税控枪号与计量枪号的映射表；索引是税控枪号，内容是计量枪号值/枪信号值；
	
}MapInfo;

*/

typedef struct _LogContent//42
{
    u32  HWID[3];// 
    u16  Fw_Ver;   
    u8   BootType;  //
    u16  PanID;       
    u8   TxMODE;
    u8   MeterBoardMode;//
    u8   MeterProtocolVer; 
    
    u8   gPXStatus;//表示4个口状态，枪模式下
    
    u8   ExceptionFlag;//1:则LOG带有u32 EXCEPTION[8]; 0：则不带此数据

    u8   TotalHistoryReocrd_CCErr;
    u8   TransctionReocrd_CCErr;//
      
    DataStorageInfo DataInfo;//    
    
    u32  Runtime;//运行秒数        
    u16  PLC_Rstcnt;//PLC重连次数
    
   
    float SNR;
    float Temperature;//
    u32 timeStamp;//
    u16 Time_Err;
    u8  PhyConnPortCnt;
    
     
    PortInfo *portinfo;
    u32 *EXCEPTION;
    
}LogContent;




unsigned char *logGetBuffer();
void logTask();
void logTask2();
void PrintfLogo(void);

#endif
