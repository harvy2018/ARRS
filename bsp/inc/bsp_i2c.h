/**
  ********************************  STM32F10x  *********************************
  * @文件名     ： i2c.h
  * @作者       ： strongerHuang
  * @库版本     ： V3.5.0
  * @文件版本   ： V1.0.0
  * @日期       ： 2016年08月18日
  * @摘要       ： I2C头文件
  ******************************************************************************/

/* 定义防止递归包含 ----------------------------------------------------------*/
#ifndef _I2C_H
#define _I2C_H

/* 包含的头文件 --------------------------------------------------------------*/
#include "stm32f10x.h"


/* 宏定义 --------------------------------------------------------------------*/
#define OWN_ADDRESS               0x28                     //设备自身地址
#define SLAVE_ADDRESS             0x30                     //从机地址

#define I2C_SPEED                 20000                   //I2C速度

/* I2C SPE mask */
//#define CR1_PE_Set                ((uint16_t)0x0001)
//#define CR1_PE_Reset              ((uint16_t)0xFFFE)

///* I2C START mask */
//#define CR1_START_Set             ((uint16_t)0x0100)
//#define CR1_START_Reset           ((uint16_t)0xFEFF)

//#define CR1_POS_Set               ((uint16_t)0x0800)
//#define CR1_POS_Reset             ((uint16_t)0xF7FF)

///* I2C STOP mask */
//#define CR1_STOP_Set              ((uint16_t)0x0200)
//#define CR1_STOP_Reset            ((uint16_t)0xFDFF)

///* I2C ACK mask */
//#define CR1_ACK_Set               ((uint16_t)0x0400)
//#define CR1_ACK_Reset             ((uint16_t)0xFBFF)

///* I2C ENARP mask */
//#define CR1_ENARP_Set             ((uint16_t)0x0010)
//#define CR1_ENARP_Reset           ((uint16_t)0xFFEF)

///* I2C NOSTRETCH mask */
//#define CR1_NOSTRETCH_Set         ((uint16_t)0x0080)
//#define CR1_NOSTRETCH_Reset       ((uint16_t)0xFF7F)

///* I2C registers Masks */
//#define CR1_CLEAR_Mask            ((uint16_t)0xFBF5)

///* I2C DMAEN mask */
//#define CR2_DMAEN_Set             ((uint16_t)0x0800)
//#define CR2_DMAEN_Reset           ((uint16_t)0xF7FF)

///* I2C LAST mask */
//#define CR2_LAST_Set              ((uint16_t)0x1000)
//#define CR2_LAST_Reset            ((uint16_t)0xEFFF)

///* I2C FREQ mask */
//#define CR2_FREQ_Reset            ((uint16_t)0xFFC0)

///* I2C ADD0 mask */
//#define OAR1_ADD0_Set             ((uint16_t)0x0001)
//#define OAR1_ADD0_Reset           ((uint16_t)0xFFFE)

///* I2C ENDUAL mask */
//#define OAR2_ENDUAL_Set           ((uint16_t)0x0001)
//#define OAR2_ENDUAL_Reset         ((uint16_t)0xFFFE)

///* I2C ADD2 mask */
//#define OAR2_ADD2_Reset           ((uint16_t)0xFF01)

///* I2C F/S mask */
//#define CCR_FS_Set                ((uint16_t)0x8000)

///* I2C CCR mask */
//#define CCR_CCR_Set               ((uint16_t)0x0FFF)

///* I2C FLAG mask */
//#define FLAG_Mask                 ((uint32_t)0x00FFFFFF)

///* I2C Interrupt Enable mask */
//#define ITEN_Mask                 ((uint32_t)0x07000000)

//#define I2C_IT_BUF                ((uint16_t)0x0400)
//#define I2C_IT_EVT                ((uint16_t)0x0200)
//#define I2C_IT_ERR                ((uint16_t)0x0100)

/* 函数申明 ------------------------------------------------------------------*/
void I2C_Initializes(void);
void I2C_Master_BufferWrite(uint8_t* pBuffer, uint32_t NumByteToWrite, uint8_t SlaveAddress);
void I2CSendBufferOut(u8 port);

#endif /* _I2C_H */

/**** Copyright (C)2016 strongerHuang. All Rights Reserved **** END OF FILE ****/
