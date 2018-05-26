#ifndef __RTC_H
#define __RTC_H

#include "stm32f10x.h"
#include "localTime.h"

// Configue the local rtc by reftime.
void rtcConfiguration(u32 refTime);
ClockSyncState rtcCheck();
void rtcInitialize(void);
#endif 
