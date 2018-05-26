/***************************************************************************************
File name: board.c
Date: 2016-9-7
Author: Wangwei/George Yan
Description:
     This file contain all the interface to initialize the board
***************************************************************************************/

#include "stm32f10x.h"
#include <stdlib.h>
#include <string.h>
#include "bsp_timer.h"
#include "board.h"
#include "18b20.h"
#include "flashMgr.h"
#include "bsp_SST25VF016B.h"
#include "meterMessager.h"
#include "gasolineTransferProtocol.h"
#include "controlledTaxSampling.h"
#include "bsp_i2c.h"

#define  dbgPrintf	SEGGER_RTT_printf

#define  NVIC_VectTab_BIAS	0x6800			// PING:0x6800  PONG:0x42800
#define  LED_INDICATOR_INTERVAL  500

unsigned int serialID[4]={0};
static unsigned char ppFlag = 2;

/**************************************************************************************
Function Name:retrievePingPongFlag
Description:
    Retrieve the PING/PONG flag from flash.
Parameter:
    NULL
Return:
    NULL
**************************************************************************************/
void retrievePingPongFlag()
{
    unsigned char c=0;
    FlashSectionStruct *pFS = uf_RetrieveFlashSectionInfo(FLASH_SECTION_PINGPONG_SEL);
    uf_ReadBuffer(pFS->type, &c, pFS->base, 1);

    if(c == 0xFF){
            ppFlag = 1;
        }    
    else{
            ppFlag = 0;
        }

  //  printf("retrievePingPongFlag: FW working on space:%d\r\n",ppFlag);
    
}

/***************************************************************************************
Function Name: getPingPongFlag
Description:
    Get current work space(Ping or Pong)
Paramter:
    NULL
return:
    0: PING  space
    1: PONG space
****************************************************************************************/
unsigned char getPingPongFlag()
{
    return  ppFlag;   
}

static void initBoardSerialID()
{
    serialID[0] = *(unsigned int*)(0x1FFFF7E8);
    serialID[1] = *(unsigned int*)(0x1FFFF7F0);	
    serialID[2] = *(unsigned int*)(0x1FFFF7EC);
    serialID[3] = 0;   
}
static void NVIC_Configuration()
{
    NVIC_InitTypeDef NVIC_InitStructure;

    /* Configure the NVIC Preemption Priority Bits */  
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
  
	
    NVIC_InitStructure.NVIC_IRQChannel = UART5_IRQn;  // Upstreaming uart port(PLC/485)
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
	
	
	
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;  // Tax uart port 0
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
	
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;  // Tax uart port 1
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
	
	
    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;  // Meter uart port 0
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
	
	
    NVIC_InitStructure.NVIC_IRQChannel = UART4_IRQn;    // Meter uart port 1
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 6;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    
    NVIC_InitStructure.NVIC_IRQChannel = I2C1_EV_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;          //主优先级
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 8;                 //从优先级
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = I2C1_ER_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;          //主优先级
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 9;                 //从优先级
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
	
	
/*	
    //Enable DMA1 Channel5 Interrupt  USART1 RX DMA
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
	
	
	
    //Enable DMA1 Channel6 Interrupt  USART2 RX DMA
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel6_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
	
	
    //Enable DMA1 Channel3 Interrupt  USART3 RX DMA
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 4;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
	
	
	
    //Enable DMA2 Channel3 Interrupt  UART4 RX DMA
    NVIC_InitStructure.NVIC_IRQChannel = DMA2_Channel3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
	
*/	
	
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 6;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 7;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
	
    //NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
    NVIC_InitStructure.NVIC_IRQChannel = RTC_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 10;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}


static void GPIO_Configuration(void)
{
	

	/* 第1步：打开GPIOA GPIOC GPIOD GPIOF GPIOG的时钟
	   注意：这个地方可以一次性全打开
	*/
	
	GPIO_InitTypeDef GPIO_InitStructure;
		
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC \
			| RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOF | RCC_APB2Periph_GPIOG,
				ENABLE);

	/* 第2步：配置所有的按键GPIO为浮动输入模式(实际上CPUf复位后就是输入状态) */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;	
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);	/* PC0,1，2，3 串口拔除检测*/
	
	
	GPIO_InitStructure.GPIO_Pin = MeterModePin;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;	
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(MeterModePort, &GPIO_InitStructure);	/* PC4用于计量模式检测*/
    

//	/* 第3步：配置所有的LED指示灯GPIO为推挽输出模式 */
//	/* 由于将GPIO设置为输出时，GPIO输出寄存器的值缺省是0，因此会驱动LED点亮
//		这是我不希望的，因此在改变GPIO为输出前，先修改输出寄存器的值为1 */

 
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_14| GPIO_Pin_15 |GPIO_Pin_8 | GPIO_Pin_9 ;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure); /* PB14 FLASH WP, PB15 RS485 DIR*/
	
	
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);  	/* PA4 SPI1 NSS */

        // GPIO_SetBits(GPIOC,   GPIO_Pin_6 |GPIO_Pin_7);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7 ;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);


    GPIO_InitStructure.GPIO_Pin = MeterResetPin | MeterModeOutPin;   //PC5、PC7
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(MeterResetPort, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = MeterPort422_DetectPin;   //PB12: 422型计量口检测PIN
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(MeterPort422_DetectPort, &GPIO_InitStructure);

    MeterResetLow;

	PLC_PWR_ON;
    PWR15_ON;
    
   
}

void InitSystem()
{
    NVIC_SetVectorTable(NVIC_VectTab_FLASH,NVIC_VectTab_BIAS);	
	
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, ENABLE);
//   RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
//    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA2, ENABLE);
    
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_BKP, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR,ENABLE);
	
    GPIO_Configuration();	   
    bsp_InitTimer(); 
     
}
/**************************************************************************************
Function Name: initBoard
Description:
	To initialize the board HW components to kick off.
Parameter:
	NULL.
Return:
	NULL
**************************************************************************************/
void initBoard()
{

    CRC_ResetDR();
	
    bsp_InitUart();
    bsp_InitUart2();
    bsp_InitUart3();  
    bsp_InitUart5();
    
    
    #ifdef USE_I2C_FOR_METERBORAD
      I2C_Initializes();
    #else
      bsp_InitUart4();
    #endif
    
      
	
    sf_InitHard();
	
    TIM_Init(TIM2,LED_INDICATOR_INTERVAL);
	
    SF_WP_Lock();//外部flash写保护开	
	
	
    TIM5_Init(CALC_TYPE_US);
	
    NVIC_Configuration(); 
	
    initBoardSerialID();
	
    
}

unsigned char *getBoardSerialID()
{
    return (unsigned char *)serialID;
}



SystemBootType GetSystemBootType()
{
	if(RCC_GetFlagStatus(RCC_FLAG_SFTRST)==SET) //软件复位
	{		  
		// printf("soft reset!\n");			
		 return SOFT_RST;
	}
	else if(RCC_GetFlagStatus(RCC_FLAG_IWDGRST)==SET) //看门狗复位
	{
		//printf("watch dog reset!\n");	
		 return WDG_RST;
		
	}	
	else if(RCC_GetFlagStatus(RCC_FLAG_PINRST)==SET) //引脚复位 
	{
	   //printf("pin reset!\n");	
		 return PIN_RST;		
		
	}
	else if(RCC_GetFlagStatus(RCC_FLAG_PORRST)==SET) //上电掉电复位
	{
	   //printf("pin reset!\n");	
		 return POR_RST;		
		
	}		
}


void DetectMeter422Blocked(void)
{	
	u8 cnt=0;
	for(u8 i=0;i<5;i++)
	{
		if(GPIO_ReadInputDataBit(MeterPort422_DetectPort,MeterPort422_DetectPin))
		  cnt++;
		
		bsp_DelayMS(10);
	}	
		
	if(cnt>=3)	
	   setMeter422Mode(0);//非422
	else
	   setMeter422Mode(1);
			
}

void DetectMeterModeBlocked(void)
{	
	u8 cnt=0;
	for(u8 i=0;i<5;i++)
	{
		if(GPIO_ReadInputDataBit(MeterModePort,MeterModePin))
		  cnt++;
		
		bsp_DelayMS(10);
	}	
    
	if(cnt>=3)	
    {	  
        setMeterBoardMode(METER_MODE);
        MeterModePinSet;
    }
	else
    {	   
        setMeterBoardMode(GUN_SIGNAL_MODE);
        GunModePinSet;
    }
			
}

void DetectMeterMode(void)
{	
	static u8 cnt=0;
	static u8 trycnt=0;

	trycnt++;
	
	if(GPIO_ReadInputDataBit(MeterModePort,MeterModePin))
	   cnt++;
			
	if(trycnt>=5)
	{		
		if(cnt>=3)		
		    setMeterBoardMode(METER_MODE);
		else
		{
		   setMeterBoardMode(GUN_SIGNAL_MODE);

		}
		
		trycnt=0;
		cnt=0;
	} 
			
}

void Erase_FwArea(void)
{
	FlashSectionStruct *pFS;
	u32 EraseAddr; 
	 
	pFS= uf_RetrieveFlashSectionInfo(FLASH_SECTION_FW_UPGRADE);
	 EraseAddr=pFS->base;
	
	 for(u8 i=0;i<3;i++)
	 {		
		uf_EraseBlock(pFS->type,64,EraseAddr);
		EraseAddr+=0x10000;//偏移64K
		bsp_DelayMS(50);
								 		 
	 }
}

u8 IAP_TO_ExFlash(u8 *pdataBuf,u32 flashdestination,u16 dlen)
{
	u8 res;
    FlashSectionStruct *pFS;                                                     
	pFS = uf_RetrieveFlashSectionInfo(FLASH_SECTION_FW_UPGRADE);
	res=uf_WriteBuffer(pFS->type,pdataBuf,flashdestination,dlen);
	
	return res;
	
}

void GPIO_PinReverse(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
{
  /* Check the parameters */
  assert_param(IS_GPIO_ALL_PERIPH(GPIOx));
  assert_param(IS_GPIO_PIN(GPIO_Pin));
  
  GPIOx->ODR ^=  GPIO_Pin;
}

