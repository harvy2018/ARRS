/***************************************************************************************
File name: log.c
Date: 2016-10-25
Author: Guoku Yan
Description:
     This file intends to buffer the important log and statistic info and upload to upstreamer.
***************************************************************************************/
#include <stdlib.h>
#include <string.h> 
#include "time.h"
#include "log.h"
#include "flashMgr.h"
#include "board.h"
#include "plcMessager.h"
#include "18b20.h"
#include "uartPort.h"
#include "localTime.h"
#include "fwVersion.h"
#include "taxControlInterface.h"
#include "meterMessager.h"
#include "controlledTaxSampling.h"

 


unsigned char  logBuf[REMOTE_LOG_SIZE];
u16 LogDataLEN=0;
u32 runtime=0;


/**************************************************************************************
Function name: collectLogInfo
Description:
    This funciton collect the important log information to static inernal buffer.
Parameter:
    NULL
Return:
    E_SUCCESS 
**************************************************************************************/



static errorCode  collectLogInfo2()
{
    int size=0;
    unsigned char *pChar;
    unsigned int  *pWord;
    UartPortState *pUartPortState;
    FlashSectionStruct *pFS;
    u8 tbuf_size=255;	
    unsigned char t[tbuf_size];
	
    int i,j;
    struct tm *time;
   // int sInt,sDec;
    float snr_t;
    unsigned char tSym,tInt;
    unsigned short tDec;
    unsigned int  secTick;
    extern u8 TotalHistoryReocrd_CCErr;
    extern u8 TransctionReocrd_CCErr;
    extern u8 TaxMonthDataReocrd_CCErr;
    extern u8 exception_flag;
    extern EXCEPTION_INFO exception_info;

    extern unsigned char  meterBoardMode;
    extern unsigned char  meterProtocolVer;
    
    TaxControlDevManager *pTaxControlDevMgr = getTaxControlDevManager();
    FLASHOperationErrorStatistic *pflashErrorStatisc = getFlashErrorStatistic();
    MeterTransactionDataGroup* pMeterDataGroup=getMeterDataGroup();
    ControlPortSetMgr *pControlPortSetMgr = getControlPortSetMgr();   

	GunStatusManager *pGunStatusMgr = getGunStatusManager();
    // 1st clear the buffer
    memset(logBuf,0, REMOTE_LOG_SIZE);

    // Release date and Revision
    pChar=getFwVersion();
    sprintf(logBuf,"\r\n*FW Ver %s %s Rev:%d.%d.%d.%d\r\n", __DATE__,__TIME__,pChar[0],pChar[1],pChar[2],pChar[3]);						 				 

    // UART port Connecting/Binding state
    pUartPortState = uartGetPortState();
    memset(t,0,tbuf_size);
    sprintf(t, "*U0 St C:%d B:%d U1 St C:%d B:%d\r\n",pUartPortState[0].cState,pUartPortState[0].bState,pUartPortState[1].cState,pUartPortState[1].bState);
    strcat(logBuf,t);    

  	 
    memset(t,0,tbuf_size);
    switch(GetSystemBootType())
    {
        case SOFT_RST:
            {
                sprintf(t,"*BootType:SOFT  ");
                strcat(logBuf,t);
            }
        break;

        case WDG_RST:
            {
                sprintf(t,"*BootType:WDG  ");
                strcat(logBuf,t);
            }
        break;
        
	case PIN_RST:
            {
                sprintf(t,"*BootType:PIN  ");
                strcat(logBuf,t);
            }
        break;
        
	 case POR_RST:
            {
                sprintf(t,"*BootType:POR  ");
                strcat(logBuf,t);
            }
        break;

        defult:
            {
                sprintf(t,"*BootType:undefined  ");
                strcat(logBuf,t);
            }
	 break;
		
    }
	
	 // PingPong working space selector
    extern u32 PLCResetCnt;
    memset(t,0,tbuf_size);
    if(getPingPongFlag())
    {
        sprintf(t,"PING  PLC RST:%d\r\n",PLCResetCnt);
        strcat(logBuf,t);
    }
    else
    {
        sprintf(t,"PONG  PLC RST:%d\r\n",PLCResetCnt);
        strcat(logBuf,t);
    }
	
	
	
	if(pMeterDataGroup[0].MeterTraDataGroup->DataCnt>0)
	{
	     memset(t,0,tbuf_size);
            sprintf(t,"*TPort0 accumulated %d meterdata,hasn't splitted!\r\n",pMeterDataGroup[0].MeterTraDataGroup->DataCnt);
            strcat(logBuf,t);
	}
	
	
	if(pMeterDataGroup[1].MeterTraDataGroup->DataCnt>0)
	{
	     memset(t,0,tbuf_size);
            sprintf(t,"*TPort1 accumulated %d meterdata,hasn't splitted!\r\n",pMeterDataGroup[1].MeterTraDataGroup->DataCnt);
            strcat(logBuf,t);
	}
		
    // Un-uploaded reocord number
    memset(t,0,tbuf_size);
    pFS = uf_RetrieveFlashSectionInfo(FLASH_SECTION_GASOLINE_DATA);
    if(pFS->wptr >= pFS->rptr)
		{
				i=pFS->wptr - pFS->rptr;
		}
    else
		{
				i = pFS->wptr+pFS->size - pFS->rptr;
		}

    i= i/64;
    sprintf(t,"*Unupload:%d Wptr=%x rptr=%x end=%x\r\n",i,pFS->wptr,pFS->rptr, pFS->base+pFS->size);
    strcat(logBuf,t);
    
        
    // PANID and TX Mode runtime
    memset(t,0,tbuf_size);
    sprintf(t,"*PID:%d  TMode:%d  ",PANID,plcGetModulationMode());
    strcat(logBuf,t);
		
	/**runtime**/	
    memset(t,0,tbuf_size);
    sprintf(t,"Runtime:%dmin\r\n",runtime/60);
    strcat(logBuf,t);

		
		
    // Date-Time-HWID-SNR-Temperature
    secTick = getLocalTick()+LOCAL_TIME_ZONE_SECOND_OFFSET;
    time= localtime((time_t *)&secTick);
    memset(t,0,tbuf_size);
    sprintf(t,"*Date %4d-%02d-%02d %02d:%02d:%02d ",time->tm_year+1900,time->tm_mon+1,time->tm_mday,
					                                          time->tm_hour,time->tm_min,time->tm_sec);
    strcat(logBuf,t);

    memset(t,0,tbuf_size);
    pWord= (unsigned int *)getBoardSerialID();
    sprintf(t,"HWID:%08X %08X %08X ",__REV(pWord[0]),__REV(pWord[1]),__REV(pWord[2]));	
    strcat(logBuf,t);

    memset(t,0,tbuf_size);
	plcGetLatestSNR(&snr_t);
    sprintf(t,"SNR:%.02fdB ",snr_t);
   // sprintf(t,"SNR:%02d.%02ddB ",sInt,sDec);

    strcat(logBuf,t);
    
    memset(t,0,tbuf_size);
    getBoardTemperature(&tSym, &tInt, &tDec);

    if(tSym==0)
        {
            sprintf(t,"T:%d.%d\r\n",tInt,tDec);
        }
    else
        {
            sprintf(t,"T:-%d.%d\r\n",tInt,tDec);
        }
    strcat(logBuf,t);


    // TaxControl Statistic
    for(i=0;i<TAX_CONTROL_DEVICE_MAX_NUM;i++)
	{
		if(pTaxControlDevMgr->taxControlDevInfo[i].bind == TAX_CONTROL_DEVICE_BOUND)
			{
				for(j=0;j<pTaxControlDevMgr->taxControlDevInfo[i].gunTotal;j++)
					{
						memset(t,0,tbuf_size);
						sprintf(t,"*PORT:%d Gun:%d Try:%08X Got:%08X Pick:%08X\r\n",
							pTaxControlDevMgr->taxControlDevInfo[i].port,j,
							pTaxControlDevMgr->taxControlDevInfo[i].statistic[j].totalTryCount,
							pTaxControlDevMgr->taxControlDevInfo[i].statistic[j].successCount,
							pGunStatusMgr[i].GunPickCnt[j]);
						
						strcat(logBuf,t);
					}                    
			}
	}

    // Gun statistic  pControlPortSetMgr->port[i].gPxTotal
    if(pControlPortSetMgr->valid)
        {
            memset(t,0,64);
            if(meterBoardMode)
            {
                sprintf(t,"Meter Board PVer:%d\r\n",meterProtocolVer);
                for(i=0;i<TAX_CONTROL_DEVICE_MAX_NUM;i++)
                {
                    if(pControlPortSetMgr->port[i].valid)
                    {
                      
                        memset(t,0,64);
                        
                        sprintf(t,"PORT:%d,TotalGun:%d,MappedGunNum:%d\r\n",i,pControlPortSetMgr->port[i].gPxTotal,pControlPortSetMgr->port[i].gPxMappedNum);
                        strcat(logBuf,t); 
                        
                        for(int g=0;g<MAXIUM_GPX_PER_PORT;g++)
                        {
                            memset(t,0,64);
                            if(pControlPortSetMgr->port[i].gPx[g].taxGun!=UNMAPPED_TAXGUNNO)
                            {
                                sprintf(t,"MGn%d->TGn:%d\r\n",pControlPortSetMgr->port[i].gPx[g].meterGun,pControlPortSetMgr->port[i].gPx[g].taxGun);
                                strcat(logBuf,t);
                            }
                            
                        }                            
                        
                    }
                    
                }
                
            }
            else
            {
                sprintf(t,"Gun Board PVer:%d\r\n",meterProtocolVer);
                   strcat(logBuf,t);
            
                for(i=0;i<TAX_CONTROL_DEVICE_MAX_NUM;i++)
                {
                    if(pControlPortSetMgr->port[i].valid)
                    {
                        for(int g=0;g<pControlPortSetMgr->port[i].gPxTotal;g++)
                        {
                            memset(t,0,64);
                            if(i==0)
                            {
                                sprintf(t,"Gp%d->TaxGun:%d  PPC:%d\r\n",g,pControlPortSetMgr->port[i].gPx[g].taxGun,pControlPortSetMgr->port[i].gPx[g].ppCount);
                                strcat(logBuf,t);
                            }
                            else
                            {
                                sprintf(t,"Gp%d->TaxGun:%d  PPC:%d\r\n",g+pControlPortSetMgr->delimiter,pControlPortSetMgr->port[i].gPx[g].taxGun,pControlPortSetMgr->port[i].gPx[g].ppCount);
                                strcat(logBuf,t);                                    
                            }
                        }
                    }
                }
                
                
            }
                
         
        }

    
    // Flash error statistic
    for(i=0;i<FLASH_MAX;i++)
	{
		if(pflashErrorStatisc[i].eraseCount!=0 || pflashErrorStatisc[i].writeCount!=0)
		{
			memset(t,0,tbuf_size);
			sprintf(t, "*FLASH%d:EE:%08X WE:%08X\r\n",i,pflashErrorStatisc[i].eraseCount,pflashErrorStatisc[i].writeCount);
			strcat(logBuf,t);
			if(pflashErrorStatisc[i].writeCount!=0)
			{
				memset(t,0,tbuf_size);
				sprintf(t, "*FLASH%d:WErrAddr:%08X\r\n",i,pflashErrorStatisc[i].errWaddr);
				strcat(logBuf,t);
			}
		}
	}

		
	if(TotalHistoryReocrd_CCErr!=0)
	{
		memset(t,0,tbuf_size);
		sprintf(t,"*TotalHistoryReocrd_CCErr=%d\r\n",TotalHistoryReocrd_CCErr);
		strcat(logBuf,t); 
	}
	if(TransctionReocrd_CCErr!=0)
	{
		memset(t,0,tbuf_size);
		sprintf(t,"*TransctionReocrd_CCErr=%d\r\n",TransctionReocrd_CCErr);
		strcat(logBuf,t); 
	}
	if(TaxMonthDataReocrd_CCErr!=0)
	{
		memset(t,0,tbuf_size);
		sprintf(t,"*TaxMonthDataReocrd_CCErr=%d\r\n",TaxMonthDataReocrd_CCErr);
		strcat(logBuf,t); 
	}
    
	
	if(exception_flag)
	{
		memset(t,0,tbuf_size);
		sprintf(t,"*r0=0x%08X,r1=0x%08X,r2=0x%08X,r3=0x%08X,r12=0x%08X\r\n",exception_info.stacked_r0,exception_info.stacked_r1,exception_info.stacked_r2,exception_info.stacked_r3,exception_info.stacked_r12);
		strcat(logBuf,t); 
		memset(t,0,tbuf_size);
		sprintf(t,"*LR=0x%08X,PC=0x%08X,PSR=0x%08X,T=0x%08X\r\n",exception_info.stacked_lr,exception_info.stacked_pc,exception_info.stacked_psr,exception_info.timestamp);		
		strcat(logBuf,t); 
		
	}
	
	
	
    memset(t,0,tbuf_size);
    sprintf(t,"*Log size ~=%d\r\n",strlen(logBuf)+20);
    strcat(logBuf,t); 

 //   printf(logBuf);    
    PrintBuf(logBuf); 	
    
}



static errorCode  collectLogInfo()
{
       
   
    unsigned int  *pWord;   
    FlashSectionStruct *pFS;
   	
    u8 i,j,k,g;
    u32 cnt;
  
    float snr_t;
    unsigned char tSym,tInt;
    unsigned short tDec;
    unsigned int  secTick;
    extern u8 TotalHistoryReocrd_CCErr;
    extern u8 TransctionReocrd_CCErr;
    extern u8 TaxMonthDataReocrd_CCErr;
    extern u8 exception_flag;
    extern u32 PLCResetCnt;
    extern u32 OTD_Upload_Cnt;
    extern EXCEPTION_INFO exception_info;
    extern unsigned char  meterBoardMode;
    extern unsigned char  meterProtocolVer;
    extern u8 MeterResetCnt;
    
    unsigned char *pChar=getFwVersion();
    TaxControlDevManager *pTaxControlDevMgr = getTaxControlDevManager();
    FLASHOperationErrorStatistic *pflashErrorStatisc = getFlashErrorStatistic();
    MeterTransactionDataGroup* pMeterDataGroup=getMeterDataGroup();
    ControlPortSetMgr *pControlPortSetMgr = getControlPortSetMgr();   
	GunStatusManager *pGunStatusMgr = getGunStatusManager();  
    UartPortState *pUartPortState = uartGetPortState();
    

    LogContent  LogData;
    memset((u8*)&LogData,0,sizeof(LogContent));
    
    u8 BindPortCnt=0;
    u8 PhyConnPort=0;
   
    LogDataLEN=0;
    
    LogData.Fw_Ver =pChar[0]*1000;
    LogData.Fw_Ver+=pChar[1]*100;
    LogData.Fw_Ver+=pChar[2]*10;
    LogData.Fw_Ver+=pChar[3];
    
    LogData.BootType=GetSystemBootType();
    LogData.PanID=PANID;
    LogData.TxMODE=plcGetModulationMode();
    
    
    LogData.MeterBoardMode=meterBoardMode;
    
    LogData.MeterProtocolVer = meterProtocolVer;
            
    
    LogData.ExceptionFlag=exception_flag;
  	LogData.PLC_Rstcnt=PLCResetCnt;
    LogData.Runtime=runtime;
   
	LogData.TotalHistoryReocrd_CCErr=TotalHistoryReocrd_CCErr;
    LogData.TransctionReocrd_CCErr=TransctionReocrd_CCErr;

    plcGetLatestSNR(&snr_t);
	LogData.SNR=snr_t;
    getBoardTemperature_F(&LogData.Temperature);
	LogData.timeStamp=getLocalTick();//+LOCAL_TIME_ZONE_SECOND_OFFSET;
        
    pWord= (unsigned int *)getBoardSerialID();
    memcpy(LogData.HWID,pWord,3*sizeof(u32));
    
    
    pFS = uf_RetrieveFlashSectionInfo(FLASH_SECTION_GASOLINE_DATA);
    
    if(pFS->wptr >= pFS->rptr)
	   cnt=pFS->wptr - pFS->rptr;		
    else		
       cnt = pFS->wptr+pFS->size - pFS->rptr;
		

    cnt= cnt/64;
        
    LogData.DataInfo.Unupload=cnt;       
    LogData.DataInfo.UploadCnt=OTD_Upload_Cnt;
    LogData.DataInfo.Wptr= pFS->wptr;
    LogData.DataInfo.Rptr= pFS->rptr;
    
    LogData.Time_Err=0;  
    LogData.PhyConnPortCnt=0;
    LogData.gPXStatus=0;
    
  
    
    for(i=0;i<TAX_CONTROL_DEVICE_MAX_NUM;i++)
	{
		if(pTaxControlDevMgr->taxControlDevInfo[i].bind == TAX_CONTROL_DEVICE_BOUND)
           BindPortCnt++;  
        
        if(pUartPortState[i].cState == CONNECTION_STATE_CONNECT)
        {
            LogData.PhyConnPortCnt++;
            PhyConnPort=i;
                                  
        }
        
	}
    
  
    
    if(LogData.PhyConnPortCnt==1)
    {
      //端口状态  
        LogData.portinfo = (PortInfo*)malloc(LogData.PhyConnPortCnt*sizeof(PortInfo));
        if( LogData.portinfo==0)
        {
            printf("LogData.portinfo malloc err!\r\n");
            return E_NO_MEMORY;
        }
        memset((u8*)LogData.portinfo,0,LogData.PhyConnPortCnt*sizeof(PortInfo));
        
        LogData.portinfo[0].port = PhyConnPort;
        
        if(pTaxControlDevMgr->taxControlDevInfo[PhyConnPort].bind == TAX_CONTROL_DEVICE_BOUND)
           LogData.portinfo[0].bState=1;
        else
           LogData.portinfo[0].bState=0;  
        
        LogData.portinfo[0].TotalGun=pTaxControlDevMgr->taxControlDevInfo[PhyConnPort].gunTotal;


        for(k=0;k<TAX_CONTROL_DEVICE_GUNNO_MAX;k++)
        {
            LogData.portinfo[0].MeterDataRemain+=pMeterDataGroup[PhyConnPort].MeterTraDataGroup[k].DataCnt;        
        //    LogData.portinfo[0].MeterSplitFail+=pMeterDataGroup[PhyConnPort].MeterTraDataGroup[k].SplitFailCnt; 
            LogData.portinfo[0].MeterResetCnt=MeterResetCnt;
            
        }
       
        LogData.portinfo[0].MeterPortStatus= pControlPortSetMgr->port[PhyConnPort].pendflag;  
        
        //枪状态
        LogData.portinfo[0].guninfo= (GunInfo*)malloc(LogData.portinfo[0].TotalGun*sizeof(GunInfo));
        
        if(LogData.portinfo[0].guninfo==0)
        {
            printf("LogData.guninfo malloc err!\r\n");
            return E_NO_MEMORY;
        }
        memset((u8*)LogData.portinfo[0].guninfo,0,LogData.portinfo[0].TotalGun*sizeof(GunInfo));
        
               
        if(meterBoardMode)//计量模式
        {
            for(g=0;g<MAXIUM_GPX_PER_PORT;g++)
            {
               
                if(pControlPortSetMgr->port[PhyConnPort].gPx[g].taxGun!=UNMAPPED_TAXGUNNO)//只对完成映射的赋值
                {                                            
                   LogData.portinfo[0].guninfo[pControlPortSetMgr->port[PhyConnPort].gPx[g].taxGun].MapStatus = 1;//已映射
                   LogData.portinfo[0].guninfo[pControlPortSetMgr->port[PhyConnPort].gPx[g].taxGun].MapTO = pControlPortSetMgr->port[PhyConnPort].gPx[g].meterGun;
                   LogData.portinfo[0].guninfo[pControlPortSetMgr->port[PhyConnPort].gPx[g].taxGun].Pickcnt = pGunStatusMgr[PhyConnPort].GunPickCnt[pControlPortSetMgr->port[PhyConnPort].gPx[g].taxGun]; 
                   LogData.portinfo[0].guninfo[pControlPortSetMgr->port[PhyConnPort].gPx[g].taxGun].Trycnt = pTaxControlDevMgr->taxControlDevInfo[PhyConnPort].statistic[pControlPortSetMgr->port[PhyConnPort].gPx[g].taxGun].totalTryCount;
                   LogData.portinfo[0].guninfo[pControlPortSetMgr->port[PhyConnPort].gPx[g].taxGun].Gotcnt = pTaxControlDevMgr->taxControlDevInfo[PhyConnPort].statistic[pControlPortSetMgr->port[PhyConnPort].gPx[g].taxGun].successCount;
                
                }
                
            }  
        }
        else //枪模式
        {
            for(g=0;g<pControlPortSetMgr->port[PhyConnPort].gPxTotal;g++)
            {
               
                                       
                if(pControlPortSetMgr->port[PhyConnPort].gPx[g].taxGun!=UNMAPPED_TAXGUNNO)                                          
                {
                   if(PhyConnPort==0)
                       LogData.portinfo[0].guninfo[pControlPortSetMgr->port[PhyConnPort].gPx[g].taxGun].MapTO = g;
                   else
                       LogData.portinfo[0].guninfo[pControlPortSetMgr->port[PhyConnPort].gPx[g].taxGun].MapTO = g + pControlPortSetMgr->delimiter;
                   
                   
                    LogData.portinfo[0].guninfo[pControlPortSetMgr->port[PhyConnPort].gPx[g].taxGun].MapStatus = 1;//已映射              
                    LogData.portinfo[0].guninfo[pControlPortSetMgr->port[PhyConnPort].gPx[g].taxGun].Pickcnt = pControlPortSetMgr->port[PhyConnPort].gPx[g].ppCount;   
                    LogData.portinfo[0].guninfo[pControlPortSetMgr->port[PhyConnPort].gPx[g].taxGun].Trycnt = pTaxControlDevMgr->taxControlDevInfo[PhyConnPort].statistic[pControlPortSetMgr->port[PhyConnPort].gPx[g].taxGun].totalTryCount;
                    LogData.portinfo[0].guninfo[pControlPortSetMgr->port[PhyConnPort].gPx[g].taxGun].Gotcnt = pTaxControlDevMgr->taxControlDevInfo[PhyConnPort].statistic[pControlPortSetMgr->port[PhyConnPort].gPx[g].taxGun].successCount;
               
                }
                                                       
            }
            
            LogData.gPXStatus+=pControlPortSetMgr->port[PhyConnPort].pendflag<<(4*PhyConnPort);
            
        }      
   
    }
    else if(LogData.PhyConnPortCnt==2)
    {
         //端口状态  
        
        LogData.portinfo = (PortInfo*)malloc(LogData.PhyConnPortCnt*sizeof(PortInfo));
        if( LogData.portinfo==0)
        {
            printf("LogData.portinfo malloc err!\r\n");
            return E_NO_MEMORY;
        }
        memset((u8*)LogData.portinfo,0,LogData.PhyConnPortCnt*sizeof(PortInfo));
        
        for(i=0;i<TAX_CONTROL_DEVICE_MAX_NUM;i++)
        {
            
            LogData.portinfo[i].port = i;
            
            if(pTaxControlDevMgr->taxControlDevInfo[i].bind == TAX_CONTROL_DEVICE_BOUND)
               LogData.portinfo[i].bState=1;
            else
               LogData.portinfo[i].bState=0;  
            
            LogData.portinfo[i].TotalGun=pTaxControlDevMgr->taxControlDevInfo[i].gunTotal;


            for(k=0;k<TAX_CONTROL_DEVICE_GUNNO_MAX;k++)
            {
                LogData.portinfo[i].MeterDataRemain+=pMeterDataGroup[i].MeterTraDataGroup[k].DataCnt;        
               // LogData.portinfo[i].MeterSplitFail+=pMeterDataGroup[i].MeterTraDataGroup[k].SplitFailCnt;  
                LogData.portinfo[i].MeterResetCnt=MeterResetCnt;
            }
           
            LogData.portinfo[i].MeterPortStatus= pControlPortSetMgr->port[i].pendflag;  
            
            //枪状态
            LogData.portinfo[i].guninfo= (GunInfo*)malloc(LogData.portinfo[i].TotalGun*sizeof(GunInfo));
            
            if(LogData.portinfo[i].guninfo==0)
            {
                printf("LogData.guninfo malloc err!\r\n");
                return E_NO_MEMORY;
            }
            memset((u8*)LogData.portinfo[i].guninfo,0,LogData.portinfo[i].TotalGun*sizeof(GunInfo));
            
                   
            if(meterBoardMode)//计量模式
            {
                for(g=0;g<MAXIUM_GPX_PER_PORT;g++)
                {
                   
                    if(pControlPortSetMgr->port[i].gPx[g].taxGun!=UNMAPPED_TAXGUNNO)//只对完成映射的赋值
                    {                                            
                       LogData.portinfo[i].guninfo[pControlPortSetMgr->port[i].gPx[g].taxGun].MapStatus = 1;//已映射
                       LogData.portinfo[i].guninfo[pControlPortSetMgr->port[i].gPx[g].taxGun].MapTO = pControlPortSetMgr->port[i].gPx[g].meterGun;
                       LogData.portinfo[i].guninfo[pControlPortSetMgr->port[i].gPx[g].taxGun].Pickcnt = pGunStatusMgr[i].GunPickCnt[pControlPortSetMgr->port[i].gPx[g].taxGun]; 
                       LogData.portinfo[i].guninfo[pControlPortSetMgr->port[i].gPx[g].taxGun].Trycnt = pTaxControlDevMgr->taxControlDevInfo[i].statistic[pControlPortSetMgr->port[i].gPx[g].taxGun].totalTryCount;
                       LogData.portinfo[i].guninfo[pControlPortSetMgr->port[i].gPx[g].taxGun].Gotcnt = pTaxControlDevMgr->taxControlDevInfo[i].statistic[pControlPortSetMgr->port[i].gPx[g].taxGun].successCount;
                    
                    }
                    
                }  
            }
            else //枪模式
            {
                for(g=0;g<pControlPortSetMgr->port[i].gPxTotal;g++)
                {
                   
                                           
                    if(pControlPortSetMgr->port[i].gPx[g].taxGun!=UNMAPPED_TAXGUNNO)                                          
                    {
                       if(i==0)
                           LogData.portinfo[i].guninfo[pControlPortSetMgr->port[i].gPx[g].taxGun].MapTO = g;
                       else
                           LogData.portinfo[i].guninfo[pControlPortSetMgr->port[i].gPx[g].taxGun].MapTO = g+pControlPortSetMgr->delimiter;
                       
                       
                        LogData.portinfo[i].guninfo[pControlPortSetMgr->port[i].gPx[g].taxGun].MapStatus = 1;//已映射              
                        LogData.portinfo[i].guninfo[pControlPortSetMgr->port[i].gPx[g].taxGun].Pickcnt = pControlPortSetMgr->port[i].gPx[g].ppCount;   
                        LogData.portinfo[i].guninfo[pControlPortSetMgr->port[i].gPx[g].taxGun].Trycnt = pTaxControlDevMgr->taxControlDevInfo[i].statistic[pControlPortSetMgr->port[i].gPx[g].taxGun].totalTryCount;
                        LogData.portinfo[i].guninfo[pControlPortSetMgr->port[i].gPx[g].taxGun].Gotcnt = pTaxControlDevMgr->taxControlDevInfo[i].statistic[pControlPortSetMgr->port[i].gPx[g].taxGun].successCount;
                   
                    }
                                                           
                }
                
                LogData.gPXStatus+=pControlPortSetMgr->port[i].pendflag<<(4*i);
                
            }
            
        }
        
        
        
    }
    else if(LogData.PhyConnPortCnt==0)
    {
        ;
    }
    
  
    memset(logBuf,0,REMOTE_LOG_SIZE);
    memcpy(logBuf,(u8*)&LogData,sizeof(LogContent)-8);
    LogDataLEN+=sizeof(LogContent)-8;  
       
    if(LogData.PhyConnPortCnt!=0)
    {
        for(i=0;i<LogData.PhyConnPortCnt;i++) 
        {        
          memcpy(logBuf+LogDataLEN,(u8*)&LogData.portinfo[i], (sizeof(PortInfo)-4));   
          LogDataLEN+= sizeof(PortInfo)-4;
          
          memcpy(logBuf+LogDataLEN,(u8*)LogData.portinfo[i].guninfo, LogData.portinfo[0].TotalGun*sizeof(GunInfo));   
          LogDataLEN+= LogData.portinfo[0].TotalGun*sizeof(GunInfo);
            
        }
      
    }
    
    
        
	if(exception_flag)
	{
        LogData.EXCEPTION=(u32*)malloc(9*sizeof(u32));
        if(LogData.EXCEPTION==0)
        {
            printf("LogData.EXCEPTION malloc failed\r\n");
            return E_NO_MEMORY;
        }
        
        memcpy((u8*)LogData.EXCEPTION,(u8*)&exception_info,9*sizeof(u32));
        
        memcpy(logBuf+LogDataLEN,(u8*)LogData.EXCEPTION,9*sizeof(u32));   
        LogDataLEN+=9*sizeof(u32);
              
        free(LogData.EXCEPTION);
    			
	}
    
    
    for(i=0;i<LogData.PhyConnPortCnt;i++)
	{
 
        if(LogData.portinfo!=0)
         free(LogData.portinfo[i].guninfo);
         
    }
    
    if(LogData.portinfo!=0)
	  free(LogData.portinfo);

    
//    printf("++++++++++\r\n",LogDataLEN);
//    Uart3SendBuf(logBuf,LogDataLEN);
   
    printf("LogSize:%d\r\n",LogDataLEN);
    
}





void logTask()
{
    collectLogInfo();    
}

void logTask2()
{
    collectLogInfo2();    
}


unsigned char *logGetBuffer()
{
    return logBuf;
}

void PrintfLogo(void)
{	
	
	 unsigned char *FW_VER;
     unsigned int *HW_ID;

    FW_VER = getFwVersion();
    HW_ID= (unsigned int *)getBoardSerialID();
   
	
	printf("**************************************************************\r\n");
	printf("* Release Date : %s %s\r\n", __DATE__,__TIME__);	
	printf("* FW Rev: %d.%d.%d.%d\r\n", FW_VER[0],FW_VER[1],FW_VER[2],FW_VER[3]);	
	if(getPingPongFlag())
	  printf("* TX_Modulation: %d, PanId: %d, PiNG!\r\n",plcGetModulationMode(),PANID);	
	else
		printf("* TX_Modulation: %d, PanId: %d, PoNG!\r\n",plcGetModulationMode(),PANID);
	
	printf("* HW ID: %08X %08X %08X %08X \r\n",__REV(HW_ID[0]),__REV(HW_ID[1]),__REV(HW_ID[2]),__REV(HW_ID[3]));	
	printf("**************************************************************\r\n");

	
}
