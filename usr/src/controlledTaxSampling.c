
#include "SEGGER_RTT.h"

#include "controlledTaxSampling.h"
#include "taxControlInterface.h"
#include "meterMessager.h"
#include "board.h"
#include "localTime.h"

#define dbgPrintf	SEGGER_RTT_printf

static ControlPortSetMgr    controlPortMgr;
static MeterState  meterState[METERPORT_MAX_NUM] = {METER_STATE_INVALID,METER_STATE_INVALID};

static unsigned char meterBoardAlive = 0;
Sample_Mode  meterBoardMode = FREE_MODE;
unsigned char  meterProtocolVer =0;
extern MeterTransactionDataGroup MeterDataGroup[TAX_CONTROL_DEVICE_MAX_NUM];

u8 MeterResetCnt=0;
//计量模式下，周期性进行税控口抄报，周期待定
//TAX_SAMPLE_UNLOCK TaxSampleUnLock[MAXIUM_PORT_SET_NUM]={0};
u8 TaxSampleUnLock[MAXIUM_PORT_SET_NUM]={0};


ControlPortSetMgr *getControlPortMgr(void)
{
    return &controlPortMgr;
}

Sample_Mode getMeterBoardMode(void)
{
    return meterBoardMode;
}

void setMeterBoardMode(Sample_Mode state)
{
     meterBoardMode=state;
}
/*********************************************************************************************
Function name: meterBoardAliveMonitor
Description:
     This function intends to monitor the alive status of meter board, it should be called every 10s(As per design, meter
board must send heratbeart message every 3 seconds, we set margin 3 times + 1 second).
Parameter: 
    NULL
Return:
    NULL
*********************************************************************************************/
void    meterBoardAliveMonitor()
{
    printf("meterBoardAliveMonitor: Alive:%d\r\n",meterBoardAlive);
    if(meterBoardAlive)
    {
        ;// do nothing
    }
    else if(getMeterBoardMode() != FREE_MODE)   // Reset Meter Board
    {
        printf("meterBoardAliveMonitor:Reset Gun/Meter Board\r\n");
        MeterResetHigh;
        bsp_DelayMS(2000);
        MeterResetLow;   
        MeterResetCnt++;        
    }

    // Anyway, we need reset this flag
    meterBoardAlive = 0;    
}



MeterState  getMeterState(u8 meterport)
{
    return meterState[meterport];
}

MeterState  setMeterState(u8 meterport,MeterState state)
{
     meterState[meterport]=state;
}

ControlPortSetMgr * getControlPortSetMgr()
{
    return &controlPortMgr;
}

void  controlledSamplingSyncGpxState(unsigned char port,unsigned char status)
{
    unsigned char  g=0;
    ControlPortSetMgr *pControlPortSetMgr = (ControlPortSetMgr *)&controlPortMgr;

    if(pControlPortSetMgr->valid && port==DummyData)//摘挂枪信号驱动的枪状态
    {
        unsigned char delimiter = pControlPortSetMgr->delimiter;

        pControlPortSetMgr->port[0].gPxSetState = GPX_PP_STATE_PUTDOWN;
        for(g=0;g<delimiter;g++)
        {
            if((1 << g) & status)
            {
                pControlPortSetMgr->port[0].gPx[g].ppState = GPX_PP_STATE_PICKUP;
                pControlPortSetMgr->port[0].gPxSetState = GPX_PP_STATE_PICKUP;
            }
            else
            {
                pControlPortSetMgr->port[0].gPx[g].ppState = GPX_PP_STATE_PUTDOWN;                            
            }
        //    printf("Gpx State Sync: Port 0: Gpx:%d  State:%d\r\n",g,pControlPortSetMgr->port[0].gPx[g].ppState);
        }
        
        pControlPortSetMgr->port[1].gPxSetState = GPX_PP_STATE_PUTDOWN;
        unsigned char gEnd = delimiter +  pControlPortSetMgr->port[1].gPxTotal;
        for(g=delimiter;g<gEnd;g++)
        {
            if((1 << g) & status)
            {
                pControlPortSetMgr->port[1].gPx[g-delimiter].ppState = GPX_PP_STATE_PICKUP;
                pControlPortSetMgr->port[1].gPxSetState = GPX_PP_STATE_PICKUP;
            }
            else
            {
                pControlPortSetMgr->port[1].gPx[g-delimiter].ppState = GPX_PP_STATE_PUTDOWN;                            
            }
         //   printf("Gpx State Sync: Port 1: Gpx:%d  State:%d\r\n",g,pControlPortSetMgr->port[1].gPx[g-delimiter].ppState);
        }
    }
    else //计量驱动的枪状态
    {
       
        pControlPortSetMgr->port[port].gPxSetState = GPX_PP_STATE_PUTDOWN;
        for(g=0;g<MAXIUM_GPX_PER_PORT;g++)
        {
            if((1 << g) & status)
            {
                pControlPortSetMgr->port[port].gPx[g].ppState = GPX_PP_STATE_PICKUP;
                pControlPortSetMgr->port[port].gPxSetState = GPX_PP_STATE_PICKUP;
            }
            else
            {
                pControlPortSetMgr->port[port].gPx[g].ppState = GPX_PP_STATE_PUTDOWN;                            
            }
            //printf("Gpx State Sync: Port 0: Gpx:%d  State:%d\r\n",g,pControlPortSetMgr->port[0].gPx[g].ppState);
        }
    }
        
}

/*
void  controlledSamplingSyncGpxState(unsigned char status)
{
    unsigned char  g=0;
    ControlPortSetMgr *pControlPortSetMgr = (ControlPortSetMgr *)&controlPortMgr;
    if(pControlPortSetMgr->valid)
        {
            unsigned char delimiter = pControlPortSetMgr->delimiter;

            pControlPortSetMgr->port[0].gPxSetState = GPX_PP_STATE_PUTDOWN;
            for(g=0;g<delimiter;g++)
            {
                if((1 << g) & status)
                {
                    pControlPortSetMgr->port[0].gPx[g].ppState = GPX_PP_STATE_PICKUP;
                    pControlPortSetMgr->port[0].gPxSetState = GPX_PP_STATE_PICKUP;
                }
                else
                {
                    pControlPortSetMgr->port[0].gPx[g].ppState = GPX_PP_STATE_PUTDOWN;                            
                }
                //printf("Gpx State Sync: Port 0: Gpx:%d  State:%d\r\n",g,pControlPortSetMgr->port[0].gPx[g].ppState);
            }
            
            pControlPortSetMgr->port[1].gPxSetState = GPX_PP_STATE_PUTDOWN;
            unsigned char gEnd = delimiter +  pControlPortSetMgr->port[1].gPxTotal;
            for(g=delimiter;g<gEnd;g++)
            {
                if((1 << g) & status)
                {
                    pControlPortSetMgr->port[1].gPx[g-delimiter].ppState = GPX_PP_STATE_PICKUP;
                    pControlPortSetMgr->port[1].gPxSetState = GPX_PP_STATE_PICKUP;
                }
                else
                {
                    pControlPortSetMgr->port[1].gPx[g-delimiter].ppState = GPX_PP_STATE_PUTDOWN;                            
                }
                //printf("Gpx State Sync: Port 1: Gpx:%d  State:%d\r\n",g,pControlPortSetMgr->port[1].gPx[g].ppState);
            }
        }
}
*/

// This APIs shoul be called after tax control dev initialize since depends on that
errorCode    initControlPortSetManager()
{
    ControlPortSetMgr   *pControlPortSetMgr = (ControlPortSetMgr *)&controlPortMgr;
    TaxControlDevManager *pTaxControlDevManager = (TaxControlDevManager *)getTaxControlDevManager();

    memset(pControlPortSetMgr,0,sizeof(ControlPortSetMgr));


    if(pTaxControlDevManager->taxControlDevInfo[0].bind == TAX_CONTROL_DEVICE_BOUND)
    {
        pControlPortSetMgr->delimiter = pTaxControlDevManager->taxControlDevInfo[0].gunTotal;
    }
    else    // At least we need one tax control device binded
    {
        return E_FAIL;
    }

    // Initialize each port's configuration to reasonable default value
    for(int port=0;port<MAXIUM_PORT_SET_NUM;port++)
    {
        ControlPortStruct *pControlPortStruct = (ControlPortStruct *)&pControlPortSetMgr->port[port];
        if(pTaxControlDevManager->taxControlDevInfo[port].bind == TAX_CONTROL_DEVICE_BOUND)
        {
            pControlPortStruct->gPxTotal = pTaxControlDevManager->taxControlDevInfo[port].gunTotal;
           // pControlPortStruct->portState = CONTROL_PORT_STATE_MAPING;
            pControlPortStruct->portState = CONTROL_PORT_STATE_PERIOD_IDLE;
            
            pControlPortStruct->gPxSetState = GPX_PP_STATE_PICKUP;
            
            pControlPortStruct->getTsdFlag=1;//上电获取总累标志
            
            for(int gPx=0;gPx<pControlPortStruct->gPxTotal;gPx++)
            {
                pControlPortStruct->gPx[gPx].ppState = GPX_PP_STATE_PICKUP;
                pControlPortStruct->gPx[gPx].taxDev  = port;
                pControlPortStruct->gPx[gPx].taskState = GPX_TASK_STATE_PEND;   // Default to sample once after binding
                pControlPortStruct->pendflag |= 1 << gPx;
               
            }
            
            for(int gPx=0;gPx<MAXIUM_GPX_PER_PORT;gPx++)//???
            {
                pControlPortStruct->gPx[gPx].taxGun = UNMAPPED_TAXGUNNO;
                pControlPortStruct->gPx[gPx].meterGun = UNMAPPED_TAXGUNNO;
                pControlPortStruct->gPx[gPx].ppState = GPX_PP_STATE_PICKUP;
                pControlPortStruct->gPx[gPx].taskState = GPX_TASK_STATE_PEND; 
                pControlPortStruct->pendflag |= 1 << gPx;
            }
            
            
            pControlPortStruct->valid = 1;
        }  
    }

   pControlPortSetMgr->valid = 1;   //delimiter确定后此valid有效
    
   return E_SUCCESS; 
}
/********************************************************************************************************
Function: ControlPortEventHandler
Description:
    Intends for external call back(should be meterMessage task) to handle Putdown,Pickup and Trasaction event, etc
I/O:
    pMsg:[I],  Message pointer
Return:
    E_SUCCESS:  Successfully processed the message
    E_FAIL:  Not supported message or other cases that can't handle this message
*********************************************************************************************************/ 
errorCode   ControlPortEventHandler(unsigned char *pMsg)
{
    ControlPortSetMgr   *pControlPortSetMgr = (ControlPortSetMgr *)&controlPortMgr;
    ControlPortStruct *pControlPortStruct = 0;    
    GunStatusManager *pGunStatusMgr = getGunStatusManager();
    TaxControlDevManager *taxdevmgr =getTaxControlDevManager();
    
    MeterMsgRcvDes * pDes = (MeterMsgRcvDes  *)pMsg;
    unsigned char mode = 0;   // Seperate gun mode and meter mode
    unsigned char port = 0;     // port number  
    unsigned char MeterGunNo=0; //计量协议中带出来的枪号
 //   unsigned char TaxGunNo=0; //报税口对应的真实枪号
    unsigned char gPx = 0; /* 枪信号模式下gpx是枪信号位置，计量模式下是计量协议中带出的枪号对报税口下支持的总枪数取模的结果，是一种虚拟映射 */
    unsigned char allgun=0;
    
    allgun=taxdevmgr->taxControlDevInfo[0].gunTotal+taxdevmgr->taxControlDevInfo[1].gunTotal;
    // Reveived message from meter board, set this flag.
    meterBoardAlive = 1;
    
    MeterMsgStatus *pMeterMsgStatus = (MeterMsgStatus *)&pDes->head;
    // We need make sure that control port manager is valid, otherwise, just drop this message
    if(pControlPortSetMgr->valid)
    {
                    
        switch  (pMeterMsgStatus->head.msg)
        {
            case    METER_MSG_STATUS:       // Pickup/Putdown message
            {
                                   
                mode = pMeterMsgStatus->mode;
              
              
                if(mode == 0)   // Gun mode
                {
                    gPx = pMeterMsgStatus->gunNo; 
                                                                           
                    if(gPx >= pControlPortSetMgr->delimiter)
                    {
                        gPx -= pControlPortSetMgr->delimiter;   // Caldulate correct gPx
                        port = 1;                              // Assign proper tax Dev
                        
                        if(gPx >= MAXIUM_GPX_PER_PORT)  // We onlu support the monster  as 8_guns_machine
                        {
                            printf("ControlPortEventHandler: Gpx:%d >= %d no processing!\r\n",gPx,MAXIUM_GPX_PER_PORT);
                            return E_INPUT_PARA;
                        }
                                                
                    } 
                                  
                    pControlPortStruct =(ControlPortStruct *)&pControlPortSetMgr->port[port];   
                    
                    if(gPx >= pControlPortStruct->gPxTotal)
                    {
                        printf("ControlPortEventHandler: Gpx:%d >= AllGunNum:%d no processing!\r\n",gPx+pControlPortSetMgr->delimiter,allgun);
                        return E_INPUT_PARA;
                    }
                    
                    
                }
                else    // Meter Mode 
                {              
                                      
                    if(pMeterMsgStatus->port>1)
					{					
						printf("ControlPortEventHandler: Port:%d is not support! CMD:0x%02X\r\n",pMeterMsgStatus->port,METER_MSG_STATUS);
						return E_FAIL;
					} 
                    
                    setMeterState(pMeterMsgStatus->port,METER_STATE_VALID);
                    
                    port=pMeterMsgStatus->port;//抄报共支持两个加油机的计量口，此计量口号与报税口号是一一对应的                                    
                    pControlPortStruct =(ControlPortStruct *)&pControlPortSetMgr->port[port]; 
                    if(pMeterMsgStatus->gunNo==0)
                    {
                         printf("ControlPortEventHandler: Msg:0x%02X,Mode:%d Port:%d [gunNo:%d] GPx:%d Status:0x%02X,gunNo err!\r\n",
                         METER_MSG_STATUS,mode,pMeterMsgStatus->port,pMeterMsgStatus->gunNo,gPx,pMeterMsgStatus->status);
                 
						return E_INPUT_PARA;
                    }
              
                    
                    MeterGunNo=pMeterMsgStatus->gunNo;
                    pMeterMsgStatus->gunNo-=1;    
                    gPx = pMeterMsgStatus->gunNo % MAXIUM_GPX_PER_PORT;
                   
                    pControlPortSetMgr->port[pMeterMsgStatus->port].gPx[gPx].taxDev=pMeterMsgStatus->port;
                    pControlPortSetMgr->port[pMeterMsgStatus->port].gPx[gPx].meterGun=MeterGunNo;
                    
                    
                    
                } 
                
                 printf("ControlPortEventHandler: Msg:0x%02X,Mode:%d Port:%d gunNo:%d GPx:%d Status:0x%02X\r\n",
                       METER_MSG_STATUS,mode,pMeterMsgStatus->port,pMeterMsgStatus->gunNo+1,gPx,pMeterMsgStatus->status);
                 
                
                if(pControlPortStruct->valid)
                {
                    // Anyway, we need sync up the pickup/putdown state for gPx
                    pControlPortStruct->gPx[gPx].ppState = pMeterMsgStatus->status;                    
                    // If current gPx in Putdown state, set pend flag, and also check port state if port state is in PICKUP state.
                    if(pControlPortStruct->gPx[gPx].ppState == GPX_PP_STATE_PUTDOWN)
                    {
                        pControlPortStruct->gPx[gPx].ppCount++;//摘挂抢信号摘枪计数
                        
                        if(pControlPortStruct->gPx[gPx].taxGun!=UNMAPPED_TAXGUNNO) //未完成映射不计数                   
                          pGunStatusMgr[port].GunPickCnt[pControlPortStruct->gPx[gPx].taxGun]++;//计量信号摘枪计数
                        
                       
                        pControlPortStruct->pendflag |= 1 << gPx;
                        pControlPortStruct->gPx[gPx].taskState = GPX_TASK_STATE_PEND;

                        printf("ControlPortEventHandler: Set pend flag for Port:%d  Gpx:%d\r\n",port,gPx);
                        // if @pickup state, need poll all Gpx state, then can change back to putdown state
                        if(pControlPortStruct->gPxSetState== GPX_PP_STATE_PICKUP)
                        {
                            u8 g=0;
                            if(mode == 0)   // Gun mode                             
                            {
                                for(g=0;g<pControlPortStruct->gPxTotal;g++)
                                {
                                    if(pControlPortStruct->gPx[g].ppState == GPX_PP_STATE_PICKUP )
                                        break;
                                }
                                
                                if(g >= pControlPortStruct->gPxTotal)
                                {
                                    pControlPortStruct->gPxSetState= GPX_PP_STATE_PUTDOWN;
                                }
                            }
                            else // METER mode     
                            {
                                for(g=0;g<MAXIUM_GPX_PER_PORT;g++)
                                {
                                    if(pControlPortStruct->gPx[g].ppState == GPX_PP_STATE_PICKUP )
                                        break;
                                }
                                
                                if(g >= MAXIUM_GPX_PER_PORT)
                                {
                                    pControlPortStruct->gPxSetState= GPX_PP_STATE_PUTDOWN;
                                }
                            }

                            printf("ControlPortEventHandler: Port is in %d state\r\n",pControlPortStruct->gPxSetState);
                        }
                    }
                    else    // Absolutely, set the port pp state to pickup
                    {
                        pControlPortStruct->gPxSetState= GPX_PP_STATE_PICKUP;
                        pControlPortStruct->gPx[gPx].ppState = GPX_PP_STATE_PICKUP;
                    }
                                                
                   
                }
                else
                {
                    printf("ControlPortEventHandler:Got Port:%d Gpx:%d Status:%d, but drop since Port is not valid\r\n",port,gPx,pMeterMsgStatus->status);
                    return E_FAIL;
                }
                
            }
            break;


            case    METER_RESP_STATUS:      // Meter board Hearbeat with Gpx status message
            {
                MeterResp *pResp = (MeterResp *)&pDes->head;
                meterBoardMode = pResp->mode;
                meterProtocolVer = pResp->head.version;
                
         
                // Synced with local putdown/pullup state. Yes, absolutely believe in the status from meter board 
                if(meterBoardMode==0)  //GUN MODE
                {                    
                    controlledSamplingSyncGpxState(pResp->port,pResp->status);
                }
                else
                {
                   if(pResp->mark==0)
                   {                   
                      controlledSamplingSyncGpxState(pResp->port,pResp->status); 
                      setMeterState(pResp->port,METER_STATE_VALID); 
                   }
                   else
                   {
                        setMeterState(pResp->port,METER_STATE_INVALID);
                   }
                       
                }
                printf("ControlPortEventHandler:Msg:0x%02X,PORT:%d,Mode:%d,Gpx:0x%02X,Mark:%d\r\n",METER_RESP_STATUS,pResp->port,meterBoardMode,pResp->status,pResp->mark);
                
               
                    
            }
            break;
            
            case    METER_MSG_TRANSACTION:
            {               
                
                MeterOnceTransactionRecord  MeterTraRecord;
                MeterMsgTransaction *pMsgTransaction = (MeterMsgTransaction *)&pDes->head;
                
                TaxControlDevManager *pTaxCDM;
                TaxControlDevInfo *pTaxControlDevInfo;
          

             //   mode = pMsgTransaction->mode;    
                                           
                  
                    unsigned int  amount,volume,price;
					u8 index;
                                       										 
					if(pMsgTransaction->port>1)
					{					
						printf("ControlPortEventHandler: Port:%d is not support!CMD:0x%02X\r\n",pMsgTransaction->port,METER_MSG_TRANSACTION);
						return E_FAIL;
					} 
                    
                   pTaxCDM=getTaxControlDevManager();
				   pTaxControlDevInfo=(TaxControlDevInfo *)(&pTaxCDM->taxControlDevInfo[pMsgTransaction->port]);
                    
                    pControlPortStruct =(ControlPortStruct *)&pControlPortSetMgr->port[pMsgTransaction->port];
                    if( pMsgTransaction->gunNo==0)
                    {
                        printf("ControlPortEventHandler: Port:%d,[gunNo:%d],Msg:0x%02X,gunNO ERR!\r\n",pMsgTransaction->port,pMsgTransaction->gunNo,METER_MSG_TRANSACTION);
                        return E_INPUT_PARA;
                    }
                    
                    MeterGunNo=pMsgTransaction->gunNo;
                    pMsgTransaction->gunNo-=1;//计量协议中枪号从1开始，对应到税控枪号要减1
                    gPx= pMsgTransaction->gunNo % MAXIUM_GPX_PER_PORT;//计量枪号对报税口下的最大总枪数取模
                    
                    pControlPortSetMgr->port[pMsgTransaction->port].gPx[gPx].taxDev=pMsgTransaction->port;
                    pControlPortSetMgr->port[pMsgTransaction->port].gPx[gPx].meterGun=MeterGunNo;
                    
                 //   TaxGunNo=pControlPortStruct->gPx[gPx].taxGun;//真实的报税口下的枪号
                    
                    index=MeterDataGroup[pMsgTransaction->port].MeterTraDataGroup[gPx].DataCnt%METER_MSG_MAX_LEN;					
                    price =  (pMsgTransaction->price[0] << 8) + pMsgTransaction->price[1];
                    volume = (pMsgTransaction->volume[0] << 24) +  (pMsgTransaction->volume[1] << 16) +  (pMsgTransaction->volume[2] << 8) +  pMsgTransaction->volume[3];
                    amount = (pMsgTransaction->amount[0] << 24) +  (pMsgTransaction->amount[1] << 16) +  (pMsgTransaction->amount[2] << 8) +  pMsgTransaction->amount[3];
             					
							
					MeterDataGroup[pMsgTransaction->port].MeterTraDataGroup[gPx].DataCnt++;					
					MeterDataGroup[pMsgTransaction->port].MeterTraDataGroup[gPx].amount[index]=amount;
					MeterDataGroup[pMsgTransaction->port].MeterTraDataGroup[gPx].volume[index]=volume;
					MeterDataGroup[pMsgTransaction->port].MeterTraDataGroup[gPx].price[index]=price;
					MeterDataGroup[pMsgTransaction->port].MeterTraDataGroup[gPx].timestamp[index]=getLocalTick();
										
                    MeterTraRecord.head.mark=DIRTY;
                    MeterTraRecord.head.data_type=MeterData;//计量数据
                    
                    MeterTraRecord.content.NZN=MeterGunNo;
                    MeterTraRecord.content.amount=amount;
                    MeterTraRecord.content.volume=volume;
                    MeterTraRecord.content.PRC=price;
                    
                    MeterTraRecord.content.timeStamp=getLocalTick();
                    MeterTraRecord.content.C_Type=pMsgTransaction->C_Type;
                    MeterTraRecord.content.T_Type=pMsgTransaction->T_Type;
                    MeterTraRecord.content.UNIT=pMsgTransaction->UNIT;
                    MeterTraRecord.content.DS =pMsgTransaction->DS;
                    
                    MeterTraRecord.content.port=pMsgTransaction->port;
                    MeterTraRecord.content.G_CODE[0]=pMsgTransaction->G_CODE[0];
                    MeterTraRecord.content.G_CODE[1]=pMsgTransaction->G_CODE[1];
                    
                    MeterTraRecord.content.POS_TTC=pMsgTransaction->POS_TTC;
                    MeterTraRecord.content.T_MAC=pMsgTransaction->T_MAC;                  
                    MeterTraRecord.content.reserve=0;
                    
                    memcpy((u8*)MeterTraRecord.content.factorySerialNo,(u8*)pTaxControlDevInfo->serialID,10);
                    
                    memcpy(MeterTraRecord.content.ASN,pMsgTransaction->ASN,10);
                    memset((u8*)(MeterTraRecord.content.ASN+10),0x66,10);
                                       
                    storeTransactionRecord(&MeterTraRecord,MeterData);


                    printf("ControlPortEventHandler: Port:%d MeterGun:%d gPx:%d,Seq:%d,Volume:%d Amount:%d Price:%d Time:%u\r\n",
                            pMsgTransaction->port,pMsgTransaction->gunNo+1,gPx,pMsgTransaction->head.seqNo,volume,amount,price,getLocalTick());                    
                                                  
               
            }
            break;
            
            default:
            {
                printf("ControlPortEventHandler: This message not supported yet\r\n");
                return E_FAIL;
            }
            break;    
        }
    }
    else
    {
        printf("ControlPortEventHandler: Invalid ControlPortSetMgr,do nothing. Msg:0x%02X,port:%d,mode:%d,status:0x%02X\r\n",
               pMeterMsgStatus->head.msg,pMeterMsgStatus->port,pMeterMsgStatus->mode,pMeterMsgStatus->status);
        return E_FAIL;
    }
    return E_SUCCESS;
}

static  errorCode controlledSamplingTaskOnMaping(ControlPortStruct *pControlPort)
{
    errorCode ret = E_SUCCESS;
    unsigned char successNum = 0;
    unsigned char taxGunNum = 0;
    unsigned char taxDev = pControlPort->gPx[0].taxDev;
  //  TaxOnceTransactionRecord record;
    
    ret =  taxControlSamplingDev(taxDev,&successNum,&taxGunNum);

    if(ret == E_SUCCESS)
    {
        unsigned char g = 0;
        if(successNum == 1)	// We successfully got one real transaction.
        {
            unsigned char count = 0;
            
            // Then we need check how many Gpx pended here, if only 1, then we can map the Gpx to return  "taxGunNum"  
            if(meterBoardMode==0)//gunmode
            {
                for(g=0;g<pControlPort->gPxTotal;g++)
                {
                    if(pControlPort->gPx[g].taskState == GPX_TASK_STATE_PEND)
                        count++;
                }
            }
            else
            {
                for(g=0;g<MAXIUM_GPX_PER_PORT;g++)
                {
                    if(pControlPort->gPx[g].taskState == GPX_TASK_STATE_PEND)
                        count++;
                }
            }

            if(count  == 1)     // Yes, we have only 1 Gpx in pended state                 
            {
                
                if(meterBoardMode==0)//gunmode
                {
                    for(g=0;g<pControlPort->gPxTotal;g++)
                    {
                        if(pControlPort->gPx[g].taskState== GPX_TASK_STATE_PEND)
                        {
                            pControlPort->gPx[g].taxGun = taxGunNum;                                            
                            pControlPort->gPx[g].mapState = GPX_MAP_YES;
                            pControlPort->gPx[g].taskState = GPX_TASK_STATE_IDLE;
                        
                            pControlPort->gPxMappedNum++;

                            printf("##controlledSamplingTaskOnMaping: Finished a Mapping: gP%d->tg%d\r\n",g,taxGunNum);
                        }
                    } 
                }
                else  
                {
                    for(g=0;g<MAXIUM_GPX_PER_PORT;g++)
                    {
                        if(pControlPort->gPx[g].taskState== GPX_TASK_STATE_PEND)
                        {                            
                                                       
                           if( pControlPort->gPx[g].mapState == GPX_MAP_NO)
                           {                                                 
                                pControlPort->gPxMappedNum++;
                                pControlPort->gPx[g].mapState = GPX_MAP_YES;
                                pControlPort->gPx[g].taxGun = taxGunNum; 
                                printf("**controlledSamplingTaskOnMaping: Finished a Mapping: MeterGun:%d->TaxGun:%d\r\n",pControlPort->gPx[g].meterGun,taxGunNum);
                           
                           }
                                                                                                                           
                           pControlPort->gPx[g].taskState = GPX_TASK_STATE_IDLE;
                           
                           
                        }
                    } 
                    
                }  

                
            }
            
                
        }
        else
        {
            printf("##controlledSamplingTaskOnMaping: Success Transaction Num:%d\r\n",successNum);
        }
        
        if(meterBoardMode==0)//gunmode
        {
            pControlPort->pendflag = 0;
            for(g=0;g<pControlPort->gPxTotal;g++)
            {
                pControlPort->gPx[g].taskState = GPX_TASK_STATE_IDLE;
            }
        }
        else
        {
            pControlPort->pendflag = 0;
            for(g=0;g<MAXIUM_GPX_PER_PORT;g++)
            {
                pControlPort->gPx[g].taskState = GPX_TASK_STATE_IDLE;
            }
            
        }
    }

    return ret;
}

static errorCode controlledSamplingTaskOnWorking(ControlPortStruct *pControlPort)
{
    errorCode   ret = E_SUCCESS;
    unsigned char taxDev = pControlPort->gPx[pControlPort->gPxCurrent].taxDev;
    unsigned char taxGun =  pControlPort->gPx[pControlPort->gPxCurrent].taxGun;

    ret = taxControlSamplingGun(taxDev,taxGun);   
                    
    return ret;
}

/********************************************************************************************************
Function: controlledSamplingTask
Description:
    This function is free running task, handle control port's mapping(Gpx # to tax control dev's gun #) process in MAPPing state, do
 all the pended task while in WORKING state. This task is self-controled by it's own descriptor -- "controlPortMgr".
 I/O:    NULL
Return: E_SUCCESS, return success forever.
*********************************************************************************************************/ 
void    controlledSamplingTask()
{
   
    unsigned char port=0;
    u8 gPx=0;
    ControlPortSetMgr   *pControlPortSetMgr = (ControlPortSetMgr *)&controlPortMgr;

    if(!pControlPortSetMgr->valid)            
         return;

    for(port=0;port<MAXIUM_PORT_SET_NUM;port++)             // Need polling all the port supported
    {
        if(pControlPortSetMgr->port[port].valid)
        {
            ControlPortStruct *pControlPort =  (ControlPortStruct *)&pControlPortSetMgr->port[port];
            
            switch(pControlPort->portState)
            {                  
                case    CONTROL_PORT_STATE_MAPING:
                {
                    
                    if((pControlPort->gPxSetState == GPX_PP_STATE_PUTDOWN)  && (pControlPort->pendflag != 0))
                    {
                        printf("controlledSamplingTask[MAPPING] Port:%d execute pending task\r\n",port);
                        controlledSamplingTaskOnMaping(pControlPort);                                            
                    }
                  

                    // Do check whether we need switch to POLLING state
                    if(getMeterBoardMode() == GUN_SIGNAL_MODE)//gun mode
                    {
                        for(gPx=0; gPx<pControlPort->gPxTotal;gPx++)
                        {
                            if(pControlPort->gPx[gPx].mapState == GPX_MAP_NO)
                                break;
                        }

                        if(gPx >= pControlPort->gPxTotal)
                        {
                            pControlPort->portState = CONTROL_PORT_STATE_POLLING;
                            printf("#controlledSamplingTask[MAPPING] Port:%d Completed Mapping, Switch to Polling State\r\n",port);
                        }
                    }
                    else
                    {
                      
                        if(pControlPort->gPxMappedNum == pControlPort->gPxTotal)
                        {
                            pControlPort->portState = CONTROL_PORT_STATE_POLLING;
                            printf("*controlledSamplingTask[MAPPING] Port:%d Completed Mapping, Switch to Polling State\r\n",port);
                        }
                    }
                }
                break;

                case    CONTROL_PORT_STATE_POLLING:
                {
                    if((pControlPort->gPxSetState == GPX_PP_STATE_PUTDOWN)  && (pControlPort->pendflag != 0))
                    {
                        printf("controlledSamplingTask[POLLING]: Port:%d in putdown state, and have Pend task:%x\r\n",port,pControlPort->pendflag);
                        // 1st: find proper gPx# for sampling from gPxCurrent, Be careful about "wrap around"MAXIUM_GPX_PER_PORT
                        if(getMeterBoardMode() == GUN_SIGNAL_MODE)//gun mode
                        {
                            if(pControlPort->gPxTotal == 1)
                            {
                                pControlPort->gPxCurrent = 0;           // Forever
                                pControlPort->portState = CONTROL_PORT_STATE_WORKING;
                                uartEmptyRcvBuffer(port);
                                taxControlDeviceRecoverStateToIdle(pControlPort->gPx[pControlPort->gPxCurrent].taxDev);   
                            }
                            else
                            {
                                pControlPort->gPxCurrent++;
                                if(pControlPort->gPxCurrent >= pControlPort->gPxTotal)
                                    pControlPort->gPxCurrent = 0;

                                if(pControlPort->gPx[pControlPort->gPxCurrent].taskState == GPX_TASK_STATE_PEND)
                                {
                                    pControlPort->portState = CONTROL_PORT_STATE_WORKING;
                                    uartEmptyRcvBuffer(port);
                                    taxControlDeviceRecoverStateToIdle(pControlPort->gPx[pControlPort->gPxCurrent].taxDev);   
                                }                                                    
                            }
                        }
                        else 
                        {
                            for(gPx=0; gPx<MAXIUM_GPX_PER_PORT;gPx++)
                            {
                                if(pControlPort->gPx[gPx].mapState == GPX_MAP_YES)
                                {
                                    if(pControlPort->gPx[gPx].taskState == GPX_TASK_STATE_PEND)
                                    {
                                        pControlPort->portState = CONTROL_PORT_STATE_WORKING;
                                        pControlPort->gPxCurrent = gPx;
                                        uartEmptyRcvBuffer(port);
                                        taxControlDeviceRecoverStateToIdle(pControlPort->gPx[pControlPort->gPxCurrent].taxDev);   
                                    }   
                                }
                                else
                                {
                                    if(pControlPort->gPx[gPx].taskState == GPX_TASK_STATE_PEND)
                                    {
                                 
                                        pControlPort->pendflag&=~(1<<gPx);
                                        pControlPort->gPx[gPx].taskState = GPX_TASK_STATE_IDLE;
                                        printf("controlledSamplingTask[POLLING]: Port:%d,MeterGun:%d isn't exist!\r\n",port,pControlPort->gPx[gPx].meterGun);
    
                                    }  
                                }
                            }                                                                                                      
                        }
                                                
                    }
                    
                }                            
                break;    

                case    CONTROL_PORT_STATE_WORKING:
                {
                    errorCode   ret = E_SUCCESS;

                    if(pControlPort->gPxSetState == GPX_PP_STATE_PUTDOWN)   // Thanks ALin, add this limit, active only in Set_putdown state
					{
						ret=controlledSamplingTaskOnWorking(pControlPort);
						if(ret == E_SUCCESS_TRANSACTION || ret == E_SUCCESS)
						{							                                                       
                            pControlPort->gPx[pControlPort->gPxCurrent].taskState = GPX_TASK_STATE_IDLE;
							pControlPort->pendflag &= ~( 1 << pControlPort->gPxCurrent);
							pControlPort->portState = CONTROL_PORT_STATE_POLLING;
                            if(ret == E_SUCCESS)
                                printf("controlledSamplingTask[WORKING]: Port:%d,taxGun:%d.No Real Transaction.\r\n",port,pControlPort->gPx[pControlPort->gPxCurrent].taxGun); 
						}
						else if(ret == E_FAIL)
						{
							pControlPort->portState = CONTROL_PORT_STATE_POLLING;
						}
						else
						{
							; // Do nothing since it is in progress
						}
					}
                    else    // This port has "pickup event" while working, need recover taxcontrol device state machine
					{
						pControlPort->portState = CONTROL_PORT_STATE_POLLING;
						printf("!!controlledSamplingTask[Working]: Port:%d Pickup event occurs, recover tax dev state\r\n",port);
					}
                }
                break;
               
                
                case    CONTROL_PORT_STATE_PERIOD_IDLE://此状态用于计量模式下的周期性抄报，以及启动时查询并上传总累数据
                {
                    UartPortState *currentState;
                    errorCode   ret = E_SUCCESS;
                              
                    currentState=uartGetPortCurrentState();
                    
                    if(getMeterBoardMode() == METER_MODE)
                    {
                        if(TaxSampleUnLock[port]==1  || pControlPort->getTsdFlag==1)
                        {
                                                   
                            if(pControlPort->gPxSetState == GPX_PP_STATE_PUTDOWN && (currentState[port].cState == CONNECTION_STATE_CONNECT))
                             {
                                 uartEmptyRcvBuffer(port); 
                                 taxControlDeviceRecoverStateToIdle(port); 
                                 pControlPort->portState = CONTROL_PORT_STATE_PERIOD_SAMPLE;                               
                                                      
                             }
                        }
                    }
                    else if(getMeterBoardMode() == GUN_SIGNAL_MODE)                   
                    {
                        pControlPort->portState = CONTROL_PORT_STATE_MAPING;
                    }
                    else if(getMeterBoardMode() == FREE_MODE)
                    {
                      
                        ret = taxControlSamplingGun(port,pControlPort->gPxCurrent);
                             
                         if(ret == E_SUCCESS_TRANSACTION || ret == E_SUCCESS)
                         {                                                    
                             pControlPort->gPxCurrent++;
                             
                             if(pControlPort->gPxCurrent >= pControlPort->gPxTotal)
                             {
                                 pControlPort->gPxCurrent=0; 
                                 
                             }
                         }
                    }                        
                    
                }
                break;
                
                case  CONTROL_PORT_STATE_PERIOD_SAMPLE:
                {
                   
                    static u8 TransNum[2]={0}; //交易笔数
                    static u8 TransGunNo[2]={0};  //最后一笔交易的枪号
                    u8 ret;
                    
                    if(pControlPort->gPxSetState == GPX_PP_STATE_PUTDOWN)  
                    { 
                        ret = taxControlSamplingDev(port,&TransNum[port],&TransGunNo[port]);
                        if(ret==E_SUCCESS)
                        {
                           if(pControlPort->getTsdFlag==1)
                                 pControlPort->getTsdFlag=0;
                           
                           TaxSampleUnLock[port]=0;                   
                           pControlPort->portState = CONTROL_PORT_STATE_PERIOD_IDLE; 
                        }
                    }
                    else
                    {
                         pControlPort->portState = CONTROL_PORT_STATE_PERIOD_IDLE; 
                    }
                                       
                }                    
                break;
                
                case    CONTROL_PORT_STATE_MAX:
                default:
                {
                    printf("!!controlledTaxInterfaceSamplingTask,Something wrong occurs, have to reset\r\n");
                    bsp_DelayMS(100);
                    NVIC_SystemReset();
                }
                break;    
            }
        }
    }    
}
