/*
*********************************************************************************************************
*	                                  
*	ģ������ : �ж�ģ��
*	�ļ����� : stm32f10x_it.c
*	��    �� : V1.0
*	˵    �� : ���ļ�������е��жϷ�������Ϊ�˱��������˽�����õ����жϣ����ǲ����齫�жϺ����Ƶ�����
*			���ļ���
*			
*			����ֻ��Ҫ�����Ҫ���жϺ������ɡ�һ���жϺ������ǹ̶��ģ��������޸��������ļ���
*				Libraries\CMSIS\CM3\DeviceSupport\ST\STM32F10x\startup\arm\startup_stm32f10x_hd.s
*			
*			�����ļ��ǻ�������ļ�������ÿ���жϵķ���������Щ����ʹ����WEAK �ؼ��֣���ʾ�����壬�����
*			��������c�ļ����ض����˸÷��������������ͬ��������ô�����ļ����жϺ������Զ���Ч����Ҳ��
*			�����ض���ĸ�����C++�еĺ������ص��������ơ�
*				
*	�޸ļ�¼ :
*		�汾��  ����       ����    ˵��
*		v0.1    2009-12-27 armfly  �������ļ���ST�̼���汾ΪV3.1.2
*		v1.0    2011-01-11 armfly  ST�̼���������V3.4.0�汾��
*		v2.0    2011-10-16 armfly  ST�̼���������V3.5.0�汾��
*
*	Copyright (C), 2010-2011, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "stm32f10x_it.h"
#include "bsp_timer.h"
#include "bsp_usart.h"
#include "ledStatus.h"
#include "SEGGER_RTT.h"
#include "flashmgr.h"	
#include "bsp_SST25VF016B.h"
#include "uartPort.h"
#include "board.h"
#include "meterMessager.h"

unsigned int tickCount = 0;


extern void SysTick_ISR(void);
extern void  updateLocalTick(u32 clock);
extern void ledIndicatorHandler();


/*
*********************************************************************************************************
*	Cortex-M3 �ں��쳣�жϷ������
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*	�� �� ��: NMI_Handler
*	����˵��: ���������жϷ������
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/  
void NMI_Handler(void)
{
}

/*
*********************************************************************************************************
*	�� �� ��: HardFault_Handler
*	����˵��: Ӳ��ʧЧ�жϷ������
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/ 
//void HardFault_Handler(void)
//{
//  /* ��Ӳ��ʧЧ�쳣����ʱ������ѭ�� */
//  NVIC_SystemReset();
// 
//	while (1)
//  {		
//	  //printf("HardFault!!!\r\n");
//  }
//}

//typedef struct _exception_info
//{
//	
//	
//	
//}exception_info;

void hard_fault_handler_c(unsigned int * hardfault_args)
{
  /* ��Ӳ��ʧЧ�쳣����ʱ������ѭ�� */
           
  while (1)
  {
    static unsigned int stacked_r0;
	static unsigned int stacked_r1;
	static unsigned int stacked_r2;
	static unsigned int stacked_r3;
	static unsigned int stacked_r12;
	static unsigned int stacked_lr;
	static unsigned int stacked_pc;
	static unsigned int stacked_psr;
//	static unsigned int SHCSR;
//	static unsigned char MFSR;
//	static unsigned char BFSR;	
//	static unsigned short int UFSR;
//	static unsigned int HFSR;
//	static unsigned int DFSR;
//	static unsigned int MMAR;
//	static unsigned int BFAR;
	
	static unsigned int stacked_scb;

	stacked_r0 = ((unsigned int) hardfault_args[0]);
	stacked_r1 = ((unsigned int) hardfault_args[1]);
	stacked_r2 = ((unsigned int) hardfault_args[2]);
	stacked_r3 = ((unsigned int) hardfault_args[3]);
	stacked_r12 = ((unsigned int) hardfault_args[4]);
	/*�쳣�жϷ���ʱ������쳣ģʽ�ض�������R14,��lr�����óɸ��쳣ģʽ��Ҫ���صĵ�ַ*/
	stacked_lr = ((unsigned int) hardfault_args[5]); 	
	stacked_pc = ((unsigned int) hardfault_args[6]);
	stacked_psr = ((unsigned int) hardfault_args[7]);
	
	//stacked_scb=(*((volatile unsigned int *)(0xE000ED24)));
	stacked_scb=1;
//  SHCSR = (*((volatile unsigned int *)(0xE000ED24))); //ϵͳHandler���Ƽ�״̬�Ĵ���
//	MFSR = (*((volatile unsigned char *)(0xE000ED28)));	//�洢������fault״̬�Ĵ���	
//	BFSR = (*((volatile unsigned char *)(0xE000ED29)));	//����fault״̬�Ĵ���	
//	UFSR = (*((volatile unsigned short int *)(0xE000ED2A)));//�÷�fault״̬�Ĵ���		
//	HFSR = (*((volatile unsigned int *)(0xE000ED2C)));  //Ӳfault״̬�Ĵ���			
//	DFSR = (*((volatile unsigned int *)(0xE000ED30)));	//����fault״̬�Ĵ���
//	MMAR = (*((volatile unsigned int *)(0xE000ED34)));	//�洢�����ַ�Ĵ���
//	BFAR = (*((volatile unsigned int *)(0xE000ED38))); //����fault��ַ�Ĵ���
//	printf("HardFault!---LR=%08X, PC=%08X, PSR=%08X",stacked_lr,stacked_pc,stacked_psr);
    FlashSectionStruct *pFSVer;
    u8 FlagInfo[12]="EXCEPTION!!!";
	u32 timestamp;
    pFSVer = uf_RetrieveFlashSectionInfo(FLASH_SECTION_EXCEPTION_INFO);
    timestamp=getLocalTick();
	
	uf_WriteBuffer(pFSVer->type, (u8 *)hardfault_args, pFSVer->base+12, 32);
	//uf_WriteBuffer(pFSVer->type, (u8 *)&stacked_scb, pFSVer->base+44, sizeof(stacked_scb));
	uf_WriteBuffer(pFSVer->type, (u8*)&timestamp, pFSVer->base+44, sizeof(timestamp));
    uf_WriteBuffer(pFSVer->type, (u8 *)FlagInfo, pFSVer->base,sizeof(FlagInfo));
 
    NVIC_SystemReset();
	while (1);
  }
}
/*
*********************************************************************************************************
*	�� �� ��: MemManage_Handler
*	����˵��: �ڴ�����쳣�жϷ������
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/   
//void MemManage_Handler(void)
//{
//  /* ���ڴ�����쳣����ʱ������ѭ�� */
//  while (1)
//  {
//  }
//}

void MemManage_Handler_c(unsigned int * hardfault_args)
{
  /* ��Ӳ��ʧЧ�쳣����ʱ������ѭ�� */
           
}
/*
*********************************************************************************************************
*	�� �� ��: BusFault_Handler
*	����˵��: ���߷����쳣�жϷ������
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/    
//void BusFault_Handler(void)
//{
//  /* �������쳣ʱ������ѭ�� */
//  while (1)
//  {
//  }
//}

void BusFault_Handler_c(unsigned int * hardfault_args)
{
 
}
/*
*********************************************************************************************************
*	�� �� ��: UsageFault_Handler
*	����˵��: δ�����ָ���Ƿ�״̬�жϷ������
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/   
//void UsageFault_Handler(void)
//{
//  /* ���÷��쳣ʱ������ѭ�� */
//  while (1)
//  {
//  }
//}

void UsageFault_Handler_c(unsigned int * hardfault_args)
{
  /* ��Ӳ��ʧЧ�쳣����ʱ������ѭ�� */
           
 
}
/*
*********************************************************************************************************
*	�� �� ��: SVC_Handler
*	����˵��: ͨ��SWIָ���ϵͳ��������жϷ������
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/   
//void SVC_Handler(void)
//{
//}


void SVC_Handler_c(unsigned int * hardfault_args)
{
  /* ��Ӳ��ʧЧ�쳣����ʱ������ѭ�� */
           
 
}
/*
*********************************************************************************************************
*	�� �� ��: DebugMon_Handler
*	����˵��: ���Լ������жϷ������
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/   
//void DebugMon_Handler(void)
//{
//}


void DebugMon_Handler_c(unsigned int * hardfault_args)
{
  /* ��Ӳ��ʧЧ�쳣����ʱ������ѭ�� */
           
  
}
/*
*********************************************************************************************************
*	�� �� ��: PendSV_Handler
*	����˵��: �ɹ����ϵͳ��������жϷ������
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/     
//void PendSV_Handler(void)
//{
//}


void PendSV_Handler_c(unsigned int * hardfault_args)
{
  /* ��Ӳ��ʧЧ�쳣����ʱ������ѭ�� */
           
}
/*
*********************************************************************************************************
*	�� �� ��: SysTick_Handler
*	����˵��: ϵͳ��શ�ʱ���жϷ������
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/     
void SysTick_Handler(void)
{
	
    tickCount++;  
	SysTick_ISR();	/* ���������bsp_timer.c�� */;
}
/*
*********************************************************************************************************
*	STM32F10x�ڲ������жϷ������
*	�û��ڴ�����õ������жϷ���������Ч���жϷ���������ο������ļ�(startup_stm32f10x_xx.s)
*********************************************************************************************************
*/

void RTC_IRQHandler(void)
{
    if (RTC_GetITStatus(RTC_IT_SEC) != RESET)
        {
            /* Clear the RTC Second interrupt */
            RTC_ClearITPendingBit(RTC_IT_SEC);
            updateLocalTick(RTC_GetCounter());
        }
}


void USART1_IRQHandler(void)
{
    unsigned char c;
	
    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        c =USART_ReceiveData(USART1);
        uartPutCharIntoRcvBuffer(0,c);
    }
}
void USART2_IRQHandler(void)
{
    unsigned char c;
    
    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        c =USART_ReceiveData(USART2);
        uartPutCharIntoRcvBuffer(1,c);
	//	printf("%02X ",c);
    }
}

void USART3_IRQHandler(void)
{
    static u8 tail[2]={0};
	static u8 count=0;
	u8 c;
	extern u8 cmd_flag;
	
    if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
    {		
		 c =USART_ReceiveData(USART3);		
         uartPutCharIntoRcvBuffer(UART_DEBUG_PORT_BASE,c);
		
		if(count>0)
		  count++;
		
		if(c==0x0D)
        {			
		  tail[0]=c;
		  count++;
		} 
			
		if(count==2)
		{
			tail[1]=c;
			count=0;
			if(tail[0]==0x0D && tail[1]==0x0A)
			cmd_flag=1;	
		}		
		
    }
}


void UART4_IRQHandler(void)
{
    u8 c;
    if(USART_GetITStatus(UART4, USART_IT_RXNE) != RESET)
    {
        c =USART_ReceiveData(UART4);
        
        #ifndef USE_I2C_FOR_METERBORAD
          uartPutCharIntoRcvBuffer(UART_METER_PORT_BASE,c);
        #endif
    } 

    		
}

void UART5_IRQHandler(void)
{
	u8  c;

    if(USART_GetITStatus(UART5, USART_IT_RXNE) != RESET)
        { 	
            c=USART_ReceiveData(UART5);
            uartPutCharIntoRcvBuffer(UART_UPSTREAM_NO,c);
		}		
}


void I2C1_EV_IRQHandler(void)
{

    messengerI2CSlaveHandler();
    
}


void DMA1_Channel5_IRQHandler(void) //USART1����DMA
{
	;
}

void DMA1_Channel6_IRQHandler(void) //USART2����DMA
{
	;
}

void DMA1_Channel3_IRQHandler(void) //USART3����DMA
{
	;
}

void DMA2_Channel3_IRQHandler(void) //USART4����DMA
{
	;	
}

void TIM2_IRQHandler(void) // Led indicator timer
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
        {  
            //TIM_Cmd(TIM2,DISABLE);
            TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
            ledIndicatorHandler();
        }
}
/*
*********************************************************************************************************
*	�� �� ��: PPP_IRQHandler
*	����˵��: �����жϷ������
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/    
/* 
	��Ϊ�жϷ�����������;����Ӧ���йأ����õ��û�����ģ��ı���������������ڱ��ļ�չ���������Ӵ�����
	�ⲿ������������include��䡣
	
	��ˣ������Ƽ�����ط�ֻдһ��������䣬�жϷ������ı���ŵ���Ӧ���û�����ģ���С�
	����һ����ûή�ʹ����ִ��Ч�ʣ�����������Ը��ʧ���Ч�ʣ��Ӷ���ǿ�����ģ�黯���ԡ�
	
	����extern�ؼ��֣�ֱ�������õ����ⲿ�������������ļ�ͷinclude����ģ���ͷ�ļ�
extern void ppp_ISR(void);	
void PPP_IRQHandler(void)
{
	ppp_ISR();
}
*/
