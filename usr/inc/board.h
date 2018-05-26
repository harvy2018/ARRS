/***************************************************************************************
File name: board.c
Date: 2016-9-7
Author: George Yan
Description:
     This file define all the interface to initialize the board
***************************************************************************************/
#ifndef _INIT_BOARD
#define  _INIT_BOARD

#define USE_I2C_FOR_METERBORAD 1

#define Concentrator_OS  1//0:WINDOWS  1:LINUX  

#define DummyData   0xFF
#define CPU_SERIAL_ID_SIZE  12

#define PLC_PWR_OFF    GPIO_SetBits(GPIOA, GPIO_Pin_12);	
#define PLC_PWR_ON     GPIO_ResetBits(GPIOA, GPIO_Pin_12);	

#define PWR15_ON      GPIO_SetBits(GPIOC, GPIO_Pin_6);	
#define PWR15_OFF     GPIO_ResetBits(GPIOC, GPIO_Pin_6);	

#define MeterModePin      GPIO_Pin_4
#define MeterModeOutPin   GPIO_Pin_7
#define MeterModePort     GPIOC


#define MeterResetPin   GPIO_Pin_5
#define MeterResetPort  GPIOC


#define MeterPort422_DetectPin   GPIO_Pin_12
#define MeterPort422_DetectPort  GPIOB

#define MeterResetLow   GPIO_ResetBits(MeterResetPort,MeterResetPin)
#define MeterResetHigh  GPIO_SetBits(MeterResetPort,MeterResetPin)

#define MeterModePinSet   GPIO_SetBits(MeterModePort,MeterModeOutPin)
#define GunModePinSet     GPIO_ResetBits(MeterModePort,MeterModeOutPin)

//#define LED_Blink   GPIO_PinReverse(LEDPort, LEDPin); 


typedef enum _SystemBootType
{
    SOFT_RST,
    WDG_RST,
    PIN_RST,
	POR_RST,
    SOFT_RST_MAX
	
}SystemBootType;

void InitSystem();
void initBoard();
unsigned char *getBoardSerialID();
void retrievePingPongFlag();
unsigned char getPingPongFlag();
SystemBootType GetSystemBootType();
void DetectMeterMode(void);
void DetectMeterModeBlocked(void);
void Erase_FwArea(void);
u8 IAP_TO_ExFlash(u8 *pdataBuf,u32 flashdestination,u16 dlen);
u8 ExFlash_TO_InterFlash(u32 FwSize);
void DetectMeter422Blocked(void);

#endif
