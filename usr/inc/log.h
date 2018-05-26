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


//LOG�õ������ݽṹ

#pragma pack(4)

typedef struct _GunInfo
{
    u8   MapStatus;//ǹӳ��״̬
    u8   MapTO;//����ģʽ�´�Ϊ����ǹ�ţ�ǹ�ź�ģʽ�´�ΪgPXֵ
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
    u8   *GunNo;//[TotalGunNum];// TotalGunNumΪ��PORT�µ���ǹ��,��̬����
    u32  *Trycnt;//[TotalGunNum];
    u32  *Gotcnt;//[TotalGunNum];
    u32  *Pickcnt;//[TotalGunNum];

}PortSampleInfo;


typedef struct _MapInfo
{
	u8  port;
	u8  TotalGun;
	u8  MappedGunNum;
	u8  *TGunTOMGun;//[TotalGunNum];//˰��ǹ�������ǹ�ŵ�ӳ���������˰��ǹ�ţ������Ǽ���ǹ��ֵ/ǹ�ź�ֵ��
	
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
    
    u8   gPXStatus;//��ʾ4����״̬��ǹģʽ��
    
    u8   ExceptionFlag;//1:��LOG����u32 EXCEPTION[8]; 0���򲻴�������

    u8   TotalHistoryReocrd_CCErr;
    u8   TransctionReocrd_CCErr;//
      
    DataStorageInfo DataInfo;//    
    
    u32  Runtime;//��������        
    u16  PLC_Rstcnt;//PLC��������
    
   
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
