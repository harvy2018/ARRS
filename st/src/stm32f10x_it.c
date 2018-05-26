/*
*********************************************************************************************************
*	                                  
*	模块名称 : 中断模块
*	文件名称 : stm32f10x_it.c
*	版    本 : V1.0
*	说    明 : 本文件存放所有的中断服务函数。为了便于他人了解程序用到的中断，我们不建议将中断函数移到其他
*			的文件。
*			
*			我们只需要添加需要的中断函数即可。一般中断函数名是固定的，除非您修改了启动文件：
*				Libraries\CMSIS\CM3\DeviceSupport\ST\STM32F10x\startup\arm\startup_stm32f10x_hd.s
*			
*			启动文件是汇编语言文件，定了每个中断的服务函数，这些函数使用了WEAK 关键字，表示弱定义，因此如
*			果我们在c文件中重定义了该服务函数（必须和它同名），那么启动文件的中断函数将自动无效。这也就
*			函数重定义的概念，这和C++中的函数重载的意义类似。
*				
*	修改记录 :
*		版本号  日期       作者    说明
*		v0.1    2009-12-27 armfly  创建该文件，ST固件库版本为V3.1.2
*		v1.0    2011-01-11 armfly  ST固件库升级到V3.4.0版本。
*		v2.0    2011-10-16 armfly  ST固件库升级到V3.5.0版本。
*
*	Copyright (C), 2010-2011, 安富莱电子 www.armfly.com
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
*	Cortex-M3 内核异常中断服务程序
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*	函 数 名: NMI_Handler
*	功能说明: 不可屏蔽中断服务程序。
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/  
void NMI_Handler(void)
{
}

/*
*********************************************************************************************************
*	函 数 名: HardFault_Handler
*	功能说明: 硬件失效中断服务程序。
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/ 
//void HardFault_Handler(void)
//{
//  /* 当硬件失效异常发生时进入死循环 */
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
  /* 当硬件失效异常发生时进入死循环 */
           
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
	/*异常中断发生时，这个异常模式特定的物理R14,即lr被设置成该异常模式将要返回的地址*/
	stacked_lr = ((unsigned int) hardfault_args[5]); 	
	stacked_pc = ((unsigned int) hardfault_args[6]);
	stacked_psr = ((unsigned int) hardfault_args[7]);
	
	//stacked_scb=(*((volatile unsigned int *)(0xE000ED24)));
	stacked_scb=1;
//  SHCSR = (*((volatile unsigned int *)(0xE000ED24))); //系统Handler控制及状态寄存器
//	MFSR = (*((volatile unsigned char *)(0xE000ED28)));	//存储器管理fault状态寄存器	
//	BFSR = (*((volatile unsigned char *)(0xE000ED29)));	//总线fault状态寄存器	
//	UFSR = (*((volatile unsigned short int *)(0xE000ED2A)));//用法fault状态寄存器		
//	HFSR = (*((volatile unsigned int *)(0xE000ED2C)));  //硬fault状态寄存器			
//	DFSR = (*((volatile unsigned int *)(0xE000ED30)));	//调试fault状态寄存器
//	MMAR = (*((volatile unsigned int *)(0xE000ED34)));	//存储管理地址寄存器
//	BFAR = (*((volatile unsigned int *)(0xE000ED38))); //总线fault地址寄存器
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
*	函 数 名: MemManage_Handler
*	功能说明: 内存管理异常中断服务程序。
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/   
//void MemManage_Handler(void)
//{
//  /* 当内存管理异常发生时进入死循环 */
//  while (1)
//  {
//  }
//}

void MemManage_Handler_c(unsigned int * hardfault_args)
{
  /* 当硬件失效异常发生时进入死循环 */
           
}
/*
*********************************************************************************************************
*	函 数 名: BusFault_Handler
*	功能说明: 总线访问异常中断服务程序。
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/    
//void BusFault_Handler(void)
//{
//  /* 当总线异常时进入死循环 */
//  while (1)
//  {
//  }
//}

void BusFault_Handler_c(unsigned int * hardfault_args)
{
 
}
/*
*********************************************************************************************************
*	函 数 名: UsageFault_Handler
*	功能说明: 未定义的指令或非法状态中断服务程序。
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/   
//void UsageFault_Handler(void)
//{
//  /* 当用法异常时进入死循环 */
//  while (1)
//  {
//  }
//}

void UsageFault_Handler_c(unsigned int * hardfault_args)
{
  /* 当硬件失效异常发生时进入死循环 */
           
 
}
/*
*********************************************************************************************************
*	函 数 名: SVC_Handler
*	功能说明: 通过SWI指令的系统服务调用中断服务程序。
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/   
//void SVC_Handler(void)
//{
//}


void SVC_Handler_c(unsigned int * hardfault_args)
{
  /* 当硬件失效异常发生时进入死循环 */
           
 
}
/*
*********************************************************************************************************
*	函 数 名: DebugMon_Handler
*	功能说明: 调试监视器中断服务程序。
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/   
//void DebugMon_Handler(void)
//{
//}


void DebugMon_Handler_c(unsigned int * hardfault_args)
{
  /* 当硬件失效异常发生时进入死循环 */
           
  
}
/*
*********************************************************************************************************
*	函 数 名: PendSV_Handler
*	功能说明: 可挂起的系统服务调用中断服务程序。
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/     
//void PendSV_Handler(void)
//{
//}


void PendSV_Handler_c(unsigned int * hardfault_args)
{
  /* 当硬件失效异常发生时进入死循环 */
           
}
/*
*********************************************************************************************************
*	函 数 名: SysTick_Handler
*	功能说明: 系统嘀嗒定时器中断服务程序。
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/     
void SysTick_Handler(void)
{
	
    tickCount++;  
	SysTick_ISR();	/* 这个函数在bsp_timer.c中 */;
}
/*
*********************************************************************************************************
*	STM32F10x内部外设中断服务程序
*	用户在此添加用到外设中断服务函数。有效的中断服务函数名请参考启动文件(startup_stm32f10x_xx.s)
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


void DMA1_Channel5_IRQHandler(void) //USART1接收DMA
{
	;
}

void DMA1_Channel6_IRQHandler(void) //USART2接收DMA
{
	;
}

void DMA1_Channel3_IRQHandler(void) //USART3接收DMA
{
	;
}

void DMA2_Channel3_IRQHandler(void) //USART4接收DMA
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
*	函 数 名: PPP_IRQHandler
*	功能说明: 外设中断服务程序。
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/    
/* 
	因为中断服务程序往往和具体的应用有关，会用到用户功能模块的变量、函数。如果在本文件展开，会增加大量的
	外部变量声明或者include语句。
	
	因此，我们推荐这个地方只写一个调用语句，中断服务函数的本体放到对应的用户功能模块中。
	增加一层调用会降低代码的执行效率，不过我们宁愿损失这个效率，从而增强程序的模块化特性。
	
	增加extern关键字，直接引用用到的外部函数，避免在文件头include其他模块的头文件
extern void ppp_ISR(void);	
void PPP_IRQHandler(void)
{
	ppp_ISR();
}
*/
