/*
*********************************************************************************************************
*	                                  
*	ģ������ : ��ʱ��ģ��
*	�ļ����� : bsp_timer.c
*	��    �� : V2.0
*	˵    �� : ����systick�жϣ�ʵ�����ɸ������ʱ��
*	�޸ļ�¼ :
*		�汾��  ����       ����    ˵��
*		v0.1    2009-12-27 armfly  �������ļ���ST�̼���汾ΪV3.1.2
*		v1.0    2011-01-11 armfly  ST�̼���������V3.4.0�汾��
*       v2.0    2011-10-16 armfly  ST�̼���������V3.5.0�汾��
*
*	Copyright (C), 2010-2011, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include <stdio.h>

#include "bsp_timer.h"
#include "controlledTaxSampling.h"

/* 
	�ڴ˶������ɸ������ʱ��ȫ�ֱ���
	ע�⣬��������__IO �� volatile����Ϊ����������жϺ���������ͬʱ�����ʣ��п�����ɱ����������Ż���
*/
#define TMR_COUNT	8		/* �����ʱ���ĸ�������1��������bsp_DelayMS()ʹ�� */

#define TimeSyn_Period    (6*3600)
#define TaxSample_Period  (6*3600)
#define Log_Period  (60)

SOFT_TMR g_Tmr[TMR_COUNT];

/* ȫ������ʱ�䣬��λ1ms������uIP */
__IO int32_t g_iRunTime = 0;

static void bsp_SoftTimerDec(SOFT_TMR *_tmr);
RCC_ClocksTypeDef m_CLK;


/*******************************************************************************************
Function Name: getSysTick()
Description:
     Get current system tick count from bootup.   1ms tick count
Parameter:
     NULL
Return:
    Current 1ms based tick count from system bootup.
*******************************************************************************************/
int32_t  getSysTick()
{
    return g_iRunTime;
}



void bsp_InitTimer(void)
{
	uint8_t i;
	
	u32 clk;
	
	/* �������е������ʱ�� */
	for (i = 0; i < TMR_COUNT; i++)
	{
		g_Tmr[i].count = 0;
		g_Tmr[i].flag = 0;
	}
	
	/* 
		����systic�ж�����Ϊ1ms��������systick�жϡ�
    	��������� \Libraries\CMSIS\CM3\CoreSupport\core_cm3.h 
    	
    	Systick�ж�������(\Libraries\CMSIS\CM3\DeviceSupport\ST\STM32F10x\startup\arm\
    		startup_stm32f10x_hd.s �ļ��ж���Ϊ SysTick_Handler��
    	SysTick_Handler������ʵ����stm32f10x_it.c �ļ���
    	SysTick_Handler����������SysTick_ISR()�������ڱ��ļ�ĩβ��
    */	
	RCC_GetClocksFreq(&m_CLK);
	
	SysTick_Config(m_CLK.HCLK_Frequency/ 1000);
	
}

/*
*********************************************************************************************************
*	�� �� ��: SysTick_ISR
*	����˵��: SysTick�жϷ������ÿ��1ms����1��
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SysTick_ISR(void)
{
	static uint16_t s_count = 0;
	extern u16 AdcValue;
	extern u32 runtime;
    extern u8 TimeSyn_Flag;  
    extern u8 TaxSampleUnLock[];    
    extern u32 count100ms;
    extern u8  LogUpload_Flag;
    
    
	uint8_t i;
	

	for (i = 0; i < TMR_COUNT; i++)
	{
		bsp_SoftTimerDec(&g_Tmr[i]);
	}

	g_iRunTime++;	/* ȫ������ʱ��ÿ1ms��1 */	

    
	if (g_iRunTime == 0x7FFFFFFF)
	{
		g_iRunTime = 0;
	}
		
   
	if ( ++s_count>= 1000)// 
	{
		s_count = 0;
		runtime++;
        
        if(runtime % Log_Period==0)
        {   
            LogUpload_Flag=1;
        }
             
        if(runtime % TimeSyn_Period==0)
        {   
            TimeSyn_Flag=1;
        }
        
        if(runtime % TaxSample_Period==0)
        {            
            TaxSampleUnLock[0]=1;
            TaxSampleUnLock[1]=1;
        }
		
	}
   

}

/*
*********************************************************************************************************
*	�� �� ��: bsp_SoftTimerDec
*	����˵��: ÿ��1ms�����ж�ʱ��������1�����뱻SysTick_ISR�����Ե��á�
*	��    �Σ�_tmr : ��ʱ������ָ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void bsp_SoftTimerDec(SOFT_TMR *_tmr)
{
	if (_tmr->flag == 0)
	{
		if (_tmr->count > 0)
		{
			/* �����ʱ����������1�����ö�ʱ�������־ */
			if (--_tmr->count == 0)
			{
				_tmr->flag = 1;
			}
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_DelayMS
*	����˵��: ms���ӳ٣��ӳپ���Ϊ����1ms
*	��    �Σ�n : �ӳٳ��ȣ���λ1 ms�� n Ӧ����2
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_DelayMS(uint32_t n)
{
	/* ���� n = 1 �������������� */
	if (n <= 1)
	{
		n = 2;
	}
	
	__set_PRIMASK(1);  		/* ���ж� */
	g_Tmr[0].count = n;
	g_Tmr[0].flag = 0;
	__set_PRIMASK(0);  		/* ���ж� */

	while (1)
	{
		CPU_IDLE();	/* �˴��ǿղ������û����Զ��壬��CPU����IDLE״̬���Խ��͹��ģ���ʵ��ι�� */

		/* �ȴ��ӳ�ʱ�䵽 */
		if (g_Tmr[0].flag == 1)
		{
			break;
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_StartTimer
*	����˵��: ����һ����ʱ���������ö�ʱ���ڡ�
*	��    �Σ�	_id     : ��ʱ��ID��ֵ��1,TMR_COUNT-1�����û���������ά����ʱ��ID���Ա��ⶨʱ��ID��ͻ��
*						  ��ʱ��ID = 0 ������bsp_DelayMS()����
*				_period : ��ʱ���ڣ���λ1ms
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_StartTimer(uint8_t _id, uint32_t _period)
{
	if (_id >= TMR_COUNT)
	{
		/* while(1); ���� */
		return;
	}

	__set_PRIMASK(1);  		/* ���ж� */
	g_Tmr[_id].count = _period;
	g_Tmr[_id].flag = 0;
	__set_PRIMASK(0);  		/* ���ж� */
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_CheckTimer
*	����˵��: ��ⶨʱ���Ƿ�ʱ
*	��    �Σ�	_id     : ��ʱ��ID��ֵ��1,TMR_COUNT-1�����û���������ά����ʱ��ID���Ա��ⶨʱ��ID��ͻ��
*						  0 ����
*				_period : ��ʱ���ڣ���λ1ms
*	�� �� ֵ: ���� 0 ��ʾ��ʱδ���� 1��ʾ��ʱ��
*********************************************************************************************************
*/
uint8_t bsp_CheckTimer(uint8_t _id)
{
	if (_id >= TMR_COUNT)
	{
		return 0;
	}

	if (g_Tmr[_id].flag == 1)
	{
		g_Tmr[_id].flag = 0;
		return 1;
	}
	else
	{
		return 0;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_GetRunTime
*	����˵��: ��ȡCPU����ʱ�䣬��λ1ms
*	��    �Σ���
*	�� �� ֵ: CPU����ʱ�䣬��λ1ms
*********************************************************************************************************
*/
int32_t bsp_GetRunTime(void)
{
	int runtick; 

	__set_PRIMASK(1);  		/* ���ж� */
	
	runtick = g_iRunTime;	/* ������Systick�жϱ���д����˹��жϽ��б��� */
		
	__set_PRIMASK(0);  		/* ���ж� */

	return runtick;
}

TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
TIM_OCInitTypeDef  TIM_OCInitStructure;

uint16_t PrescalerValue = 0;


void TIM_Init(TIM_TypeDef* TIMx,u32 mS)
{
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	TIM_DeInit(TIMx);
	/* Time base configuration */
	TIM_TimeBaseStructure.TIM_Period = mS << 1;     
	TIM_TimeBaseStructure.TIM_Prescaler = 36000-1;//��ʱ��ʱ��Ƶ��72M
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIMx, &TIM_TimeBaseStructure);
	TIM_ClearFlag(TIMx, TIM_FLAG_Update); 
	TIM_ARRPreloadConfig(TIMx, ENABLE);
	/* TIM IT enable */
	TIM_ITConfig(TIMx,TIM_IT_Update, ENABLE);
	/* TIM2 enable counter */
	TIM_Cmd(TIMx, ENABLE);
}

void TIM5_Init(CALC_TYPE type)  
{  
    TIM_TimeBaseInitTypeDef Tim5;  
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5,ENABLE);  
    Tim5.TIM_Period=1; //???   
    if(type==CALC_TYPE_S) //��ʱ��SΪ��λʱ��ʱ��Ƶ��57600Hz���ⲿ��Ҫ1250�μ�ʱ   
    {  
        Tim5.TIM_Prescaler=57600-1; //Ԥ��Ƶ 72MHz / 57600= 1250Hz   
    }else if(type==CALC_TYPE_MS)  
    {  
        Tim5.TIM_Prescaler=995-1; ////100KHz ,��ʱ������100��Ϊ1ms   ��Ƶ99532800
 
    }else if(type==CALC_TYPE_US)  
    {     
        Tim5.TIM_Prescaler=72-1; //1MHz ,����1��Ϊus   
    }else  
    {  
        Tim5.TIM_Prescaler=9953-1;  //10KHZ
    }  
    Tim5.TIM_ClockDivision=0;  
    Tim5.TIM_CounterMode=TIM_CounterMode_Down; //���¼���   
    TIM_TimeBaseInit(TIM5,&Tim5);         
}  
  
void TIM5_S_CALC(uint32_t s)  
{  
    u16 counter=(s*1250)&0xFFFF; //ǰ�ᶨʱ��ʱ��Ϊ1250Hz   
    TIM_Cmd(TIM5,ENABLE);  
    TIM_SetCounter(TIM5,counter); //���ü���ֵ   
      
    while(counter>1)  
    {  
        counter=TIM_GetCounter(TIM5);  
    }  
    TIM_Cmd(TIM5,DISABLE);  
}  
  
void TIM5_MS_CALC(uint32_t ms)  
{  
    u16 counter=(ms*100)&0xFFFF;   //&0xFFFF ��ֹ���65535
    TIM_Cmd(TIM5,ENABLE);  
    TIM_SetCounter(TIM5,counter); //���ü���ֵ   
      
    while(counter>1)  
    {  
       counter=TIM_GetCounter(TIM5); 
			
    }  
    TIM_Cmd(TIM5,DISABLE);  
}  
  
void TIM5_US_CALC(uint32_t us)  
{  
    u16 counter=us&0xffff;  
    TIM_Cmd(TIM5,ENABLE);  
    TIM_SetCounter(TIM5,counter); //���ü���ֵ   
  
    while(counter>1)  
    {  
        counter=TIM_GetCounter(TIM5);  
    }  
    TIM_Cmd(TIM5,DISABLE);  
}  

