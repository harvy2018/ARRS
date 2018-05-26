
#include <stdio.h>	
#include <time.h>
#include "bsp_rtc.h"
#include "localTime.h"
#include "SEGGER_RTT.h"

#define dbgPrintf   SEGGER_RTT_printf


void rtcInitialize(void)
{
    /* Enable PWR and BKP clocks */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);

    /* Allow access to BKP Domain */
    PWR_BackupAccessCmd(ENABLE);

    /* Reset Backup Domain */
    BKP_DeInit();

    /* Enable LSE */
    RCC_LSEConfig(RCC_LSE_ON);
    
    /* Wait till LSE is ready */
    while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET)
        {
            ;
        }

    /* Select LSE as RTC Clock Source */
    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);

    /* Enable RTC Clock */
    RCC_RTCCLKCmd(ENABLE);

    /* Wait for RTC registers synchronization */
    RTC_WaitForSynchro();
	
    RTC_WaitForLastTask();	
		  /* Enable the RTC Second */
    RTC_ITConfig(RTC_IT_SEC, ENABLE);

    /* Wait until last write operation on RTC registers has finished */
    RTC_WaitForLastTask();


    /* Set RTC prescaler: set RTC period to 1sec */
    RTC_SetPrescaler(32767); /* RTC period = RTCCLK/RTC_PR = (32.768 KHz)/(32767+1) */

    /* Wait until last write operation on RTC registers has finished */
    RTC_WaitForLastTask();
		
}

static void timeAdjust(u32 refTime)
{	
	  /* Wait until last write operation on RTC registers has finished */
  RTC_WaitForLastTask();
  /* Change the current time */
  RTC_SetCounter(refTime);
  /* Wait until last write operation on RTC registers has finished */
  RTC_WaitForLastTask();
}


void rtcConfiguration(u32 refTime)
{
  
    rtcInitialize();
	timeAdjust(refTime);
	BKP_WriteBackupRegister(BKP_DR1, 0xA5A5);
	
}


ClockSyncState rtcCheck()
{
    if(BKP_ReadBackupRegister(BKP_DR1) == 0xA5A5)
    {
      
         RTC_ITConfig(RTC_IT_SEC, ENABLE);
		 ClockStateSet(CLOCK_GTC);        
		 printf("rtcCheck: CLOCK_GTC\r\n");
		 return CLOCK_GTC;
    }
	else
	{
        ClockStateSet(CLOCK_LOCAL);
		printf("rtcCheck: CLOCK_LOCAL\r\n");
		return CLOCK_LOCAL;
	}
}


