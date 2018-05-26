/***************************************************************************************
File name: ledStatus.c
Date: 2016-12-6
Author: Wangwei/GuokuYan
Description:
     This file contain all the interface to control the led which reflect the system operating staus.
****************************************************************************************/     
#include "stm32f10x.h"
#include "ledStatus.h"
#include "taxControlInterface.h"
#include "plcMessager.h"
#include "meterMessager.h"
#include "controlledTaxSampling.h"

#define  WORK_FINE_SECOND_DURATION      60

LedState    ledMainState=LED_STATE_BOOTUP;
LedInitMeterState  ledMeterState=LED_INIT_METER_1;
LedInitTaxState     ledTaxState=LED_INIT_TAX_1;
LedRunningState   ledRunningState = LED_RUNNING_STATE_TAX_0;
LedUpgradeState  ledUpgradeState = LED_UPGRADE_STATE_1;
unsigned char ledvalue[LED_STATE_MAX]={0};      // 0  all the led defualt to "turn off" for all the state(main or sub state)

static void    ledIndicatorOnMeterInit(LedInitMeterState state)
{
    switch  (state)
	{
		case    LED_INIT_METER_1:
		case    LED_INIT_METER_2:
		case    LED_INIT_METER_3:
			{
				;// do nothing at this time now
			}
		break;    
		default:
		break;    
	}
}

static void    ledIndicatorOnTaxInit(LedInitTaxState state)
{
    TaxControlDevManager *pTaxDevMgr = getTaxControlDevManager();
    
    switch  (state)
        {
            case    LED_INIT_TAX_1:
			{
				if(pTaxDevMgr->taxControlDevInfo[0].bind == TAX_CONTROL_DEVICE_BOUND ||
				   pTaxDevMgr->taxControlDevInfo[1].bind == TAX_CONTROL_DEVICE_BOUND )
				{
						GPIO_SetBits(LED_INDICATOR_PORT, LED_TAX_COMP);		
				}
				else
				{
						GPIO_ResetBits(LED_INDICATOR_PORT,LED_TAX_COMP);
				}
			}
            break;

            case   LED_INIT_TAX_2:
			{
				if(pTaxDevMgr->taxControlDevInfo[0].bind == TAX_CONTROL_DEVICE_BOUND &&
				   pTaxDevMgr->taxControlDevInfo[1].bind == TAX_CONTROL_DEVICE_BOUND )
				{
						GPIO_SetBits(LED_INDICATOR_PORT, LED_TAX_COMP);
				}
				else
				{
						GPIO_ResetBits(LED_INDICATOR_PORT,LED_TAX_COMP);                    
				}

			}
            break;

            default:
            break;    
        }
}

static void   ledIndicatorOnRunning(LedRunningState state)
{
    TaxControlDevManager *pTaxDevMgr = getTaxControlDevManager();
    
    switch  (state)
        {
            case  LED_RUNNING_STATE_TAX_0:
                {
                    GPIO_ResetBits(LED_INDICATOR_PORT,LED_METER_COMP);
                    GPIO_ResetBits(LED_INDICATOR_PORT,LED_NETWORK_COMP);
                    if(pTaxDevMgr->taxControlDevInfo[0].bind == TAX_CONTROL_DEVICE_BOUND)
                        {
                            GPIO_SetBits(LED_INDICATOR_PORT, LED_TAX_COMP);
                        }
                    else
                        {
                            GPIO_ResetBits(LED_INDICATOR_PORT,LED_TAX_COMP);
                        }                        
			}
            break;

            case  LED_RUNNING_STATE_TAX_1:
		    {
		        if(pTaxDevMgr->taxControlDevInfo[1].bind == TAX_CONTROL_DEVICE_BOUND)
			    {
			        GPIO_SetBits(LED_INDICATOR_PORT, LED_TAX_COMP);
                        }
			else
                        {
                            GPIO_ResetBits(LED_INDICATOR_PORT,LED_TAX_COMP);
                        }                        
			}
            break;

            case  LED_RUNNING_STATE_METER0:
                {
                    GPIO_ResetBits(LED_INDICATOR_PORT,LED_TAX_COMP);
                    GPIO_ResetBits(LED_INDICATOR_PORT,LED_NETWORK_COMP);
                    
                    if(getMeterState(0)==METER_STATE_VALID)
                        {
                            GPIO_SetBits(LED_INDICATOR_PORT, LED_METER_COMP);
                        }
                    else
                        {
                            GPIO_ResetBits(LED_INDICATOR_PORT,LED_METER_COMP);
                        }
                }
            break;

            case  LED_RUNNING_STATE_METER1:
                {
                   if(getMeterState(1)==METER_STATE_VALID)
                       {
                            GPIO_SetBits(LED_INDICATOR_PORT, LED_METER_COMP);
                        }
                   else
                        {
                            GPIO_ResetBits(LED_INDICATOR_PORT,LED_METER_COMP);
                        }
                }    
            break;

            case  LED_RUNNING_STATE_NETWORK_PING:
            case  LED_RUNNING_STATE_NETWORK_PONG:             
                {
                    GPIO_ResetBits(LED_INDICATOR_PORT,LED_TAX_COMP);
                    GPIO_ResetBits(LED_INDICATOR_PORT,LED_METER_COMP); 
                
                    if(plcMessagerGetChannelState() == PLC_MSG_STATE_NETWORK)
                        {
                            GPIO_SetBits(LED_INDICATOR_PORT, LED_NETWORK_COMP);
                        }
                    else
                        {
                            GPIO_ResetBits(LED_INDICATOR_PORT,LED_NETWORK_COMP);
                        }
                }
            break;
            
            default:
            break;    
        }    
}

static void  ledIndicatorOnUpgrading(LedUpgradeState state)
{
    ;
}


void    ledIndicatorHandler()
{
    switch  (ledMainState)
        {
            case   LED_STATE_BOOTUP:
                {
                    static unsigned char ppflag=0;
                    if(ppflag)
                        {
                            ledAllOn();
                            ppflag = 0;
                        }
                    else
                        {
                            ledAllOff();
                            ppflag = 1;
                        }
                }    
            break;    
            case   LED_STATE_INIT_METER:
                {
                    ledIndicatorOnMeterInit(ledMeterState);
                    ledMeterState++;
                    if(ledMeterState >= LED_INIT_METER_MAX)
                        ledMeterState = 0;                    
                }   
            break;

            case   LED_STATE_INIT_TAX:
                {
                    ledIndicatorOnTaxInit(ledTaxState);
                    ledTaxState++;
                    if(ledTaxState >= LED_STATE_MAX)
                        ledTaxState=0;
                }
            break;

            case   LED_STATE_RUNNING:
                {
                     ledIndicatorOnRunning(ledRunningState);
                     ledRunningState++;
                     if(ledRunningState >= LED_RUNNING_STATE_MAX)
                        ledRunningState=0;
                }
            break;

            case   LED_STATE_UPGRADE:
                {
                    ledIndicatorOnUpgrading(ledUpgradeState);
                    ledUpgradeState++;
                    if(ledUpgradeState >= LED_UPGRADE_STATE_MAX)
                       ledUpgradeState = 0; 
                }
            break;

            default:
            break;    
        }
}

void    setLedState(LedState state)
{
    ledMainState = state;
    ledAllOff();
}

LedState    getLedState()
{
    return  ledMainState;
}

void    ledAllOn()
{
      GPIO_SetBits(LED_INDICATOR_PORT, LED_TAX_COMP);
      GPIO_SetBits(LED_INDICATOR_PORT, LED_METER_COMP);
      GPIO_SetBits(LED_INDICATOR_PORT, LED_NETWORK_COMP);    
}

void   ledAllOff()
{
      GPIO_ResetBits(LED_INDICATOR_PORT,LED_TAX_COMP);
      GPIO_ResetBits(LED_INDICATOR_PORT,LED_METER_COMP);
      GPIO_ResetBits(LED_INDICATOR_PORT,LED_NETWORK_COMP); 
}
