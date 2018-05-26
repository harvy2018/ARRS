
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "common.h"
#include "menu.h"
#include "ymodem.h"
#include "board.h"
#include "ledStatus.h"
#include "bsp_timer.h"
#include "flashMgr.h"
#include "18b20.h"
#include "bsp_usart.h"
#include "uartPort.h"    
#include "plcMessager.h"

unsigned char comtestdata[]="DXYS_ARRS_COMTEST";
u8 MGBoardTest=0;

void TestModeIOInt(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
   
		
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOB| RCC_APB2Periph_AFIO,ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable,ENABLE);
    
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);	
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4|GPIO_Pin_5;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);	
    
    GPIO_SetBits(GPIOA,GPIO_Pin_15);    
    GPIO_ResetBits(GPIOB,GPIO_Pin_3);
    GPIO_ResetBits(GPIOB,GPIO_Pin_4);    
    GPIO_ResetBits(GPIOB,GPIO_Pin_5);
    
}

void GunSignalAnalogInit(void)   
{
    GPIO_InitTypeDef GPIO_InitStructure;
   
		
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	
    
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);	
    
     
}
 
void LED_Test(void)
{

	  TIM_Cmd(TIM2, DISABLE);
	  GPIO_ResetBits(LED_INDICATOR_PORT,LED_TAX_COMP);
	  GPIO_ResetBits(LED_INDICATOR_PORT,LED_NETWORK_COMP);
    
	  GPIO_SetBits(LED_INDICATOR_PORT, LED_METER_COMP);	    
	  bsp_DelayMS(1000);
	
	  GPIO_ResetBits(LED_INDICATOR_PORT,LED_METER_COMP);
	  GPIO_SetBits(LED_INDICATOR_PORT, LED_TAX_COMP);	 
//	  GPIO_ResetBits(LED_INDICATOR_PORT,LED_NETWORK_COMP); 
	  bsp_DelayMS(1000);       
	
	  GPIO_ResetBits(LED_INDICATOR_PORT,LED_TAX_COMP);
      GPIO_SetBits(LED_INDICATOR_PORT, LED_NETWORK_COMP); 
//	  GPIO_ResetBits(LED_INDICATOR_PORT,LED_METER_COMP);
	  bsp_DelayMS(1000);
		
		
	  ledAllOff();
}

void ExFlash_Test(void)
{
	
	u8 *databuf;
	u8 dataerr=0;
	databuf=(unsigned char *)malloc(FLASH_SECTOR_SIZE);
	if(databuf==0)
	{
		printf("�ڴ�����ʧ��!\r\n");
		return ;
	}
	uf_EraseChip(FLASH_SST25VF016B);
	printf("�ⲿFlash�������!\r\n");
	
	memset(databuf,0x55,FLASH_SECTOR_SIZE);
	
	uf_WriteBuffer(FLASH_SST25VF016B,databuf,0,FLASH_SECTOR_SIZE);
	//printf("�ⲿFlashд���������!\r\n");
	
	memset(databuf,0,FLASH_SECTOR_SIZE);
	uf_ReadBuffer(FLASH_SST25VF016B,databuf,0,FLASH_SECTOR_SIZE);
	//printf("�ⲿFlash�����������!\r\n");
	
	for (u16 i=0; i<(FLASH_SECTOR_SIZE); i++) 
	{ 
				
		if (databuf[i]!=0x55)
		{
			printf("�ⲿFlash����У�����,λ��:%d\r\n", i);	
            dataerr++;			
		}
	}
	
	free(databuf);
	if(!dataerr)
	  printf("�ⲿFlash���ݶ�дУ����ȷ!\r\n");	
	
	uf_EraseChip(FLASH_SST25VF016B);
	
}


void MeterBoardTest(void)
{
   unsigned char availBytes=0; 
   unsigned char combuf[50]={0}; 
   MGBoardTest=1; 
   
   bsp_InitUartTestMode();
   bsp_InitUart2TestMode(); 
    
   Uart1SendBuf(comtestdata,sizeof(comtestdata));
   bsp_DelayMS(500);
   Uart2SendBuf(comtestdata,sizeof(comtestdata));
   

   
}


void GunBoardTest(void)
{
    
    MGBoardTest=2;
    GunSignalAnalogInit();
    
    GPIO_SetBits(GPIOA,GPIO_Pin_9); 
    GPIO_SetBits(GPIOA,GPIO_Pin_2);  
    
    Serial_PutString("ժǹ---1\r\n");
    Serial_PutString("��ǹ---2\r\n");
    Serial_PutString("�˳�---3\r\n");
   while(1)
   {       
        switch (GetKey())
        {
            case '1':
            {
                
                GPIO_ResetBits(GPIOA,GPIO_Pin_9);
                 GPIO_ResetBits(GPIOA,GPIO_Pin_2);    
                
            }
            break;
            
            case '2':
            {
                 GPIO_SetBits(GPIOA,GPIO_Pin_9); 
                GPIO_SetBits(GPIOA,GPIO_Pin_2); 
            }
            break;
            
            case '3':
                return ;
                                    
            
        }
        
   }
                
    
}


void Main_Menu(void)
{
    unsigned char key = 0,availBytes=0,res;
    unsigned char tSym,tInt;
    unsigned short tDec;
   
    unsigned char combuf[50];

    TestModeIOInt();
   
    
    Serial_PutString("\r\n======================================================================");
    Serial_PutString("\r\n               (C) COPYRIGHT ����������˰���缼�����޹�˾");
    Serial_PutString("\r\n");
    Serial_PutString("\r\n  		       ����װ���Զ�������(Version 1.0)");
    Serial_PutString("\r\n");
    Serial_PutString("\r\n======================================================================");
    Serial_PutString("\r\n\r\n");


    Serial_PutString("\r\n=================== ���Բ˵� ============================\r\n");
    Serial_PutString("  LED�Ʋ��� ------------------------------------------- 1\r\n");
    Serial_PutString("  ����ͨ�Ų��� ---------------------------------------- 2\r\n");
    Serial_PutString("  �¶ȴ��������� -------------------------------------- 3\r\n");
    Serial_PutString("  ����PLC��ͨ���� ------------------------------------- 4\r\n");
    Serial_PutString("  �ⲿFlash��д���� ----------------------------------- 5\r\n");
    Serial_PutString("  ARRS-MeterBoard���� --------------------------------- 6\r\n");
    Serial_PutString("  ARRS-GunBoard���� ----------------------------------- 7\r\n");
    Serial_PutString("=========================================================\r\n");

   USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);
	
	
  /* Test if any sector of Flash memory where user application will be loaded is write protected */
//  FlashProtection = FLASH_If_GetWriteProtectionStatus();

  while (1)
  {
	
	key = GetKey();

    switch (key)
    {
      case '1' :
	  {  
		   LED_Test();
		   Serial_PutString("LED�Ʋ�����ɣ�\r\n");
	  }
      break;
  
      case '2' :
	  {
		  Uart1SendBuf(comtestdata,sizeof(comtestdata));
		  Uart2SendBuf(comtestdata,sizeof(comtestdata));
		//  Uart4SendBuf(comtestdata,sizeof(comtestdata));
		  bsp_DelayMS(200);   
		  availBytes=uartGetAvailBufferedDataNum(0);
		  uartRead(0,combuf,availBytes);
		  if(memcmp(combuf,comtestdata,sizeof(comtestdata)))
			 Serial_PutString("����1����δͨ��!!!\r\n"); 		 
		  else
			 Serial_PutString("����1����ͨ��!\r\n");  
		  
		  memset(combuf,0,sizeof(combuf));		  
		  availBytes=uartGetAvailBufferedDataNum(1);
		  uartRead(1,combuf,availBytes);
		  if(memcmp(combuf,comtestdata,sizeof(comtestdata)))
			 Serial_PutString("����2����δͨ��!!!\r\n"); 
		  else
			 Serial_PutString("����2����ͨ��!\r\n");   
		  
//		  memset(combuf,0,sizeof(combuf));
//		  availBytes=uartGetAvailBufferedDataNum(3);
//		  uartRead(3,combuf,availBytes);
//		  if(memcmp(combuf,comtestdata,sizeof(comtestdata)))
//			 Serial_PutString("����4����δͨ��!!!\r\n"); 
//		  else
//			 Serial_PutString("����4����ͨ��!\r\n");  
		  
		  
	  }	  
      break;
	
	  case '3' :
      {
		 
			sampleBoardTempertureTask();		
			bsp_DelayMS(1000);   
			sampleBoardTempertureTask();

			getBoardTemperature(&tSym, &tInt, &tDec);

			if(tSym==0)
				printf("�¶�:%d.%d��\r\n",tInt,tDec);
			else
				printf("�¶�:-%d.%d��\r\n",tInt,tDec);
		  		  
	  }
	  break;
	  
	  case '4' :
      {
		        
		    PLCMessageRCVDescriptor pDes={{0},PLC_MSG_RCV_EMPTY,0}; 
		    PLC_PWR_ON;
			bsp_InitUart5(); 
//	        printf("PLC�ϵ�!\r\n");	
//	        bsp_DelayMS(5000);   			
			res = SendGetSystemInfoRequest();		  	
			if(res == E_SUCCESS)
				printf("��ѯPLC��Ϣ!\r\n");
			else
				printf("��ѯPLC��Ϣʧ��!\r\n");
			
			bsp_DelayMS(1000);   
			
			if((plcMsgRetriever(&pDes) == PLC_MSG_RCV_COMPLETE) && (pDes.head._Id == PLC_MSG_GET_SYSTEM_INFO))
			{
				SystemInfo sysInfo;
				memset(&sysInfo,0,sizeof(SystemInfo));
				memcpy(&sysInfo,&pDes,sizeof(SystemInfo));
				printf("��ȡPLC��Ϣ�ɹ�!\r\n");
			}
			else
			{
				printf("��ȡPLC��Ϣʧ��!\r\n");
			}
			
//			Uart5_Pin_Disable();//disable uart5 pin 
//		    PLC_PWR_OFF;
//			printf("PLC�ص�!\r\n");		
				
	  }
	  break;
	  
      case '5' :
      {
		  ExFlash_Test();
	  }
	  break;
      
      case '6' :
      {
		  MeterBoardTest();
	  }
	  break;
      
      case '7' :
      {
		  GunBoardTest();
           Serial_PutString("\r\n=================== ���Բ˵� ============================\r\n");
            Serial_PutString("  LED�Ʋ��� ------------------------------------------- 1\r\n");
            Serial_PutString("  ����ͨ�Ų��� ---------------------------------------- 2\r\n");
            Serial_PutString("  �¶ȴ��������� -------------------------------------- 3\r\n");
            Serial_PutString("  ����PLC��ͨ���� ------------------------------------- 4\r\n");
            Serial_PutString("  �ⲿFlash��д���� ----------------------------------- 5\r\n");
            Serial_PutString("  ARRS-MeterBoard���� --------------------------------- 6\r\n");
            Serial_PutString("  ARRS-GunBoard���� ----------------------------------- 7\r\n");
            Serial_PutString("=========================================================\r\n");
	  }
	  break;
  
  
	  default:
	  {
		    Serial_PutString("\r\n=================== ���Բ˵� ============================\r\n");
            Serial_PutString("  LED�Ʋ��� ------------------------------------------- 1\r\n");
            Serial_PutString("  ����ͨ�Ų��� ---------------------------------------- 2\r\n");
            Serial_PutString("  �¶ȴ��������� -------------------------------------- 3\r\n");
            Serial_PutString("  ����PLC��ͨ���� ------------------------------------- 4\r\n");
            Serial_PutString("  �ⲿFlash��д���� ----------------------------------- 5\r\n");
            Serial_PutString("  ARRS-MeterBoard���� --------------------------------- 6\r\n");
            Serial_PutString("  ARRS-GunBoard���� ----------------------------------- 7\r\n");
            Serial_PutString("=========================================================\r\n");
	  }
	  break;
    }
  }
}

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
