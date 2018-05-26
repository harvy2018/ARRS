/**
  ********************************  STM32F10x  *********************************
  * @文件名     ： i2c.c
  * @作者       ： strongerHuang
  * @库版本     ： V3.5.0
  * @文件版本   ： V1.0.0
  * @日期       ： 2016年08月18日
  * @摘要       ： I2C源文件
  ******************************************************************************/
/*----------------------------------------------------------------------------
  更新日志:
  2016-08-18 V1.0.0:初始版本
  ----------------------------------------------------------------------------*/
/* 包含的头文件 --------------------------------------------------------------*/
#include "bsp_i2c.h"
#include "uartPort.h"

/************************************************
函数名称 ： I2C_GPIO_Configuration
功    能 ： I2C引脚配置(复用开漏)
参    数 ： 无
返 回 值 ： 无
作    者 ： strongerHuang
*************************************************/
void I2C_GPIO_Configuration(void)
{
  GPIO_InitTypeDef  GPIO_InitStructure;
    
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
     
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/************************************************
函数名称 ： I2C_Configuration
功    能 ： I2C配置
参    数 ： 无
返 回 值 ： 无
作    者 ： strongerHuang
*************************************************/
void I2C_Configuration(void)
{
  I2C_InitTypeDef I2C_InitStructure;

  I2C_DeInit(I2C1);                                                  //复位I2C

  I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;                         //I2C模式
  I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;                 //占空比(快速模式时)
  I2C_InitStructure.I2C_OwnAddress1 = OWN_ADDRESS;                   //设备地址
  I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;                        //应答
  I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
  I2C_InitStructure.I2C_ClockSpeed = I2C_SPEED;                      //速度
  I2C_Init(I2C1, &I2C_InitStructure);

  I2C_ITConfig(I2C1, I2C_IT_BUF | I2C_IT_EVT, ENABLE);               //使能中断

  I2C_Cmd(I2C1, ENABLE);                                             //使能I2C
}

/************************************************
函数名称 ： I2C_Initializes
功    能 ： I2C初始化
参    数 ： 无
返 回 值 ： 无
作    者 ： strongerHuang
*************************************************/
void I2C_Initializes(void)
{
  I2C_GPIO_Configuration();
  I2C_Configuration();
}

void I2C_Master_BufferWrite(uint8_t* pBuffer, uint32_t NumByteToWrite, uint8_t SlaveAddress)
{
  /* 1.开始 */
  I2C_GenerateSTART(I2C1, ENABLE);
  while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));

  /* 2.设备地址/写 */
  I2C_Send7bitAddress(I2C1, SlaveAddress, I2C_Direction_Transmitter);
  while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

  /* 3.连续写数据 */
  while(NumByteToWrite--)
  {
    I2C_SendData(I2C1, *pBuffer);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
    pBuffer++;
  }

  /* 4.停止 */
  I2C_GenerateSTOP(I2C1, ENABLE);
  while ((I2C1->CR1&0x200) == 0x200);
}


void I2CSendBufferOut(u8 port)
{
    extern UartSBuffer	uartSBuffer[];
 
    u8 *buf;
    u16 size;
        
    
    buf=&uartSBuffer[port].data[uartSBuffer[port].rptr];
    
    if(uartSBuffer[port].rptr > uartSBuffer[port].wptr)
    {
        size=uartSBuffer[port].size-uartSBuffer[port].rptr;
        size+=uartSBuffer[port].wptr;
    }
    else if(uartSBuffer[port].rptr < uartSBuffer[port].wptr) 
    {
        size=uartSBuffer[port].wptr-uartSBuffer[port].rptr; 
    }
    else
    {
        return;
    }    
    
    I2C_ITConfig(I2C1, I2C_IT_BUF | I2C_IT_EVT, DISABLE); 
    
    I2C_Master_BufferWrite(buf,size,SLAVE_ADDRESS);
    uartSBuffer[port].rptr = uartSBuffer[port].wptr;
      
    I2C_ITConfig(I2C1, I2C_IT_BUF | I2C_IT_EVT, ENABLE);    
    
}
/**** Copyright (C)2016 strongerHuang. All Rights Reserved **** END OF FILE ****/
