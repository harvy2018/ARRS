/**
  ********************************  STM32F10x  *********************************
  * @�ļ���     �� i2c.c
  * @����       �� strongerHuang
  * @��汾     �� V3.5.0
  * @�ļ��汾   �� V1.0.0
  * @����       �� 2016��08��18��
  * @ժҪ       �� I2CԴ�ļ�
  ******************************************************************************/
/*----------------------------------------------------------------------------
  ������־:
  2016-08-18 V1.0.0:��ʼ�汾
  ----------------------------------------------------------------------------*/
/* ������ͷ�ļ� --------------------------------------------------------------*/
#include "bsp_i2c.h"
#include "uartPort.h"

/************************************************
�������� �� I2C_GPIO_Configuration
��    �� �� I2C��������(���ÿ�©)
��    �� �� ��
�� �� ֵ �� ��
��    �� �� strongerHuang
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
�������� �� I2C_Configuration
��    �� �� I2C����
��    �� �� ��
�� �� ֵ �� ��
��    �� �� strongerHuang
*************************************************/
void I2C_Configuration(void)
{
  I2C_InitTypeDef I2C_InitStructure;

  I2C_DeInit(I2C1);                                                  //��λI2C

  I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;                         //I2Cģʽ
  I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;                 //ռ�ձ�(����ģʽʱ)
  I2C_InitStructure.I2C_OwnAddress1 = OWN_ADDRESS;                   //�豸��ַ
  I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;                        //Ӧ��
  I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
  I2C_InitStructure.I2C_ClockSpeed = I2C_SPEED;                      //�ٶ�
  I2C_Init(I2C1, &I2C_InitStructure);

  I2C_ITConfig(I2C1, I2C_IT_BUF | I2C_IT_EVT, ENABLE);               //ʹ���ж�

  I2C_Cmd(I2C1, ENABLE);                                             //ʹ��I2C
}

/************************************************
�������� �� I2C_Initializes
��    �� �� I2C��ʼ��
��    �� �� ��
�� �� ֵ �� ��
��    �� �� strongerHuang
*************************************************/
void I2C_Initializes(void)
{
  I2C_GPIO_Configuration();
  I2C_Configuration();
}

void I2C_Master_BufferWrite(uint8_t* pBuffer, uint32_t NumByteToWrite, uint8_t SlaveAddress)
{
  /* 1.��ʼ */
  I2C_GenerateSTART(I2C1, ENABLE);
  while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));

  /* 2.�豸��ַ/д */
  I2C_Send7bitAddress(I2C1, SlaveAddress, I2C_Direction_Transmitter);
  while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

  /* 3.����д���� */
  while(NumByteToWrite--)
  {
    I2C_SendData(I2C1, *pBuffer);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
    pBuffer++;
  }

  /* 4.ֹͣ */
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
