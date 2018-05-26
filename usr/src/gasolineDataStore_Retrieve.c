/***************************************************************************************
File name: gasolineDataStore_Retrieve.c
Date: 2016-9-23
Author: Wangwei/GuokuYan
Description:
     This file contain all the interface to store data/info to flash and retrieve them from flash, this is helper components. 
***************************************************************************************/
#include <string.h>
#include "errorCode.h"
#include "gasolineDataStore_Retrieve.h"
#include "flashMgr.h"
#include "taxControlInterface.h"
#include "time.h"
#include "localTime.h"
#include "board.h"

#define dbgPrintf   SEGGER_RTT_printf

static TaxHistoryTotalRecordInfo taxHistoryTotalRecordInfo[TAX_CONTROL_MACHINE_GUNNO_MAX];
u8 Check_Data(u8 *buf,u16 dlen);
u8 TotalHistoryReocrd_CCErr=0;
u8 TransctionReocrd_CCErr=0;
u8 TaxMonthDataReocrd_CCErr=0;
//u32 GasDataOriginWptr=0; //每次上电后检索到的加油数据写指针位置
/**********************************************************************************************
Function Name: retrieveTotalHistoryRecord
Description:
    Initialize hisory "total" flash section for each gun.
Parameter:
    NULL
Return:
    NULL
**********************************************************************************************/
errorCode retrieveTotalHistoryRecord()
{
    int i,j;
    unsigned char magic[4];
    unsigned char pBuf[TAX_CONTROL_DEVICE_HISTORY_INDEX_SPACE_SIZE];
    unsigned int addr;
    FlashSectionStruct *pFS;
	
    TaxHistoryTotalRecordInfo *pTaxHistoryTotalRecordInfo = taxHistoryTotalRecordInfo;
    unsigned char LastValidRecord=0;
    pFS = uf_RetrieveFlashSectionInfo(FLASH_SECTION_TAXCONTROL_HISTORY);
   
    addr = pFS->base;
    
    // if the flash sector for each gun(as maxium as 16 guns) not initialized, initialize them here.
    for(i=0;i<TAX_CONTROL_MACHINE_GUNNO_MAX;i++)
        {
            // first of all, read the magic # at the beginning of each sector.
            uf_ReadBuffer(pFS->type, magic,addr,4);
            if(memcmp(magic,TAX_CONTROL_DEVICE_HISTORY_INDEX_SPACE_MAGIC,4))
                {
                    // magic number is not correct, need initialize it
                    uf_EraseSector(pFS->type, addr);    // erase the whole sector
                    uf_WriteBuffer(pFS->type,TAX_CONTROL_DEVICE_HISTORY_INDEX_SPACE_MAGIC,addr,4);  // write the magic #
                    pTaxHistoryTotalRecordInfo[i].index = 3;    // valid index:[4,63],0 means empty // initialize the index to invalid
                    pTaxHistoryTotalRecordInfo[i].valid = TAX_CONTROL_HISTORY_RECORD_INVALID;   // invalid flag
                    printf("totalHistoryRecordRetrieve: Gun %d total space not initialized, erase and initialize\r\n",i);
                }
            else    // Already initialized, need serach the the lastest index
                {
                    uf_ReadBuffer(pFS->type, pBuf, addr,TAX_CONTROL_DEVICE_HISTORY_INDEX_SPACE_SIZE);
                    for(j=4;j<TAX_CONTROL_DEVICE_HISTORY_INDEX_SPACE_SIZE;j++)
                        {
                            if(pBuf[j] == FRESH)
                                {
                                    if(j==4)    // initialized, but empty, so still invalid
                                        {
                                            pTaxHistoryTotalRecordInfo[i].index= 3;
                                            pTaxHistoryTotalRecordInfo[i].valid = TAX_CONTROL_HISTORY_RECORD_INVALID;
                                            printf("totalHistoryRecordRetrieve: Gun %d total space initialized but empty\r\n",i);
                                        }
                                    else
                                        {         // initialized, not empty
                                            if(pBuf[j-1] == DONE)
                                                {
                                                    TaxHistoryTotalRecord *pTaxHistoryTotalRecord;
                                                    unsigned char tt[11]={0};
                                                    u8 cc;
                                                    pTaxHistoryTotalRecordInfo[i].index = j-1;     // this is the latest record index
                                                    // Read the record out of flash
                                                    uf_ReadBuffer(pFS->type, pBuf,addr+pTaxHistoryTotalRecordInfo[i].index*sizeof(TaxHistoryTotalRecord) ,sizeof(TaxHistoryTotalRecord));
                                                    pTaxHistoryTotalRecord = (TaxHistoryTotalRecord *)pBuf;

                                                    cc=Check_Data(pBuf+1,sizeof(TaxHistoryTotalRecord)-1);
                                                    if(cc==pTaxHistoryTotalRecord->cc)
                                                        {
                                                            pTaxHistoryTotalRecordInfo[i].record.gunNo = pTaxHistoryTotalRecord->gunNo;
                                                            pTaxHistoryTotalRecordInfo[i].record.portNo = pTaxHistoryTotalRecord->portNo;
                                                            pTaxHistoryTotalRecordInfo[i].record.initLevel = pTaxHistoryTotalRecord->initLevel;
                                                            pTaxHistoryTotalRecordInfo[i].record.totalAmount = pTaxHistoryTotalRecord->totalAmount;
                                                            pTaxHistoryTotalRecordInfo[i].record.totalVolume = pTaxHistoryTotalRecord->totalVolume;
                                                            memcpy(pTaxHistoryTotalRecordInfo[i].record.serialID,pTaxHistoryTotalRecord->serialID,10);
                                                            memcpy(pTaxHistoryTotalRecordInfo[i].record.taxUniqueID,pTaxHistoryTotalRecord->taxUniqueID,20);
                                                            pTaxHistoryTotalRecordInfo[i].valid = TAX_CONTROL_HISTORY_RECORD_VALID;
                                                            memcpy(tt,pTaxHistoryTotalRecordInfo[i].record.serialID,10);
                                                            LastValidRecord=1;//找到最后一条合法的历史总累记录
                                                            printf("totalHistoryRecordRetrieve:Gun %d total space found valid index at %d\r\n",i,j-1);
                                                            printf("totalHistoryRecordRetrieve:SerialID:%s\r\n",tt);
                                                            printf("totalHistoryRecordRetrieve: TotalVolume:%lld\r\n",pTaxHistoryTotalRecord->totalVolume);
                                                            printf("totalHistoryRecordRetrieve: TotalAmount:%lld\r\n",pTaxHistoryTotalRecord->totalAmount);                                                            
                                                        }
                                                    else
                                                        {
                                                            TotalHistoryReocrd_CCErr++;
                                                            pTaxHistoryTotalRecordInfo[i].valid = TAX_CONTROL_HISTORY_RECORD_INVALID;
                                                            printf("totalHistoryRecordRetrieve:Gun %d total space found invalid index, record cc err! cc_old:%02X,cc_new:%02X\r\n",i,pTaxHistoryTotalRecordInfo[i].record.cc,cc);
                                                        }
                                                }
                                            else
                                                {
                                                    pTaxHistoryTotalRecordInfo[i].index = j;
                                                    pTaxHistoryTotalRecordInfo[i].valid = TAX_CONTROL_HISTORY_RECORD_INVALID;
                                                    printf("totalHistoryRecordRetrieve:Gun %d total space found invalid index, drop and recover\r\n",i);
                                                }
                                        }
                                    break;
                                }
                        }
                        
                        if(j == TAX_CONTROL_DEVICE_HISTORY_INDEX_SPACE_SIZE-1)
                            {
                                uf_EraseSector(pFS->type, addr);
                                uf_WriteBuffer(pFS->type,TAX_CONTROL_DEVICE_HISTORY_INDEX_SPACE_MAGIC,addr,4);
                                if(LastValidRecord==0)
                                    {
                                        pTaxHistoryTotalRecordInfo[i].index = 3;    // valid index:[4,63],0 means empty
                                        pTaxHistoryTotalRecordInfo[i].valid = TAX_CONTROL_HISTORY_RECORD_INVALID;
                                        printf("totalHistoryRecordRetrieve:Gun %d total space full, reinitialize it\r\n",i);
                                    }
                                else
                                    {
                                        uf_WriteBuffer(pFS->type,pBuf,addr+4*sizeof(TaxHistoryTotalRecord),sizeof(TaxHistoryTotalRecord));
                                        pTaxHistoryTotalRecordInfo[i].index = 4;    // valid index:[4,63],0 means empty
                                        pTaxHistoryTotalRecordInfo[i].valid = TAX_CONTROL_HISTORY_RECORD_VALID;
                                        printf("totalHistoryRecordRetrieve:Gun %d total space full, retrieve lastvalidrecord！\r\n",i);
                                    }
                            }
			    }
            bsp_DelayMS(10);
            addr += FLASH_SECTOR_SIZE;
        }
    return E_SUCCESS;    
}
/***********************************************************************************************
Function Name: storeTotalHistoryRecord
Description:
    Store an Total sample record into flash as per gun(total 8 guns supported)
Parameters:
    port: [I], current uart port #
    gun: [I], current gun # of tax control device
    gunBaseBo:[I], Current tax control device start gun No offset.
    volume:[I], current total volume
    amount:[I], current total amount
    pSerialID:[I], current tax control unique ID
    pTaxID:[I], current gasoline station unique ID
Return:
    E_SUCCESS: always return successfully.
************************************************************************************************/
errorCode storeTotalHistoryRecord(unsigned char port,unsigned char gun,unsigned char gunBaseNo,
										   long long volume, long long amount,
										   unsigned char *pSerialId,unsigned char *pTaxId)
{
    unsigned int index;
    unsigned int gunSpaceStartAddr;
    unsigned int addr;
    FlashSectionStruct *pFS;
    unsigned char mark,cc;
    TaxHistoryTotalRecordInfo *pTaxHistoryTotalRecordInfo = taxHistoryTotalRecordInfo;

    pFS = uf_RetrieveFlashSectionInfo(FLASH_SECTION_TAXCONTROL_HISTORY);

    gunSpaceStartAddr = pFS->base + (gun+gunBaseNo) * FLASH_SECTOR_SIZE;
    pTaxHistoryTotalRecordInfo[gun+gunBaseNo].index++;
    if(pTaxHistoryTotalRecordInfo[gun+gunBaseNo].index>= (TAX_CONTROL_DEVICE_HISTORY_INDEX_SPACE_SIZE - 4 - 1))//保留一个空位
    {
		uf_EraseSector(pFS->type, gunSpaceStartAddr);
		uf_WriteBuffer(pFS->type,TAX_CONTROL_DEVICE_HISTORY_INDEX_SPACE_MAGIC,gunSpaceStartAddr,4);
		pTaxHistoryTotalRecordInfo[gun+gunBaseNo].index=4;  
    }

    addr = gunSpaceStartAddr +  pTaxHistoryTotalRecordInfo[gun+gunBaseNo].index * sizeof(TaxHistoryTotalRecord);
    
    // Assign value to structure

    pTaxHistoryTotalRecordInfo[gun+gunBaseNo].record.gunNo = gun;
    pTaxHistoryTotalRecordInfo[gun+gunBaseNo].record.portNo = port;
    pTaxHistoryTotalRecordInfo[gun+gunBaseNo].record.initLevel = 0;
    memcpy(pTaxHistoryTotalRecordInfo[gun+gunBaseNo].record.serialID,pSerialId,10);
    memcpy(pTaxHistoryTotalRecordInfo[gun+gunBaseNo].record.taxUniqueID,pTaxId,20);
    pTaxHistoryTotalRecordInfo[gun+gunBaseNo].record.totalAmount = amount;
    pTaxHistoryTotalRecordInfo[gun+gunBaseNo].record.totalVolume = volume;

	cc=Check_Data(((u8 *)&(pTaxHistoryTotalRecordInfo[gun+gunBaseNo].record))+1,sizeof(TaxHistoryTotalRecord)-1);
	
	pTaxHistoryTotalRecordInfo[gun+gunBaseNo].record.cc=cc;

    // 1st of all, write dirty
    mark = DIRTY;
    uf_WriteBuffer(pFS->type,&mark, gunSpaceStartAddr+pTaxHistoryTotalRecordInfo[gun+gunBaseNo].index,1);

    // 2nd, write the content
    uf_WriteBuffer(pFS->type, (u8 *)&pTaxHistoryTotalRecordInfo[gun+gunBaseNo].record, addr, sizeof(TaxHistoryTotalRecord));

    // 3rd, write done
    mark = DONE;
    uf_WriteBuffer(pFS->type, &mark, gunSpaceStartAddr+pTaxHistoryTotalRecordInfo[gun+gunBaseNo].index, 1);


    printf("storeTotalHistoryRecord: gun:%d  port:%d gunOrder:%d @%x\r\n",gun,port,gun+gunBaseNo,addr);
    printf("storeTotalHistoryRecord: totalVol:%lld\r\n",volume);
    printf("storeTotalHistoryRecord: totalAmount:%lld\r\n",amount);
    
    return E_SUCCESS;
}
/****************************************************************************************************
Function Name: storeRetrieveIndexIntialize
Description:
	This function intends to initialize the flash space for store/retrieve the tax transaction record. After this call, the next wptr for 
stroing a new reocrd and the next rptr for retrieveing a record to upload are positioned.
Parameter:
	NULL
Return:
	NULL
*****************************************************************************************************/
errorCode   storeRetrieveIndexIntialize(TraDataType DataType)
{
    u32 i,k;
    u32 spaceSize,spaceStartAddr,spaceEndAddr,spaceSectorNum;
    FLASH_TYPE type;
    u8  c[2];
    u32 addr;

    FlashSectionStruct *pFS ;
    
    if(DataType==TaxData) 
    {
	   pFS= uf_RetrieveFlashSectionInfo(FLASH_SECTION_GASOLINE_DATA);
       
        
        
    }
    else if(DataType==MeterData) 
    {
       pFS= uf_RetrieveFlashSectionInfo(FLASH_SECTION_METER_DATA);

        
    }
    else
    {
        printf("storeRetrieveIndexIntialize: DataType Err!\r\n");
        return E_INPUT_PARA;
    }
    
    
   
    spaceStartAddr = pFS->base;
    spaceSize = pFS->size;
    spaceEndAddr = spaceStartAddr + spaceSize;
    spaceSectorNum = pFS->size >> 12;
    type = pFS->type;


    // 1st initialized the wptr and  rptr to the start address of the space
    pFS->wptr = spaceStartAddr;

    // Then look for next address to store the new record
    addr= spaceStartAddr;
    while(addr < spaceEndAddr)
	{
		uf_ReadBuffer(type, c, addr , 2);
		if((c[0] == FRESH) && (c[1] == FRESH))      // we must lookfor where mark=fresh and type = fresh to write next new record
		{
			pFS->wptr = addr;
			break;
		}
		addr= addr + sizeof(TaxOnceTransactionRecord);
	}

    // Till the end of the space, still not found the proper address, may need erase the 1st sector
    if(addr == spaceEndAddr)
	{
		// readback the last record of the 1st sector, if the mark == done indicates the sector is useless and can be erased
		uf_ReadBuffer(type,c, spaceStartAddr + FLASH_SECTOR_SIZE - sizeof(TaxOnceTransactionRecord),1);
		if(c[0] == DONE)
			{                    
				uf_EraseSector(type, spaceStartAddr);
				pFS->wptr = pFS->base;
				printf("storeRetrieveIndexIntialize: Erase the 1st sector: 1\r\n");
			}
		else
			{
				printf("!!!!storeRetrieveIndexIntialize: Fatal error 1\r\n");// TODO: fata error since no empty space
			}
	}
        
    // 1st initilize rptr to wptr
    pFS->rptr = pFS->wptr;

    // then look for where mark = dirty
    addr= spaceStartAddr;
    while(addr < spaceEndAddr - sizeof(TaxOnceTransactionRecord))
    {
        uf_ReadBuffer(type, c, addr, 1);
        if(c[0] == DIRTY)
        {
            pFS->rptr = addr;
            break;
        }
        addr = addr + sizeof(TaxOnceTransactionRecord);
    }
        
  //  GasDataOriginWptr=pFS->wptr;
        
    printf("storeRetrieveIndexIntialize: wptr=%x  rptr=%x\r\n",pFS->wptr,pFS->rptr);

    return E_SUCCESS;    
}
/****************************************************************************************************
Function Name: storeTransactionRecord
Description:
	This function intends to stor one gasoline transaction record onto flash.
Parameter:
	pRecord:[I/O],pointer of TaxOnceTransactionRecord structure.
Return:
	E_SUCCESS: Succeed to store one record.
	E_NO_MEMORY: flash buffer is full.
*****************************************************************************************************/

errorCode   storeTransactionRecord(void *pRecord,TraDataType DataType)
{
    
    u8  mark,cc;
    FlashSectionStruct *pFS;
    
    if(DataType==TaxData) 
    {
	   pFS= uf_RetrieveFlashSectionInfo(FLASH_SECTION_GASOLINE_DATA);
       
       cc=Check_Data((u8*)pRecord,sizeof(TaxOnceTransactionRecord)-1);
	   ((TaxOnceTransactionRecord *)pRecord)->cc=cc;
        
        printf("storeTaxTraRecord: wptr:%x  rptr:%x\r\n",pFS->wptr,pFS->rptr);
          
        
    }
    else if(DataType==MeterData) 
    {
       pFS= uf_RetrieveFlashSectionInfo(FLASH_SECTION_METER_DATA);
        cc=Check_Data((u8*)pRecord,sizeof(MeterOnceTransactionRecord)-1);
	   ((MeterOnceTransactionRecord *)pRecord)->cc=cc;
        
        printf("storeMeterTraRecord: wptr:%x  rptr:%x\r\n",pFS->wptr,pFS->rptr);
        
    }
    else
    {
        printf("storeTransactionRecord: DataType Err!\r\n");
        return E_INPUT_PARA;
    }
     
		
    // 1st write the content,,TaxOnceTransactionRecord和MeterOnceTransactionRecord size相同故用其一计算size
    uf_WriteBuffer(pFS->type, (u8 *)pRecord + 1, pFS->wptr+1, sizeof(TaxOnceTransactionRecord)-1);
    // The write the "mark"
    uf_WriteBuffer(pFS->type, (u8 *)pRecord, pFS->wptr, 1);


    pFS->wptr += sizeof(TaxOnceTransactionRecord);

    if(pFS->wptr >= (pFS->base+pFS->size))
        pFS->wptr = pFS->base;


    // Judge if wptr is at the beginning of a sector, if yes, need judge whether erase the sector or not
    if(pFS->wptr % FLASH_SECTOR_SIZE == 0 )
    {
        uf_ReadBuffer(pFS->type, &mark, pFS->wptr, 1);
        if(mark != FRESH)
        {
            u8   lastMark;
            uf_ReadBuffer(pFS->type, &lastMark,pFS->wptr+FLASH_SECTOR_SIZE - sizeof(TaxOnceTransactionRecord),1);
            if(lastMark == DONE)
            {
                uf_EraseSector(pFS->type, pFS->wptr);
                printf("storeTransactionRecord:Erase the next sector: %x\r\n",pFS->wptr);
            }
            else
            {
                printf("storeTransactionRecord:!!!! No free space\r\n");
                return E_NO_MEMORY;
            }    
        }
    }
    return E_SUCCESS;
}
/****************************************************************************************************
Function Name: retrieveTransactionRecord
Description:
	This function intends to readback one gasoline transaction record from flash.
Parameter:
	pRecord:[I/O],pointer of TaxOnceTransactionRecord structure.
Return:
	E_SUCCESS: Succeed to retrieve one record. 
	E_EMPTY: no vaild record.
*****************************************************************************************************/
errorCode  retrieveTransactionRecord(void *pRecord,TraDataType DataType)
{
    FlashSectionStruct *pFS;
    u8 mark,cc;
	
    if(DataType==TaxData) 
    {
	   pFS= uf_RetrieveFlashSectionInfo(FLASH_SECTION_GASOLINE_DATA);
       
    }
    else if(DataType==MeterData) 
    {
       pFS= uf_RetrieveFlashSectionInfo(FLASH_SECTION_METER_DATA);   
        
    }
    else
    {
        printf("retrieveTransactionRecord: DataType Err!\r\n");
        return E_INPUT_PARA;
    }
          
    while(pFS->rptr != pFS->wptr)
	{
		uf_ReadBuffer(pFS->type, (u8 *)pRecord, pFS->rptr, sizeof(TaxOnceTransactionRecord));
		if(((TaxOnceTransactionRecord*)pRecord)->head.mark == DIRTY) // Normally, this is the case
		{
			 cc=Check_Data((u8*)pRecord,sizeof(TaxOnceTransactionRecord)-1);
			 if(((TaxOnceTransactionRecord*)pRecord)->cc==cc)
			 {
				 printf("retrieveTransactionRecord: Successfully retrieve a transaction record @ %x\r\n",pFS->rptr);
				 return E_SUCCESS;
			 }	 
			 else
			 {
				 TransctionReocrd_CCErr++;
				 printf("retrieveTransactionRecord: CC ERR,retrieve a transaction record @ %x failed!\r\n",pFS->rptr);
				 pFS->rptr += sizeof(TaxOnceTransactionRecord);
			     if(pFS->rptr >= pFS->base + pFS->size)
				    pFS->rptr = pFS->base;
				 
				 return E_CHECK_CODE;
			 }
									
		}   
		else        // This is the abnormal case means "mark" was not programmed then somehing abnormal occured
		{
			pFS->rptr += sizeof(TaxOnceTransactionRecord);
			if(pFS->rptr >= pFS->base + pFS->size)
				pFS->rptr = pFS->base;
		}
	}   

    return E_EMPTY;

}

void MakeTraDataToSend(void *Record,void *Send,TraDataType DataType)
{
    
    if(DataType==TaxData) 
    {	          
        ((TaxTraSendContent *)Send)->timeStamp= ((TaxOnceTransactionRecord*)Record)->content.timeStamp;
        ((TaxTraSendContent *)Send)->volume= ((TaxOnceTransactionRecord*)Record)->content.volume;
        ((TaxTraSendContent *)Send)->amount= ((TaxOnceTransactionRecord*)Record)->content.amount;
        ((TaxTraSendContent *)Send)->PRC= ((TaxOnceTransactionRecord*)Record)->content.price;
        
        ((TaxTraSendContent *)Send)->port= ((TaxOnceTransactionRecord*)Record)->content.port;
        ((TaxTraSendContent *)Send)->gunno= ((TaxOnceTransactionRecord*)Record)->content.gunNo;
        
        ((TaxTraSendContent *)Send)->TotalA= ((TaxOnceTransactionRecord*)Record)->content.Totalamount;
        ((TaxTraSendContent *)Send)->TotalV= ((TaxOnceTransactionRecord*)Record)->content.Totalvolume;
        
         memcpy(((TaxTraSendContent *)Send)->factorySerialNo,((TaxOnceTransactionRecord*)Record)->content.factorySerialNo,10);
       
    }
    else if(DataType==MeterData) 
    {
        
        ((MeterTraSendContent *)Send)->MeterType=1;
         memset(((MeterTraSendContent *)Send)->MeterVer,0,4);    
        ((MeterTraSendContent *)Send)->POS_TTC= ((MeterOnceTransactionRecord*)Record)->content.POS_TTC;
        ((MeterTraSendContent *)Send)->T_MAC= ((MeterOnceTransactionRecord*)Record)->content.T_MAC;
        ((MeterTraSendContent *)Send)->timeStamp= ((MeterOnceTransactionRecord*)Record)->content.timeStamp;
        
        ((MeterTraSendContent *)Send)->volume= ((MeterOnceTransactionRecord*)Record)->content.volume;
        ((MeterTraSendContent *)Send)->amount= ((MeterOnceTransactionRecord*)Record)->content.amount;
        ((MeterTraSendContent *)Send)->PRC= ((MeterOnceTransactionRecord*)Record)->content.PRC;
        
        ((MeterTraSendContent *)Send)->port= ((MeterOnceTransactionRecord*)Record)->content.port;
        ((MeterTraSendContent *)Send)->NZN= ((MeterOnceTransactionRecord*)Record)->content.NZN;
        
        ((MeterTraSendContent *)Send)->C_Type= ((MeterOnceTransactionRecord*)Record)->content.C_Type;
   
         memcpy(((MeterTraSendContent *)Send)->factorySerialNo,((MeterOnceTransactionRecord*)Record)->content.factorySerialNo,10);
         memcpy(((MeterTraSendContent *)Send)->ASN,((MeterOnceTransactionRecord*)Record)->content.ASN,20);
       
        
    }
    else
    {
        printf("MakeTraDataToSend: DataType Err!\r\n");
        return;
    }
}
/****************************************************************************************************
Function Name: retrieveTransactionRecordReadPointer
Description:
	This function intends to mark the current single_record which poined by rptr as "DONE".
Parameter:
	NULL.
Return:
	E_SUCCESS: Succeed to retrieve one record.
*****************************************************************************************************/
errorCode   retrieveTransactionRecordReadPointer(TraDataType DataType)
{
    u8 mark = DONE;
    FlashSectionStruct *pFS;
    
   if(DataType==TaxData) 
    {
	   pFS= uf_RetrieveFlashSectionInfo(FLASH_SECTION_GASOLINE_DATA);
       
    }
    else if(DataType==MeterData) 
    {
       pFS= uf_RetrieveFlashSectionInfo(FLASH_SECTION_METER_DATA);   
        
    }
    else
    {
        printf("retrieveTransactionRecord: DataType Err!\r\n");
        return E_INPUT_PARA;
    }
    
    uf_WriteBuffer(pFS->type,&mark,pFS->rptr, 1);

    pFS->rptr +=sizeof(TaxOnceTransactionRecord);
    if(pFS->rptr >= pFS->base + pFS->size)
        pFS->rptr = pFS->base;

    printf("retrieveTransactionRecordReadPointer: rptr:%x\r\n",pFS->rptr);

    return E_SUCCESS;
}
/****************************************************************************************************
Function Name: TaxHistoryTotalRecordInfo
Description:
    Return the pointer of TaxHistoryTotalRecordInfo;
Parameter:
    NULL
****************************************************************************************************/
TaxHistoryTotalRecordInfo* getTaxHistoryTotalRecordInfo()
{
    return taxHistoryTotalRecordInfo;
}
/*****************************************************************************************************
Function Name: storeRetrieveTaxInfoIndexIntialize
Description:
	This function intends to initialize the flash space for store/retrieve the tax device info record. After this call, the next wptr for 
stroing a new reocrd and the next rptr for retrieveing a record to upload are positioned.

Parameter:
	NULL
Return:
	NULL
*****************************************************************************************************/
errorCode   storeRetrieveTaxInfoIndexIntialize()
{
    u32 i,k;
    u32 spaceSize,spaceStartAddr,spaceEndAddr,spaceSectorNum;
    FLASH_TYPE type;
    u8  c[2];
    u32 addr;

    FlashSectionStruct *pFS = uf_RetrieveFlashSectionInfo(FLASH_SECTION_TAX_INFO);
    spaceStartAddr = pFS->base;
    spaceSize = pFS->size;
    spaceEndAddr = spaceStartAddr + spaceSize;
    spaceSectorNum = pFS->size >> 12;
    type = pFS->type;


    // 1st initialized the wptr and  rptr to the start address of the space
    pFS->wptr = spaceStartAddr;

    // Then look for next address to store the new record
    addr= spaceStartAddr;
    while(addr < spaceEndAddr)
	{
		uf_ReadBuffer(type, c, addr , 2);
		if((c[0] == FRESH) && (c[1] == FRESH))      // we must lookfor where mark=fresh and type = fresh to write next new record
		{
			pFS->wptr = addr;
			break;
		}
		addr= addr + sizeof(TaxDevInfo);
	}

    // Till the end of the space, still not found the proper address, may need erase the 1st sector
    if(addr == spaceEndAddr)
    {
        // readback the last record of the 1st sector, if the mark == done indicates the sector is useless and can be erased
        uf_ReadBuffer(type,c, spaceStartAddr + FLASH_SECTOR_SIZE - sizeof(TaxDevInfo),1);
        if(c[0] == DONE)
        {                    
            uf_EraseSector(type, spaceStartAddr);
            pFS->wptr = pFS->base;
            printf("storeRetrieveTaxInfoIndexIntialize: Erase the 1st sector: 1\r\n");
        }
        else
        {
            printf("!!!!storeRetrieveTaxInfoIndexIntialize: Fatal error 1\r\n");
            // TODO: fata error since no empty space
        }
    }
        
    // 1st initilize rptr to wptr
    pFS->rptr = pFS->wptr;

    // then look for where mark = dirty
    addr= spaceStartAddr;
    while(addr < spaceEndAddr - sizeof(TaxDevInfo))
    {
        uf_ReadBuffer(type, c, addr, 1);
        if(c[0] == DIRTY)
        {
            pFS->rptr = addr;
            break;
        }
        addr = addr + sizeof(TaxDevInfo);
    }

    printf("storeRetrieveTaxInfoIndexIntialize: wptr=%x  rptr=%x\r\n",pFS->wptr,pFS->rptr);

    return E_SUCCESS;    
}
/****************************************************************************************************
Function Name: storeTaxDevInfoRecord
Description:
	This function intends to stor one tax device info record to flash.
Parameter:
	pRecord:[I/O],pointer of TaxDevInfo structure.
Return:
	E_SUCCESS: Succeed to store one record.
	E_NO_MEMORY: flash buffer is full.
*****************************************************************************************************/
errorCode   storeTaxDevInfoRecord(TaxDevInfo *pRecord)
{
    FlashSectionStruct *pFS= uf_RetrieveFlashSectionInfo(FLASH_SECTION_TAX_INFO);
    u8  mark,cc;
    

    printf("storeTaxDevInfoRecord: wptr:%x  rptr:%x\r\n",pFS->wptr,pFS->rptr);
	
	cc=Check_Data((u8*)pRecord,sizeof(TaxDevInfo)-1);
	pRecord->cc=cc;
    
    // 1st write the content
    uf_WriteBuffer(pFS->type, (u8 *)pRecord + 1, pFS->wptr+1, sizeof(TaxDevInfo)-1);
    // Then write the "mark"
    uf_WriteBuffer(pFS->type, (u8 *)pRecord, pFS->wptr, 1);


    pFS->wptr += sizeof(TaxDevInfo);

    if(pFS->wptr >= (pFS->base+pFS->size))
        pFS->wptr = pFS->base;


    // Judge if wptr is at the beginning of a sector, if yes, need judge whether erase the sector or not
    if(pFS->wptr % FLASH_SECTOR_SIZE == 0 )
    {
        uf_ReadBuffer(pFS->type, &mark, pFS->wptr, 1);
        if(mark != FRESH)
        {
            u8   lastMark;
            uf_ReadBuffer(pFS->type, &lastMark,pFS->wptr+FLASH_SECTOR_SIZE - sizeof(TaxDevInfo),1);
            if(lastMark == DONE)
            {
                uf_EraseSector(pFS->type, pFS->wptr);
                printf("storeTaxDevInfoRecord:Erase the next sector: %x\r\n",pFS->wptr);
            }
            else
            {
                printf("storeTaxDevInfoRecord:!!!! No free space\r\n");
                return E_NO_MEMORY;
            }    
        }
    }
    return E_SUCCESS;
}

/****************************************************************************************************
Function Name: retrieveTaxDevInfoRecord
Description:
	This function intends to readback one tax device info record from flash.
Parameter:
	pRecord:[I/O],pointer of TaxDevInfo structure.
Return:
	E_SUCCESS: Succeed to retrieve one record.
	E_EMPTY: no vaild record.
*****************************************************************************************************/
errorCode  retrieveTaxDevInfoRecord(TaxDevInfo *pRecord)
{
    FlashSectionStruct *pFS= uf_RetrieveFlashSectionInfo(FLASH_SECTION_TAX_INFO);
    u8 cc;
    while(pFS->rptr != pFS->wptr)
        {
            uf_ReadBuffer(pFS->type, (u8 *)pRecord, pFS->rptr, sizeof(TaxDevInfo));
            if(pRecord->head.mark == DIRTY) // Normally, this is the case
                {
                    
					cc=Check_Data((u8*)pRecord,sizeof(TaxDevInfo)-1);
					if(pRecord->cc==cc)
					{
						printf("retrieveTaxDevInfoRecord: Successfully retrieve a TaxDevInfo record @ %x\r\n",pFS->rptr);
                        return E_SUCCESS;						
					}
					else
					{
						printf("retrieveTaxDevInfoRecord: CC Err,Retrieve a TaxDevInfo record @ %x Failed!\r\n",pFS->rptr);
                        pFS->rptr += sizeof(TaxDevInfo);
                        if(pFS->rptr >= pFS->base + pFS->size)
                           pFS->rptr = pFS->base;
						
						return E_CHECK_CODE;	
					}
										
                }   
            else        // This is the abnormal case means "mark" was not programmed then somehing abnormal occured
                {
                    pFS->rptr += sizeof(TaxDevInfo);
                    if(pFS->rptr >= pFS->base + pFS->size)
                        pFS->rptr = pFS->base;
                }
        }   

    return E_EMPTY;

}

/****************************************************************************************************
Function Name: retrieveTaxDevInfoRecordReadPointer
Description:
	This function intends to mark the current tax device info record which poined by rptr as "DONE".
Parameter:
	NULL.
Return:
	E_SUCCESS: Succeed to retrieve one record.
*****************************************************************************************************/
errorCode   retrieveTaxDevInfoRecordReadPointer()
{
    u8 mark = DONE;
    FlashSectionStruct *pFS= uf_RetrieveFlashSectionInfo(FLASH_SECTION_TAX_INFO);
    
    uf_WriteBuffer(pFS->type,&mark,pFS->rptr, 1);

    pFS->rptr +=sizeof(TaxDevInfo);
    if(pFS->rptr >= pFS->base + pFS->size)
        pFS->rptr = pFS->base;

    printf("retrieveTaxDevInfoRecordReadPointer: rptr:%x\r\n",pFS->rptr);

    return E_SUCCESS;
}

/*****************************************************************************************************
Function Name: StoreTaxDevInitializationInfo()
Description:
    This function intends to store the tax control device 's initialization info into flash for the future readbak to avoid "1111" issue
if reset.
Parameter:
    devNo:[I], Which tax control device, we only supports 2 tax control devices by far.
    buf:[I], The data pointer which wants to store
    size:[I], how many bytes of data want to store 
Return: 
    E_SUCCESS: Successfully store the info
    E_INPUT_PARA:  Wrong input parameter
    E_FAIL:  Normally flash write error or the flash section is not clean.
*****************************************************************************************************/
errorCode StoreTaxDevInitializationInfo(unsigned char devNo, unsigned char *buf, unsigned short size)
{
    FlashSectionStruct *pFS;
    unsigned int t;
    TaxDevInitializationInfo    *pInfo = (TaxDevInitializationInfo *)buf;
        
    if(devNo == 0 )
        pFS= uf_RetrieveFlashSectionInfo(FLASH_SECTION_TAX_DEV1_INIT);
    else if(devNo == 1)
        pFS= uf_RetrieveFlashSectionInfo(FLASH_SECTION_TAX_DEV2_INIT);
    else
        {
            printf("StoreTaxDevInitializationInfo, wrong input devNo\r\n");
            return E_INPUT_PARA;
        }

    uf_ReadBuffer(pFS->type, (unsigned char *)&t, pFS->base, 4);

    if(t != 0xffffffff)
        {
            printf("StoreTaxDevInitializationInfo: fail\r\n");
            return E_FAIL;
        }    

    uf_WriteBuffer(pFS->type, buf, pFS->base, size);
     
    return E_SUCCESS;
}

/*****************************************************************************************************
Function Name: RetrieveTaxDevInitializationInfo()
Description:
    This function intends to retrieve the tax control device 's initialization info from  flash to avoid "1111" issue if reset.
Parameter:
    devNo:[I], Which tax control device, we only supports 2 tax control devices by far.
    buf:[O], The data pointer which wants to store
    size:[I], how many bytes of data want to store 
Return: 
    E_SUCCESS: Successfully store the info
    E_INPUT_PARA:  Wrong input parameter
    E_FAIL:  Normally flash write error or the flash section is not clean.
*****************************************************************************************************/
errorCode RetrieveTaxDevInitializationInfo(unsigned char devNo, unsigned char *buf, unsigned short size)
{
    FlashSectionStruct *pFS;
    unsigned char *wptr;
    unsigned int t;
    TaxDevInitializationInfo    *pInfo = (TaxDevInitializationInfo *)buf;
        
    if(devNo == 0 )
        pFS= uf_RetrieveFlashSectionInfo(FLASH_SECTION_TAX_DEV1_INIT);
    else if(devNo == 1)
        pFS= uf_RetrieveFlashSectionInfo(FLASH_SECTION_TAX_DEV2_INIT);
    else
        {
            printf("RetrieveTaxDevInitializationInfo, wrong input devNo\r\n");
            return E_INPUT_PARA;
        }

    uf_ReadBuffer(pFS->type, buf, pFS->base,size);
     
    return E_SUCCESS;
}

u8 Check_Data(u8 *buf,u16 dlen)
{
	u8 cc=0;
	for(u16 i=0;i<dlen;i++)
		cc^=buf[i];
	
	return cc;
}

