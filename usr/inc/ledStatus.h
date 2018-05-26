/***************************************************************************************
File name: ledStatus.h
Date: 2016-12-6
Author: Wangwei/GuokuYan
Description:
     This file contain all the definition/status/prototype to control the led.
****************************************************************************************/
#ifndef  _LED_STATUS
#define  _LED_STATUS

typedef enum _LedState
{
    LED_STATE_BOOTUP = 0,
    LED_STATE_INIT_METER,
    LED_STATE_INIT_TAX,
    LED_STATE_RUNNING,
    LED_STATE_UPGRADE,
    LED_STATE_MAX
}LedState;


// Define the usage for LED driving port

#define LED_INDICATOR_PORT     (GPIOB) 

#define LED_TAX_COMP           (GPIO_Pin_8)        // Green          
#define LED_METER_COMP         (GPIO_Pin_15)      // Red      
#define LED_NETWORK_COMP       (GPIO_Pin_9)     //Blue

typedef enum _LedInitMeterState
{
    LED_INIT_METER_1=0,
    LED_INIT_METER_2,
    LED_INIT_METER_3,
    LED_INIT_METER_MAX
}LedInitMeterState;

typedef enum _LedInitTaxState
{
    LED_INIT_TAX_1=0,
    LED_INIT_TAX_2,
    LED_INIT_TAX_3,
    LED_INIT_TAX_MAX  
}LedInitTaxState;

typedef enum _LedRunningState
{
    LED_RUNNING_STATE_TAX_0=0,
    LED_RUNNING_STATE_TAX_1,
    LED_RUNNING_STATE_METER0,
    LED_RUNNING_STATE_METER1,
    LED_RUNNING_STATE_NETWORK_PING,
    LED_RUNNING_STATE_NETWORK_PONG,
    LED_RUNNING_STATE_MAX
}LedRunningState;

typedef enum _LedUpgradeState
{
    LED_UPGRADE_STATE_1=0,
    LED_UPGRADE_STATE_2,
    LED_UPGRADE_STATE_3,
    LED_UPGRADE_STATE_4,
    LED_UPGRADE_STATE_MAX        
}LedUpgradeState;


void    setLedState(LedState state);
LedState    getLedState();
void    ledAllOn();
void    ledAllOff();

#endif

