/*
*********************************************************************************************************
*	                                  
*	模块名称 : 定时器模块
*	文件名称 : bsp_timer.h
*	版    本 : V2.0
*	说    明 : 头文件
*
*	Copyright (C), 2010-2011, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_TIMER_H
#define __BSP_TIMER_H

#include "stm32f10x.h"

/* 目前是空操作，用户可以定义让CPU进入IDLE状态的函数和喂狗函数 */
#define CPU_IDLE()

typedef enum 
{   
    Tick100ms=1,
    Tick1s=2,
    Tick10s=3,
    
}Tick_TYPE;

 
/* 定时器结构体，成员变量必须是 volatile, 否则C编译器优化时可能有问题 */
typedef struct
{
	volatile uint32_t count;	/* 计数器 */
	volatile uint8_t flag;		/* 定时到达标志  */
}SOFT_TMR;



typedef enum 
{   CALC_TYPE_S = 0,
    CALC_TYPE_MS,
    CALC_TYPE_US
 } CALC_TYPE;
/* 供外部调用的函数声明 */
void bsp_InitTimer(void);
void bsp_DelayMS(uint32_t n);
void bsp_StartTimer(uint8_t _id, uint32_t _period);
uint8_t bsp_CheckTimer(uint8_t _id);
int32_t bsp_GetRunTime(void);

void TIM_Init(TIM_TypeDef* TIMx,u32 mS);
void uDelay(unsigned int usec);

void TIM5_Init(CALC_TYPE type) ;
void TIM5_S_CALC(uint32_t s) ;
void TIM5_MS_CALC(uint32_t ms);
void TIM5_US_CALC(uint32_t us);

int32_t  getSysTick();

#endif
