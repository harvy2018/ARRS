/***************************************************************************************
File name: debugr.c
Date: 2016-9-27
Author: Wangwei/GuokuYan
Description:
     This file contain all the debug command handler which capture the keyboad input and process it. 
***************************************************************************************/

#include "stm32f10x.h"
#include <stdio.h>
#include "flashMgr.h"
#include "uartPort.h"
#include "debug.h"
#include "menu.h"
#include "plcMessager.h"
#include "meterMessager.h"
#include "controlledTaxSampling.h"

#define dbgPrintf	SEGGER_RTT_printf
#define Comx 2

u8 cmd_flag;

void debugCmdHandler()
{
    u8 dlen;
    u8 buf[12];
    u8 flashdata[4096];
    u32 raddr;
    u8 txmode;
    u8 metermode;
    DebugCmd *cmd;

    if(cmd_flag==1)
    {
        cmd_flag=0;
        dlen=uartGetAvailBufferedDataNum(Comx);
        if(dlen>10)
        {
            uartClearRcvBuffer(Comx);
            printf("cmd buf overflow!\r\n");
        }
        else
        {
            uartRead(Comx,buf,dlen);
        }
        cmd=(DebugCmd *)buf;
        uartClearRcvBuffer(Comx);
        if(cmd->cmd_tail==0x0A0D)
        {
            switch  (cmd->cmd_id)
            {
                case EraseFlash:           // Erase historic _total_data and _transaction_data flash space and reset system
                {
                    unsigned int addr;	

                    uf_EraseChip(FLASH_SST25VF016B);
                    bsp_DelayMS(1000);
                    printf("debugCmdHandler: Erased the external flash\r\n");
                    
                    printf("debugCmdHandler: Reset CPU, waiting .......\r\n");
                    
                    
                    NVIC_SystemReset();
                }
                break;

                case SystemReset:
                {
                    printf("debugCmdHandler: Reset CPU, waiting .......\r\n");
                    bsp_DelayMS(1000);
                    NVIC_SystemReset();
                }
                break;
            
                case LogReport:
                {                                  
                    logTask2();
                }
                break;
                        
                case TxModeSet:
                {
                    txmode=atoi(cmd->cmd_data);
                    plcStoreModulationMode(txmode);
                    plcRetrieveModulationMode();
                    plcMessagerReset();
                }
                break;

                case ReadFlash:
                {
                    raddr=atoi(cmd->cmd_data);
                    uf_ReadBuffer(FLASH_SST25VF016B,flashdata,raddr<<12,FLASH_SECTOR_SIZE);
                    for (u16 i=0; i<(FLASH_SECTOR_SIZE); i++)
                    {
                        printf("%02X ", flashdata[i]);
                        if (!((i+1)%16) && i>0)
                        {
                            printf(": %d\r\n", i+1);
                        }
                    }
                }
                break;
            
                case MeterModeSet:
                {
                    metermode=atoi(cmd->cmd_data);
                    
                    if(metermode==0)
                       setMeterBoardMode(GUN_SIGNAL_MODE);
                    else if(metermode==1)
                       setMeterBoardMode(METER_MODE);
                    else if(metermode==2)                        
                        setMeterBoardMode(FREE_MODE);
                    else
                       setMeterBoardMode(GUN_SIGNAL_MODE); 
                    
                    
                    printf("debugCmdHandler: MeterMode=%d\r\n",metermode);
                }
                break;
            
                case Upgrade:
                {
                //	USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);
                //	Main_Menu();
                //	printf("Unsupported command: %04X\r\n",cmd->cmd_id);
                    
                }	
                break;
                
                case TestMode:
                {
                    Main_Menu();
                }
                break;
                
                default: 	
                    printf("Unkonw supported command: %04X\r\n",cmd->cmd_id);
                break;
        
        }     
    }
    else
    {
         printf("cmd tail err!\r\n");
         uartClearRcvBuffer(Comx);
    }			
  }

}        
