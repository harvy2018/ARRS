/***************************************************************************************
File name: flashMgr.c
Date: 2016-8-24
Author: Wangwei  622848 1360 3103 35218
Description:
     This file contain all the interface related with flash operation.
***************************************************************************************/
#include "SEGGER_RTT.h"
#include "stm32f10x.h"	
#include "flashmgr.h"	
#include "bsp_SST25VF016B.h"
#include "errorCode.h"
#include "version.h"

#define HARDWARE_EXCEPTION_MAGIC  "EXCEPTION!!!"


#define dbgPrintf  SEGGER_RTT_printf

EXCEPTION_INFO exception_info;  
u8 exception_flag=0;
static unsigned char flashVersion[4]={1,0,6,0};
static FLASHOperationErrorStatistic flashErrorStatistic[FLASH_MAX];

// Below static definition define all the functional section of both internal and external flash to unify flash management
static FlashSectionStruct flashSectionStruct[FLASH_SECTION_MAX]=
{
	{ FLASH_SST25VF016B,	0x0000000,	512*1024,   0x0000000,	0x0000000},    		//FLASH_SECTION_FW_UPGRADE
	{ FLASH_SST25VF016B,	0x0090000, 	256*1024,	0x0090000,  0x0090000},          //FLASH_SECTION_GAS_DATA: [X]Followed by hole
    { FLASH_SST25VF016B,	0x00E0000, 	256*1024,   0x00E0000,  0x00E0000},         //FLASH_SECTION_METER_DATA:
   
	{ FLASH_SST25VF016B,	0x0140000,  64*1024,   0x0140000,  0x0140000},        //FLASH_SECTION_TAXCONTROL_HISTORY
	
    
    { FLASH_SST25VF016B,	0x01A0000, 	4*1024,	    0x01A0000,  0x01A0000},       // FLASH_SECTION_TAX_DEV1_INIT  
    { FLASH_SST25VF016B,	0x01A1000, 	4*1024,	    0x01A1000,  0x01A1000},       // FLASH_SECTION_TAX_DEV2_INIT

    { FLASH_SST25VF016B,	0x01A2000,  8*1024,     0x01A2000,  0x01A2000},  	   //FLASH_SECTION_TAX_INFO
	{ FLASH_SST25VF016B,	0x01A4000,  4*1024,     0x01A4000,  0x01A4000},	         //FLASH_SECTION_EXCEPTION_INFO
	{ FLASH_SST25VF016B,	0x01A5000,  4*1024,     0x01A5000,  0x01A5000},	         //FLASH_SECTION_FAULT_INFO
	
    { FLASH_SST25VF016B,	0x01FE000,  8*1024, 	0x01FE000,  0x01FE000},     //FLASH_SECTION_STATISTICS_DATA
	

    { FLASH_STM32F103RET6,	0x8000000, 	0x3000,     0x8000000, 	0x8000000},			//FLASH_SECTION_FW_BOOT
	{ FLASH_STM32F103RET6,	0x8003000, 	0x1000,     0x8003000, 	0x8003000},			//FLASH_SECTION_PINGPONG_SEL
	{ FLASH_STM32F103RET6,	0x8004000, 	0x1000,     0x8004000, 	0x8004000},			//FLASH_SECTION_FLASH_MANAGER_VER
	{ FLASH_STM32F103RET6,	0x8006000, 	176*1024,   0x8006000, 	0x8006000},			//FLASH_SECTION_FW_PING,  64K reserved.
	{ FLASH_STM32F103RET6,	0x8042000, 	176*1024,   0x8042000,  0x8042000}, 		//FLASH_SECTION_FW_PONG
	
	{ FLASH_STM32F103RET6,	0x806E000,  4*1024,     0x806E000,  0x806E000}, 		// FLASH_SECTION_PLC_MODULATION
	{ FLASH_STM32F103RET6,	0x806F000,  4*1024,     0x806F000,  0x806F000},  		// FLASH_SECTION_DEVIE_PARAMETER
	{ FLASH_STM32F103RET6,	0x8070000,  64*1024,	0x8072000,  0x8072000}, 		//No use
	{ FLASH_STM32F103RET6,	0x8080000,  0,		    0x8080000,  0x8080000}          // End of internal flash   
};

FLASHOperationErrorStatistic * getFlashErrorStatistic()
{
    return (&flashErrorStatistic[0]);
}

/****************************************************************************************************
Function Name: uf_RetrieveFlashSectionInfo
Description:
	This function intends to retrieve the specific flash section information as per SECTIONID
Parameter:
	sectionID:[I], section enum ID
Return:
	FlashSectionStruct pointer.
*****************************************************************************************************/
FlashSectionStruct  *uf_RetrieveFlashSectionInfo(FlashSectionID sectionID)
{
	return (FlashSectionStruct *)((unsigned char *)flashSectionStruct + sectionID * sizeof(FlashSectionStruct));
}
/****************************************************************************************************
Function Name: uf_EraseChip
Description:
	This function intends to erase the whole flash: either internal flash or external flash
Parameter:
	type:[I] Flash type, internal flash or external flash
return:
	E_SUCCESS: erased successfully
	E_NOT_SUPPORT: erase failed due to not support
****************************************************************************************************/
errorCode uf_EraseChip(FLASH_TYPE type)
{
	if(type==FLASH_SST25VF016B)
	{
		SF_WP_Unlock();
		sf_EraseChip();
		SF_WP_Lock();
		return E_SUCCESS;
	}
	else
	{
		// TODO: Add body to erase the internal flash
		return E_NOT_SUPPORT;
	}
}

/****************************************************************************************************
Function Name: uf_EraseSector
Description:
	This function intends to erase specified sector: either internal flash or external flash
Parameter:
	type:[I] Flash type, internal flash or external flash
	_uiSectorAddr:[I], sector start address
return:
	E_SUCCESS: erased successfully or record the errorCount
****************************************************************************************************/
errorCode uf_EraseSector(FLASH_TYPE type,uint32_t _uiSectorAddr)
{
     if(type==FLASH_SST25VF016B)
     {
         SF_WP_Unlock();
		 sf_EraseSector(_uiSectorAddr);
		 SF_WP_Lock();
     }	
	 else if(type==FLASH_STM32F103RET6)     // STM internal flash page is 2K, we need erase 2 pages since we unified sector to 4K
	 {
			FLASH_Unlock();
		 	if(FLASH_ErasePage(_uiSectorAddr) != FLASH_COMPLETE)
            {
                bsp_DelayMS(5);
                if(FLASH_ErasePage(_uiSectorAddr) != FLASH_COMPLETE)
                {
                    flashErrorStatistic[FLASH_STM32F103RET6].eraseCount++;
					flashErrorStatistic[FLASH_STM32F103RET6].errEaddr=_uiSectorAddr;
                }
            }
			
            if(flashSectionStruct[FLASH_SECTION_FW_PING].base!=_uiSectorAddr && flashSectionStruct[FLASH_SECTION_FW_PONG].base!=_uiSectorAddr)
            {
				if(FLASH_ErasePage(_uiSectorAddr+2048) != FLASH_COMPLETE)
				{
					bsp_DelayMS(5);
					if(FLASH_ErasePage(_uiSectorAddr+2048) != FLASH_COMPLETE)
					{
						flashErrorStatistic[FLASH_STM32F103RET6].eraseCount++;
						flashErrorStatistic[FLASH_STM32F103RET6].errEaddr=_uiSectorAddr+2048;
					}
				}	    
			}   
            FLASH_Lock();
	 }
     
     bsp_DelayMS(10);
     
	return E_SUCCESS;	 	 
}
/****************************************************************************************************
Function Name: uf_EraseBlock
Description:
	This function intends to erase a block: either internal flash or external flash
Parameter:
	type:[I] Flash type, internal flash or external flash
	blocksize:[I], block size
	_uiSectorAddr:[I], sector start address
return:
	E_SUCCESS: erased successfully
	E_NOT_SUPPORT: erase failed due to not support
****************************************************************************************************/
errorCode uf_EraseBlock(FLASH_TYPE type,uint8_t blocksize, uint32_t _uiSectorAddr)
{
	if(type==FLASH_SST25VF016B)
		{
			SF_WP_Unlock();
			sf_EraseBlock(blocksize, _uiSectorAddr);
			SF_WP_Lock();
			return E_SUCCESS;
		}	
	  return E_NOT_SUPPORT;
}
/****************************************************************************************************
Function Name: uf_WriteBuffer
Description:
	This function intends to write the buffed data into specified flash address
Parameter:
	type:[I] Flash type, internal flash or external flash
	_pBuf:[I], source buffer pointer
	_uiWriteAddr:[I], target flash address
	_usWriteSize:[I], # bytes of data
return:
	1: Succeed  0: Fail
****************************************************************************************************/
uint8_t uf_WriteBuffer(FLASH_TYPE type,uint8_t* _pBuf, uint32_t _uiWriteAddr, uint16_t _usWriteSize)
{
	uint8_t res=1;	
	if(type==FLASH_SST25VF016B)
    {
        SF_WP_Unlock();
		if(!sf_WriteBuffer(_pBuf,  _uiWriteAddr,  _usWriteSize))
        {
            bsp_DelayMS(5);
            if(!sf_WriteBuffer(_pBuf,  _uiWriteAddr,  _usWriteSize))
			{
				flashErrorStatistic[FLASH_SST25VF016B].writeCount++;
				flashErrorStatistic[FLASH_SST25VF016B].errWaddr=_uiWriteAddr;
				res=0;
			}
        }
		SF_WP_Lock();
    }	
	else if(type==FLASH_STM32F103RET6)
    {
      FLASH_Unlock();
      if(!FLASH_WriteBank(_pBuf, _uiWriteAddr, _usWriteSize))
        {
            bsp_DelayMS(5);                   
            if(!FLASH_WriteBank(_pBuf, _uiWriteAddr, _usWriteSize))
                {
                    flashErrorStatistic[FLASH_STM32F103RET6].writeCount++;
					flashErrorStatistic[FLASH_STM32F103RET6].errWaddr=_uiWriteAddr;
                    res = 0;
                }
        }
      FLASH_Lock();		  	
    }
	
	return res;	
}
/****************************************************************************************************
Function Name: uf_ReadBuffer
Description:
	This function intends to read specified number bytes of data from specified flash address  
Parameter:
	type:[I] Flash type, internal flash or external flash
	_pBuf:[I/O], target buffer pointer
	_uiReadAddr:[I], source flash address
	_uiSize:[I], # bytes of data
return:
	E_SUCCESS: read complete successfully
	E_NOT_SUPPORT: read  failed due to not support
****************************************************************************************************/
void uf_ReadBuffer(FLASH_TYPE type,uint8_t * _pBuf, uint32_t _uiReadAddr, uint32_t _uiSize)
{
	 if(type==FLASH_SST25VF016B)
	  	{
	  		sf_ReadBuffer(_pBuf,  _uiReadAddr,  _uiSize);
	 	}
	 else
	 	{
	 		memcpy(_pBuf,_uiReadAddr,_uiSize);
	 	}
}

void exception_check(void)
{
	FlashSectionStruct *pFSVer;
    u8 FlagInfo[12];
			
    pFSVer = uf_RetrieveFlashSectionInfo(FLASH_SECTION_EXCEPTION_INFO);
	uf_ReadBuffer(pFSVer->type, FlagInfo, pFSVer->base, sizeof(FlagInfo));
	
	if(!memcmp(FlagInfo,HARDWARE_EXCEPTION_MAGIC,12))
	{
		exception_flag=1;
		uf_ReadBuffer(pFSVer->type, (u8*)&exception_info, pFSVer->base+12, sizeof(EXCEPTION_INFO));
		uf_EraseSector(pFSVer->type,pFSVer->base);
	}
}

/****************************************************************************************************
Function Name: uf_ReadBuffer
Description:
	This function intends to read specified number bytes of data from specified flash address  
Parameter:
	type:[I] Flash type, internal flash or external flash
	_pBuf:[I/O], target buffer pointer
	_uiReadAddr:[I], source flash address
	_uiSize:[I], # bytes of data
return:
	E_SUCCESS: read complete successfully
	E_NOT_SUPPORT: read  failed due to not support
****************************************************************************************************/
errorCode flashVersionCheck()
{
    FlashSectionStruct *pFSVer;
    FLASHVersion flashVer;
    unsigned char eraseFlag=0;
    unsigned char res=0;
    
    pFSVer = uf_RetrieveFlashSectionInfo(FLASH_SECTION_FLASH_MANAGER_VER);

    uf_ReadBuffer(pFSVer->type, (unsigned char *)&flashVer, pFSVer->base, sizeof(FLASHVersion));

    if(!memcmp(flashVer.magic,FLASH_VERSION_MAGIC,4))
	{
		printf("flashVersionCheck: Magic Code OK, Ver:%d.%d.%d.%d\r\n",flashVer.version[0],flashVer.version[1],flashVer.version[2],flashVer.version[3]);
		if(memcmp(flashVer.version,flashVersion,4))
		{
			eraseFlag = 1;	
			printf("NewFlashVer is Ver:%d.%d.%d.%d\r\n",flashVersion[0],flashVersion[1],flashVersion[2],flashVersion[3]);			
		}
		else
			return E_SUCCESS;
	}
    else    // previous version, need to cleanup flash
	{
		printf("flashVersionCheck: Magic Code NG\r\n");
		eraseFlag = 1;
	}

    if(eraseFlag)
	{
		unsigned int addr;
		
		// 1st erase the whole external flash chips
		uf_EraseChip(FLASH_SST25VF016B);
		bsp_DelayMS(1000);
		printf("flashVersionCheck: Erased the external flash\r\n");
		
		// 2nd erase the necessary internal flash area, attention not touch the key area marked in  above "flashSectionStruct" definition.
		// Start address:  end of FLASH_SECTION_FW_PONG area
		// End address:    the size of this flash: 512KB
		addr = flashSectionStruct[FLASH_SECTION_FW_PONG].base + flashSectionStruct[FLASH_SECTION_FW_PONG].size;
		while(addr < flashSectionStruct[FLASH_SECTION_END].base)
			{
				uf_EraseSector(FLASH_STM32F103RET6, addr);
				bsp_DelayMS(50);
				addr+= FLASH_SECTOR_SIZE;
				printf("flashVersionCheck: internal flash add:%08X  ",addr);
			}
		printf("flashVersionCheck: Erased necessary parts of internal flash\r\n");

		// last, write the correct "MAGIC" code and version number
		uf_EraseSector(pFSVer->type, pFSVer->base);
		uf_WriteBuffer(pFSVer->type, flashVersion,pFSVer->base+4, 4);
		uf_WriteBuffer(pFSVer->type, FLASH_VERSION_MAGIC, pFSVer->base , 4);
        
		printf("flashVersionCheck: Write correct magic code and version\r\n");
		bsp_DelayMS(50);
	}

    return E_SUCCESS;
    
}


