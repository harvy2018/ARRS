
#ifndef _FLASHMGR_H_
#define _FLASHMGR_H_

#include "stm32f10x.h"	
#include "stdint.h"	
#include "stm32f10x_flash.h"
#include "errorCode.h"

#define 	FLASH_SECTOR_SIZE		4096
#define   FLASH_VERSION_MAGIC   "FLHV"

#pragma pack(1)

typedef enum _FlashSectionID
{
	FLASH_SECTION_FW_UPGRADE=0,
	FLASH_SECTION_GASOLINE_DATA,
    FLASH_SECTION_METER_DATA,
    FLASH_SECTION_TAXCONTROL_HISTORY,
    
	FLASH_SECTION_TAX_DEV1_INIT,
	FLASH_SECTION_TAX_DEV2_INIT,
	
    FLASH_SECTION_TAX_INFO,
	FLASH_SECTION_EXCEPTION_INFO,
    FLASH_SECTION_FAULT_INFO,
	FLASH_SECTION_STATISTICS_DATA,
    
	FLASH_SECTION_FW_BOOT,
	FLASH_SECTION_PINGPONG_SEL,
	FLASH_SECTION_FLASH_MANAGER_VER,
	FLASH_SECTION_FW_PING,
	FLASH_SECTION_FW_PONG,
	FLASH_SECTION_PLC_MODULATION,
	FLASH_SECTION_DEVICE_PARAMETER,
	FLASH_SECTION_NO_USE,
	FLASH_SECTION_END,
	FLASH_SECTION_MAX	
}FlashSectionID;

typedef enum _FLASH_TYPE
{
	FLASH_STM32F103RET6=0,			// CPU internal flash
	FLASH_SST25VF016B,			// CPU external board installed flash
	FLASH_MAX	
}FLASH_TYPE;	

typedef struct _FlashSectionStruct
{
    FLASH_TYPE  type;     	//  Flash type: FLASH_STM32F103RET6 or FLASH_SST25VF016B
    unsigned int  base;      		//  above section's flash start address, normally sector aligned
    unsigned int  size;	 		//  section size: # of bytes
    unsigned int  wptr;     		//  write pointer
    unsigned int  rptr;      		//  read pointer	
}FlashSectionStruct;


typedef struct _FLASHVersion
{
    unsigned char  magic[4];
    unsigned char  version[4];
}FLASHVersion;


typedef struct _FLASHOperationErrorStatistic
{
    unsigned int   eraseCount;
    unsigned int   writeCount;
	unsigned int   errWaddr;
	unsigned int   errEaddr;
}FLASHOperationErrorStatistic;


typedef struct _EXCEPTION_INFO
{
	unsigned int stacked_r0;
	unsigned int stacked_r1;
	unsigned int stacked_r2;
	unsigned int stacked_r3;
	unsigned int stacked_r12;
	unsigned int stacked_lr;
	unsigned int stacked_pc;
	unsigned int stacked_psr;
	//unsigned int stacked_scb;
	unsigned int timestamp;
	
}EXCEPTION_INFO;



errorCode uf_EraseChip(FLASH_TYPE type);
errorCode uf_EraseSector(FLASH_TYPE type,uint32_t _uiSectorAddr);
errorCode uf_EraseBlock(FLASH_TYPE type,uint8_t blocksize, uint32_t _uiSectorAddr);
uint8_t uf_WriteBuffer(FLASH_TYPE type,uint8_t* _pBuf, uint32_t _uiWriteAddr, uint16_t _usWriteSize);
void uf_ReadBuffer(FLASH_TYPE type,uint8_t * _pBuf, uint32_t _uiReadAddr, uint32_t _uiSize);

FlashSectionStruct  *uf_RetrieveFlashSectionInfo(FlashSectionID sectionID);
errorCode flashVersionCheck();
FLASHOperationErrorStatistic * getFlashErrorStatistic();
void exception_check(void);
#endif


