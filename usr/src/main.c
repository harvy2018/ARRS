/******************************************************************************************************
Function Name:								Main.c
Description:
	This the main entry funciton to sample the gasoline transactions.
Author: DXYS
Date: 2016-8-31
******************************************************************************************************/
#include "SEGGER_RTT.h"
#include "errorCode.h"
#include "uartPort.h"
#include "taxControlInterface.h"
#include "version.h"
#include "board.h"
#include "18b20.h"
#include "plcMessager.h"
#include "gasolineTransferProtocol.h"
#include "localTime.h"
#include "fwVersion.h"
#include "log.h"
#include "gasolineDataStore_Retrieve.h"
#include "flashMgr.h"
#include "ledStatus.h"
#include "meterMessager.h"
#include "bsp_rtc.h"
#include "bsp_timer.h"
#include "controlledTaxSampling.h"



extern void debugCmdHandler();

u32 count1s = 0;
u32 count100ms=0;

void main()
{
   

    // Initialize chip interrupt/GPIO/Timer,etc.
    InitSystem();

    // Initialize uart buffer manager descriptors    
    uartBufferInitialize();

    // Initialize board specific configuration, for instance, special IO usage,etc.
    initBoard();

    // Retrieve which binary(ping or pong) this program is running on
    retrievePingPongFlag();

    // Output basic infomation of the system
    PrintfLogo();

    // Check whether RTC has already synced w/ concentrator     
    if(rtcCheck()==CLOCK_LOCAL)//检查rtc是否已经同步过
	{
		rtcInitialize();
	}

    // Flash management strcuture sanity check, erase user space if necessary
    flashVersionCheck();

    // Check whether we have exception occurs before this boot and logs them if have
    exception_check();
	
    // 1st time detect the connection status of uart ports
    uartConnectionDetectBlocked(uartGetPortState());
   
        
    DetectMeter422Blocked();   //422/PLC跳线短接后，抄报模式为自由抄报模式
    if(getMeter422Mode()==0)
      DetectMeterModeBlocked();
    else
      setMeterBoardMode(FREE_MODE);
       	
    
    // Retrieve the taxcontrol device history record
    retrieveTotalHistoryRecord();

    // Initialize the tax device info flash space for the new one if have
    storeRetrieveTaxInfoIndexIntialize();

    // Bind UART port w/ tax device based on the history record.
    // taxControlDevToPortBinding(TAX_CONTROL_DEV_BINDING_REQ_TOTAL);
      
    // Change Led Indicator State
    setLedState(LED_STATE_INIT_TAX);
	 
    // Install callback funciton for upstreamer's message handling
    gasolineTrasferAppProtocolInitialize();

    // Retrieve the index of previously stored transaction records
    
    storeRetrieveIndexIntialize(MeterData);
    
    storeRetrieveIndexIntialize(TaxData);
    
    //Disable Month_Total_Sampling functionality 
    //MonthStoreRetrieveIndexIntialize();

    // Retrieve the previously configured PLC_MODULATION_MODE
    plcRetrieveModulationMode();
    
    MeterResetHigh;
    ledAllOn();

    bsp_DelayMS(4000);   //wait for plc init
    //  RetrieveMonthDataSampleFlag();	
    printf("------Enter into task loop,Init Elapse: %dms------\r\n",getSysTick()); 
    
    ledAllOff();
    bsp_StartTimer(Tick100ms,100);
           
    MeterResetLow;  
    
    while(1)
    {
        if(bsp_CheckTimer(Tick100ms))  // 100ms based tasks
        {           
            bsp_StartTimer(Tick100ms,100);
            count100ms++;
            // Try to connect w concentrator and accept app data if connected already        
            plcMessagerTask();
            // Try to parse coming event(putdown/pullup/transaction) and handle them
            //if(getMeter422Mode()==0)
            meterMessagerTask();

            // Monitoring tax control uart port's physical connection status
            uartConnectionDetect(uartGetPortCurrentState());
                                                        
            // 1000ms tick based tasks
            if(count100ms >= 10)       // 1s Based task scheduler
            {
                count100ms=0;
                count1s++;
                if(getLedState() != LED_STATE_RUNNING)
                {
                    if(count1s >= 3)
                        setLedState(LED_STATE_RUNNING);
                }

                // Upload sampled transaction record and/or heartbeat/log message to concentrator
                gasolineAppUploadingTask();
                
                taxPortDetectAndRebindCheckTask();    
                // Sampling board temperature periodically
                sampleBoardTempertureTask();

                /*
                
                if(getMeter422Mode()==1) 
                  CPCCMessagerTask();
                
                */
                
                controlledSamplingTask();
                
            }

            if(count1s >= 15)
            {
                meterBoardAliveMonitor();
                count1s = 0;
            }
            
            // Processing debug command if have
            debugCmdHandler();

            // At least, we need delay 1 ms here.
            bsp_DelayMS(1);     // Consume at least 1mS to avoid retenry this loop infinitely
        
        }
     
        
    }
}

