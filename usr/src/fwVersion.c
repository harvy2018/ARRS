/***************************************************************************************
File name: fwVersion.c
Date: 2016-10-24
Author: GuokuYan
Description:
     This file contain all the firmware version infomation.
     
Definition:
    Define firmware version of this program, definition as below:
    format:  x.y.z.p
        x:   Increment by 1 when framework changes
        y:   Increment by 1 when feature changes
        z:   Increment by 1 when bug fix
        p:   PING/PONG flag,  PING:0  PONG:1
**************************************************************************************/
/*************************************************************************************
Version History:

    3.6.2.0: Modification£º
        1. Fix the bug when temperature dropped to below zero,the temperature sample function got an incorrect result. 
        
    3.6.1.0: NEW FEATURE
        1. Add ARRS board and Nozzle board factory test mode. 

    3.6.0.0: Big change
        1.  This version is suited for new hardware which MeterBoard or GunBoard communicate with ARRS through i2c port.
        2.  In this version meterdata do not drive ARRS to read taxport any more.Meterdata and taxdata are uploaded respectively.
            And taxport read once every 6 hours,taxdata is a accumulate data in this period.
        3.  Modify MeterLED(Red LED) indicate way same as TaxPort indicate way.        
    
    3.5.0.0:  Big change
        1.  This version integrates meter control and gun signal control in one code.
        2.  Modify communication protocol between ARRS board and PLC board in concentrator side.
        3.  Modify data struct of tax transaction data,optimize transfer efficient.
        4.  Modify data struct log,optimize transfer efficient.
        5.  Old data struct and old log struct are all can be configed through macro definition "Concentrator_OS".
        6.  Fix 6 hours time synchronization bug.
          
    3.4.0.0:  Big change
        1.  From this version, tax control sample will be controled by either gun_putdown/pullup signal or meter
             message for ever, not support free sampling any more.
        2.  The initial version plan to support gun_putdowb/pullup signal, and also take the framework for the coming
             meter message control, however, this may break the 3.3.3.0's primitve meter_message_control functionality, 
             will fix it in the next version.
        3.  The meter message protocol need also enhance,  add "mode" to seperate "gun_putdown/pullup" and "meter message"
             control.
        4.  Optimize the internal tax control sampling logic to meet the new changes. encapsulate the APIs to only one which only
             read designated gun one time.
        5.  Flash version upgrade. 
             
    3.3.3.0:  New feature:
		1.Add self test mode.Debug port send #CCCCC cmd enter self test mode.
			  
    3.3.2.0£º Modification£º
	       1.Reset exflash wptr when get upgrade notice.

    3.3.1.0:  New feature:
        1.     Add reset type in log;
        2.     Increase oiltranscation data storage capacity to 64KB.
        3.     Change RTC synchronization times from power on to every 6 hours.
        4.     Change Monthdata query to unblocking mode.
        5.     Optimize spiflash write program.
        6.     Change firmware storage capacity to 192KB.
        7.     Add Remote erase spiflash function.
        8. 	   Increase PLC power on and power off delay time to 5 seconds.
        9. 	   Device will restart when PLC continuously reset reach 20 times(about 8 minutes).
        10.    Optimize spiflash write program,add erase sector and rewrite function when write fail.
        11.    (F7)storeTotalHistoryRecord: reserve one TotalHistoryRecord store spece to avoid F7 fault.
        12.    Fix retrieveTotalHistoryRecord bug.reserve last 64Byte section.Add retrieveTotalHistoryRecord check err in log.
        13.    Add PLC Getsysteminfo retrycnt to 5 times.
        14.    Add Hardware exception info dump;
    3.3.0.0:  New feature:
	
              1. Implement the "meter messaged controlled tax control sampling" feature;
              2. Enchance the RTC feature since we have new HW that have battery installed, then can conserve
                  the synced time.
    3.2.0.0:  New feature: Led indicators implementation.
              Modification£º
              1. Revise PLC power control logic to suit hardward v0.4.
              2. RRevise COM connection detect IO to suit hardward v0.4.
              3. Disable UART when power off PLC module and reenable it to avoid electricty leakage causing
                  PLC not shutdown completely.
              4. Fix a bug which may cause cc checking fails on officially initialized gasoline machine        
    3.1.0.0:  Base code of new Arch framework
**************************************************************************************/
unsigned char fw_ver[4]={3,6,3,0};


unsigned char *getFwVersion()
{
    return fw_ver;
}


