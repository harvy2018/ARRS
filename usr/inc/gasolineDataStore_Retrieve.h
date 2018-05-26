/***************************************************************************************
File name: gasolineDataStore_Retrieve.h
Date: 2016-9-23
Author: GuoxiZhang/Wangwei/GuokuYan
Description:
     This file contain all the public interface and default definition declaration for the gasolineDataStorge_Retrieve.c.
***************************************************************************************/
#ifndef _GASOLINEDATASTORE_RETRIEVE
#define _GASOLINEDATASTORE_RETRIEVE
#include "flashMgr.h"
#include "board.h"

#define FRESH   0xFF
#define DIRTY   0xF0
#define DONE    0x00

#define   TAX_CONTROL_DEVICE_HISTORY_INDEX_SPACE_SIZE	0x40	// 64 bytes quick index space(4Bytes Magic + 60 record mark)
#define	TAX_CONTROL_DEVICE_HISTORY_GUN_SECTOR_NUM	1		// one sector for each gun
#define   TAX_CONTROL_DEVICE_HISTORY_INDEX_SPACE_MAGIC	"TAXH"	// magic word

#define TAX_CONTROL_HISTORY_RECORD_INVALID    0x00
#define TAX_CONTROL_HISTORY_RECORD_VALID      0x01  

#define TAX_CONTROL_DEVICE_MSDSAMPLE_FLAG_FLASH_MAGIC  "MSDH"

typedef enum _TraDataType
{
    
    MeterData=0,
    TaxData
    
}TraDataType;


#pragma pack(1)
typedef struct _OnceStorageRecordHeader
{
    u8  mark;  
    u8  data_type;//0：计量交易 1：税控交易
    
}OnceStorageRecordHeader;


typedef struct _TaxDevInfoHeader
{
    u8  mark;
    u8  type;
    u8  reserved1;
    u8  reserved2;    
}TaxDevInfoHeader;

typedef struct _TaxDevInfoContent
{
    unsigned char   serialNum[10];
    unsigned char   gunNum[2];
    unsigned char   taxerRegNum[20];
    unsigned char   oilGrade[4];
    unsigned char   year[4];
    unsigned char   month[2];
    unsigned char   day[2];
    unsigned char   hour[2];
    unsigned char   minute[2];
    unsigned short  PortNum;  /// gun number + serial port number
    unsigned int    timeStamp;
}TaxDevInfoContent;

// Notice: Pad this structure to 64 Bytes sized
typedef struct _TaxDevInfo
{
    TaxDevInfoHeader head;
    TaxDevInfoContent content;
    unsigned char   reserved[5];
    unsigned char cc;
}TaxDevInfo;

typedef struct _TaxMonthDatadHeader
{
    u8  mark;
    u8  type;
    u8  reserved1;
    u8  reserved2;
}TaxMonthDatadHeader;

#if Concentrator_OS == 0

typedef struct _TaxOnceStorageRecordContent
{
    u8  date[2];        // sampling date
    u8  hour[2];        // sampling hour
    u8  minute[2];    // sampling minute
    u8  volume[6];   //  The transaction volume
    u8  amount[6];  //  The transaction money
    u8  price[4];       //  the gasoline price
    u8  Totalvolume[12];
    u8  Totalamount[12];
    u16 gunNo;        //   gun No of the deivce  
    u32 timeStamp;  // Timestamp
    u8  factorySerialNo[10];   // The unique # of the device
}TaxOnceStorageRecordContent;

#else

typedef struct _TaxOnceStorageRecordContent//61
{

    u32  timeStamp;  // Timestamp
    u32  volume;   //  The transaction volume
    u32  amount;  //  The transaction money
    u64  Totalvolume;
    u64  Totalamount;
    u16  price;       //  the gasoline price
    u8   gunNo;        //   gun No of the deivce  
    u8   port;

    u8   factorySerialNo[10];   // The unique # of the device
    u8   reserve[19];
    
}TaxOnceStorageRecordContent;


#endif

typedef struct _MeterOnceStorageRecordContent//60+1
{
    u32  timeStamp;  // Timestamp
    u32  volume;   //  The transaction volume
    u32  amount;  //  The transaction money
    u16  PRC;       //  the gasoline price  
    u8   port;
    u8   NZN;//计量枪号;        //   gun No of the deivce   
    u8   factorySerialNo[10];   // The unique # of the device
    u8   T_Type;//交易类型
    u8   C_Type;//卡类 
    u8   ASN[20];//加油卡号  46
    u8   G_CODE[2];//油品代码 ;
    u8   UNIT;//结算单位/方式 
    u8   DS;//扣款来源	
    u32  POS_TTC;
    u32  T_MAC;
    u8   reserve;
    
}MeterOnceStorageRecordContent;

typedef struct _MeterTraSendContent//
{
    u32  MeterType;//石油石化协议区分
    u8   MeterVer[4];//计量协议版本
    u32  POS_TTC;
    u32  T_MAC;
    u32  timeStamp;  // Timestamp
    u32  volume;   //  The transaction volume
    u32  amount;  //  The transaction money
    u16  PRC;       //  the gasoline price  
    u8   port;
    u8   NZN;//计量枪号;        //   gun No of the deivce 
    u8   C_Type;//卡类     
    u8   factorySerialNo[10];   // The unique # of the device     
    u8   ASN[20];//加油卡号  46
 
}MeterTraSendContent;



typedef struct _TaxTraSendContent//
{  
    u32  volume;   //  The transaction volume
    u32  amount;  //  The transaction money
    u64  TotalV;
    u64  TotalA;
    u16  PRC;       //  the gasoline price  
    u8   gunno;//税控枪号;        //   gun No of the deivce    
    u8   port;  
  
    u32  timeStamp;  // Timestamp
    u8   factorySerialNo[10];   // The unique # of the device      
    
}TaxTraSendContent;



typedef struct _TaxMonthStorageRecordContent
{
    u8 year[4];
    u8 month[2];
    u8 volume[10];
    u8 amount[10];
    u16 gunNo;        //   gun No of the deivce
    u32 timeStamp;  // Timestamp
    u8  factorySerialNo[10];   // The unique # of the device
}TaxMonthStorageRecordContent;


typedef struct _TaxOnceTransactionRecord
{
   OnceStorageRecordHeader  head;
   TaxOnceStorageRecordContent content;
   u8   cc;
}TaxOnceTransactionRecord;

typedef struct _MeterOnceTransactionRecord
{
   OnceStorageRecordHeader  head;
   MeterOnceStorageRecordContent content;
   u8   cc;
}MeterOnceTransactionRecord;


//typedef struct _TaxMonthDataRecord
//{
//   TaxMonthDatadHeader  head;
//   TaxMonthStorageRecordContent content;
//   u8   reserved[17];               // make this structure 64bytes alignment for simple coding
//   u8   cc;
//}TaxMonthDataRecord;


#pragma pack(4)
typedef struct _TaxOnceTransactionSpaceManager
{
    u32 intialized;
    FLASH_TYPE type;
    u32 startAddr;
    u32 endAddr;
    u32 StoreAddr;
    u32 RetrieveAddr;
}TaxOnceTransactionSpaceManager;

#pragma pack()
// Define tax control device history record related structure
typedef struct _TaxHistoryTotalRecord	// 存储在flash中
{
    unsigned char cc;                   // The XOR of the remaining 31bytes
    unsigned char gunNo;				        //  Gun Number
    unsigned char portNo;				        //  Port Number
    unsigned char initLevel;
    unsigned int  timeStamp;
    long long     totalVolume;
    long long	  totalAmount;
    unsigned char serialID[10];		      // device unique ID
    unsigned char taxUniqueID[20];         // tax unique ID

    unsigned char reserved2[10];           //  Reserved for future usage
    
}__attribute__((aligned(8))) TaxHistoryTotalRecord;

typedef struct _TaxHistoryTotalRecordInfo
{
	unsigned char valid;					// Record valid or not
	unsigned char index;					// The position of "Valid flag" in index space, and also help to position the address of the actual record.
	TaxHistoryTotalRecord record;			// The record structure definition
}TaxHistoryTotalRecordInfo;

errorCode storeTotalHistoryRecord(unsigned char port,unsigned char gun,unsigned char gunBaseNo,
										   long long volume, long long amount,
										   unsigned char *pSerialId,unsigned char *pTaxId);  
                                           
errorCode   storeRetrieveIndexIntialize(TraDataType DataType);
errorCode   storeTransactionRecord(void *pRecord,TraDataType DataType);
errorCode   retrieveTransactionRecord(void *pRecord,TraDataType DataType);                                                                                    
errorCode   retrieveTransactionRecordReadPointer(TraDataType DataType);

void MakeTraDataToSend(void *Record,void *Send,TraDataType DataType);

errorCode   retrieveTotalHistoryRecord();                                           
TaxHistoryTotalRecordInfo* getTaxHistoryTotalRecordInfo();
errorCode   storeRetrieveTaxInfoIndexIntialize();
errorCode   storeTaxDevInfoRecord(TaxDevInfo *pRecord);
errorCode  retrieveTaxDevInfoRecord(TaxDevInfo *pRecord);
errorCode   retrieveTaxDevInfoRecordReadPointer();      
errorCode StoreTaxDevInitializationInfo(unsigned char devNo, unsigned char *buf, unsigned short size);
errorCode RetrieveTaxDevInitializationInfo(unsigned char devNo, unsigned char *buf, unsigned short size);
										   
#endif
                                           

