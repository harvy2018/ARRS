/***************************************************************************************
File name: gasolineTransferProtocal.c
Date: 2016-9-21
Author: GuoxiZhang/Wangwei/GuokuYan
Description:
     This file contain all the interface and default parameter for gasoline transaction application transfer protocol.
     
More:
     This module was redesigned by GuokuYan, based on Guoxi Zhang and Wei Wang's previous design.
***************************************************************************************/
#include "gasolineTransferProtocol.h"
#include "board.h"
#include "bsp_rtc.h"
#include "plcMessager.h"
#include "localTime.h"
#include "gasolineDataStore_Retrieve.h"
#include "fwVersion.h"
#include "log.h"
#include "time.h"
#include "flashmgr.h"	
#include "controlledTaxSampling.h"
#include "meterMessager.h"

#define dbgPrintf	SEGGER_RTT_printf

#define GASOLINE_APP_PACKET_BUFFER_SIZE 1200
static u32 sequenceNo=0;
static FWUpgradeInfo fwUpInfo;
static FWInfo fwInfo;
static u8  AppPacketBuffer[GASOLINE_APP_PACKET_BUFFER_SIZE];
static GasolineAppUploadingStateDes gasolineAppUploadstateDes = {GASOLINE_UPLOAD_STATE_IDLE,0,0,0};
static u8  sum_frame,this_frame,rcv_frame;
static u8  upgradeErr=0;

u8  TimeSyn_Flag=0;
u8  LogUpload_Flag=0;

u32  OTD_Upload_Cnt=0;

ARRSInfo arrsinfo;
/*************************************************************************************,
Function Name: gasolineTransferAPPMessageSend
Description:
   Send application message out.
Parameter:
    msgId:[I], command ID or ack ID
    fs: Total frame number
    fc: Current frame number
    pData: Applicaiton data buffer
    len: Applicaiton data len
Return:
   E_SUCCESS:  Put into uart send buffer successfully and start to send
   E_FAIL:  fails to put into send buffer.
***************************************************************************************/
errorCode gasolineTransferAPPMessageSend(u16 msgId, u8 fs,  u8 fc, u8  *pData, u16 len )
{
    GasolineTransferCmdHeader header;
	
//	#if Concentrator_OS == 1
//	if(msgId==CASOLINE_CMD_CURRENT_TRANSACTION_DATA)
//		len-=20;
//		
//	#endif
	
    u8 *pBuf =(u8 *)malloc(len+sizeof(GasolineTransferCmdHeader) +4);
    u32 crc32;
    u16 dstLen = GASOLINE_APP_PACKET_BUFFER_SIZE;
    
    if(!pBuf)
        return E_NO_MEMORY;
    
    memcpy(header.preamble,GASOLINE_MSG_PREAMBLE,4);
    header.len = len + sizeof(GasolineTransferCmdHeader) + 4;     // payload length + 4(CRC32) + header
    header.msgId = msgId;
    header.sequenceNo = sequenceNo++;
    header.protocolVersion = GASOLINE_PROTOCOL_VERSION;
    header.frame[1] = fs;
    header.frame[0] = fc;
    memcpy(header.boardId,getBoardSerialID(),16);

    memcpy(pBuf,&header,sizeof(GasolineTransferCmdHeader));                // Put the header part into buffer
    memcpy(pBuf+sizeof(GasolineTransferCmdHeader),pData,len);           // Put the applicaiotn payload

    crc32 = CRC_CalcBlockCRC((u32 *)pBuf, header.len-4);
    *(u32 *)(pBuf+header.len-4) = crc32;

    createPLCDataPacket(pBuf, header.len,AppPacketBuffer,&dstLen);
    
    plcWrite(AppPacketBuffer,dstLen);
    
    free(pBuf);
    return E_SUCCESS;
}

/*************************************************************************************,
Function Name: gasolineTransferAPPMessageResp
Description:
   Response receieved message.
Parameter:
    pHead:[I], Response message header pointer, points to the received message header
    fs:[I],Total frame number
    fc:[I],Current frame number
    result:[I], indicate OK or error
Return:
   E_SUCCESS:  Always return sucessfully.
***************************************************************************************/
static errorCode gasolineTransferAPPMessageResp(GasolineTransferCmdHeader *pHead, u8 fs,  u8 fc, u8  result)
{
    GasolineTransferCmdHeader *pRespHeader;
    u32 crc32;
    u8  pRespBuf[128];
    pRespHeader = (GasolineTransferCmdHeader *)pRespBuf;
    u16 dstLen = GASOLINE_APP_PACKET_BUFFER_SIZE;


    memcpy(pRespHeader->preamble,GASOLINE_MSG_PREAMBLE,4);
    pRespHeader->len = sizeof(GasolineTransferCmdHeader) + 4 + 1;       // header length + 4bytes CRC + 1bytes Result
    if((pHead->msgId & 0x8000) == 0x8000)
        pRespHeader->msgId = pHead->msgId & 0x0FFF;
    else
        pRespHeader->msgId = pHead->msgId & 0x1FFF;

    pRespHeader->sequenceNo = pHead->sequenceNo;
    pRespHeader->protocolVersion = GASOLINE_PROTOCOL_VERSION;

    pRespHeader->frame[1] = fs;
    pRespHeader->frame[0] = fc;
   
//    memcpy(pRespHeader->boardId,getBoardSerialID(),16);

    *(pRespBuf+sizeof(GasolineTransferCmdHeader)) = result; 

    crc32 = CRC_CalcBlockCRC((u32 *)pRespBuf, pRespHeader->len-4);
    *(u32 *)(pRespBuf+pRespHeader->len-4) = crc32;
 
    createPLCDataPacket(pRespBuf, pRespHeader->len,AppPacketBuffer,&dstLen);
    
    plcWrite(AppPacketBuffer,dstLen);

    return E_SUCCESS;
}


/*************************************************************************************,
Function Name: gasTransferAppMessageHandler
Description:
   This function intends to handler all the repsonse and message issued by concentrator
Parameter:
    pMsg:[I], UDP packet. need stripe UDP and IP from the message to get the pure applicaiton packet.
Return:
   NULL
***************************************************************************************/
void  gasolineTransferAppMessageHandler(u8 *pMsg)
{
    u32 crcL,crcR;
    u32 rcv_seqNo;
    u8  *pAppPacket;
    GasolineTransferCmdHeader *pAppHeader;
    GasolineAppUploadingStateDes *pDes = &gasolineAppUploadstateDes;
    u8  *pdataBuf=NULL;

    pAppHeader = (GasolineTransferCmdHeader *)pMsg;
    pdataBuf = pMsg + sizeof(GasolineTransferCmdHeader);

    crcL = CRC_CalcBlockCRC((u32 *)pMsg,pAppHeader->len - 4);
    crcR = *((u32 *)(pMsg + pAppHeader->len - 4));

    sum_frame = pAppHeader->frame[1];
    this_frame  = pAppHeader->frame[0];
    
    // Judge the check code
    if(crcL != crcR)
	{
		printf("GasolineTransferCmdHeader: CRC error with MsgID:%x\r\n",pAppHeader->msgId);
		return;
	}
    
    // Judge the sequence Number only for Response Message type.
    if(((pAppHeader->sequenceNo + 1) != sequenceNo) && ((pAppHeader->msgId & 0x8000) != 0))
	{
		printf("gasolineTransferAppMessageHandler: Sequence Number is not expected\r\n");
		return;
	}
        
    switch(pAppHeader->msgId)
        {
            case    GASOLINE_RESP_FIRMWARE_VERSION:
            {
                printf("Firmware Info Resp\r\n");
                if(pDes->state == GASOLINE_UPLOAD_STATE_FW_INFO_WAITING_RESP)
                {
                    pDes->state = GASOLINE_UPLOAD_STATE_CLOCK;
                    pDes->stateRetryCount = 0;
                    pDes->stateTimeoutCount = 0;
                    pDes->stateTotalCount = 0;
                }    
            }
            break;

            case    GASOLINE_RESP_CLOCK_SYNC:
                {
                    printf("Clock sync Resp\r\n");

                   // Configurate local rtc w/ external reference clock. 
                   rtcConfiguration(*((u32 *)pdataBuf) );   //LOCAL_TIME_ZONE_SECOND_OFFSET Need the timezone offest: 8Hours
                   ClockStateSet(CLOCK_GTC);
                   if(pDes->state == GASOLINE_UPLOAD_STATE_CLOCK_WAITING_RESP)
                    {
                        pDes->state = GASOLINE_UPLOAD_STATE_NORMAL; 
                        pDes->stateRetryCount = 0;
                        pDes->stateTimeoutCount = 0;
                        pDes->stateTotalCount = 0;

                    }
                   
                    // 关闭月累查询 
                   //  RetrieveMonthDataSampleFlag();
                    
                }
            break;

            case    GASOLINE_RESP_HEARTBEAT:
                {
                    printf("Heartbeat Resp\r\n");

                    if(pDes->state == GASOLINE_UPLOAD_STATE_HEART_WAITING_RESP)
                        {
                            pDes->state =  GASOLINE_UPLOAD_STATE_NORMAL;                    
                            pDes->stateRetryCount = 0;
                            pDes->stateTimeoutCount = 0;
                            pDes->stateTotalCount = 0;

                        }
                }
            break;

            case    GASOLINE_RESP_LOG_UPLOAD:
                {
                    printf("Log Resp\r\n");
                    if(pDes->state == GASOLINE_UPLOAD_STATE_LOG_WAITING_RESP)
                        {
                            pDes->state =  GASOLINE_UPLOAD_STATE_NORMAL;                    
                            pDes->stateRetryCount = 0;
                            pDes->stateTimeoutCount = 0;
                            pDes->stateTotalCount = 0;
                        } 
                }
            break;

            case    GASOLINE_RESP_TAXINFO:
                {
                    printf("TaxDevInfo Resp\r\n");
                    retrieveTaxDevInfoRecordReadPointer();
                    if(pDes->state == GASOLINE_UPLOAD_STATE_TRANSACTION_WAITING_RESP)
					{
						pDes->state = GASOLINE_UPLOAD_STATE_NORMAL; 
						pDes->stateRetryCount = 0;
						pDes->stateTimeoutCount = 0;
						pDes->stateTotalCount = 0;
					}                                    
                }
            break;
            
            case    GASOLINE_RESP_CURRENT_TRANSACTION_DATA:
                {
                    printf("OTD Data Resp\r\n");
                    if(getMeterBoardMode()==GUN_SIGNAL_MODE)//枪模式 
                      OTD_Upload_Cnt++;
                    
                    retrieveTransactionRecordReadPointer(TaxData);
                    if(pDes->state == GASOLINE_UPLOAD_STATE_TRANSACTION_WAITING_RESP)
					{
						pDes->state = GASOLINE_UPLOAD_STATE_NORMAL; 
						pDes->stateRetryCount = 0;
						pDes->stateTimeoutCount = 0;
						pDes->stateTotalCount = 0;
					}
                }
            break;
                
            case  CASOLINE_RESP_CURRENT_METER_TRANSACTION_DATA:
            {
                   printf("Meter Data Resp\r\n");
                
                   if(getMeterBoardMode()==METER_MODE)//计量模式
                      OTD_Upload_Cnt++;
             
                    retrieveTransactionRecordReadPointer(MeterData);
                    if(pDes->state == GASOLINE_UPLOAD_STATE_TRANSACTION_WAITING_RESP)
					{
						pDes->state = GASOLINE_UPLOAD_STATE_NORMAL; 
						pDes->stateRetryCount = 0;
						pDes->stateTimeoutCount = 0;
						pDes->stateTotalCount = 0;
					}
                
            }            
            break;
            
            case    GASOLINE_MSG_FW_UPGRADE:
                {
                     FlashSectionStruct *pFS;
					 u32 EraseAddr;
                     u8 ppSel;
                     u8 i;
                     pFS = uf_RetrieveFlashSectionInfo(FLASH_SECTION_PINGPONG_SEL);
                     uf_ReadBuffer(pFS->type,&ppSel,pFS->base,1);
                     
                     printf("Notice of FW UPGRADE SUM_F:%d  THIS_F:%d\r\n", sum_frame,this_frame);
                     memcpy(&fwUpInfo,pdataBuf,sizeof(FWUpgradeInfo));

                     printf(" crc_ping:%x  crc_pong:%x\r\n",fwUpInfo.CRC_Ping,fwUpInfo.CRC_Pong);
                     printf(" size_ping:%x size_pong:%x\r\n",fwUpInfo.Size_Ping,fwUpInfo.Size_Pong);
                     printf(" Ver:%d.%d.%d.%d\r\n",fwUpInfo.FW_Rev[0],fwUpInfo.FW_Rev[1],fwUpInfo.FW_Rev[2],fwUpInfo.FW_Rev[3]);
                     
                     rcv_frame = 1;
                     
                     // Below codes to erase the temp target flash sectors
                     pFS= uf_RetrieveFlashSectionInfo(FLASH_SECTION_FW_UPGRADE);
                     EraseAddr=pFS->base;
					
					/*重置flash写指针，用于升级中断后重新进入升级 2017-5-10 15:25:50*/
					 pFS->wptr=pFS->base;
					 
					 for(i=0;i<3;i++)
					 {
					    printf("Erase 64k Block addr=0x%08X\r\n",EraseAddr); 
						uf_EraseBlock(pFS->type,64,EraseAddr);
						EraseAddr+=0x10000;//偏移64K
						bsp_DelayMS(50);
						 						 
						 
					 }

                     // Send response to upstreamer based on the current PINGPONG flag
                     if(ppSel == 0xff)
                        {
                            gasolineTransferAPPMessageResp(pAppHeader,1,1,1);
                        }   
                     else
                        {
                            gasolineTransferAPPMessageResp(pAppHeader,1,1,0);
                        }
                }
            break;

            case    GASOLINE_MSG_FW_TRANSFER:
                {
                    FlashSectionStruct *pFS;
                    
                    printf("FW_TRANSFERING SUM_F:%d THIS_F:%d Exp_F:%d\r\n",sum_frame,this_frame,rcv_frame);
                    
                    pFS = uf_RetrieveFlashSectionInfo(FLASH_SECTION_FW_UPGRADE);
                    uf_WriteBuffer(pFS->type,pdataBuf,pFS->wptr,pAppHeader->len - sizeof(GasolineTransferCmdHeader)-4);
                    pFS->wptr += pAppHeader->len -sizeof(GasolineTransferCmdHeader)- 4;

                    if(rcv_frame < sum_frame)          // receiveing						
                    {
                        if(rcv_frame != this_frame)
                            {
                                gasolineTransferAPPMessageResp(pAppHeader,sum_frame,this_frame,2);
                            }
                        else
                            {
                                rcv_frame++;
                                gasolineTransferAPPMessageResp(pAppHeader,sum_frame,this_frame,0);
                            }
                    }
                    else                                        // Got the last packet
                    {
                        FlashSectionStruct *pFSSel;
                        FlashSectionStruct *pFSDst;
                        u8  ppSel;
                        short cpSectorNo;
                        short i;
                        u32 crc32;
                        u32 newFWSize;
                        u32 newFWCRC;
                        unsigned char *pTempBuf;

                        pTempBuf =(unsigned char *)malloc(FLASH_SECTOR_SIZE);
                        if(pTempBuf == NULL)
                        {
                            printf(" Mallocate buffer fail\r\n");
                            gasolineTransferAPPMessageResp(pAppHeader,sum_frame,this_frame,2);
                            break;
                        }

                        pFSSel=uf_RetrieveFlashSectionInfo(FLASH_SECTION_PINGPONG_SEL);
                        uf_ReadBuffer(pFSSel->type, &ppSel, pFSSel->base,1);
                        if(ppSel == 0xFF)   // current PING
                        {
                            pFSDst=uf_RetrieveFlashSectionInfo(FLASH_SECTION_FW_PONG);
                            newFWSize= fwUpInfo.Size_Pong;
                            newFWCRC=fwUpInfo.CRC_Pong;
                        }
                        else
                        {
                            pFSDst=uf_RetrieveFlashSectionInfo(FLASH_SECTION_FW_PING);
                            newFWSize= fwUpInfo.Size_Ping;
                            newFWCRC=fwUpInfo.CRC_Ping;
                        }
                        
                        // Copy FW binary from external flash to internal flash
                        pFSDst->wptr= pFSDst->base + (FLASH_SECTOR_SIZE >> 1);//第一个扇区用于存储固件信息

                        cpSectorNo = (pFS->wptr - pFS->base + FLASH_SECTOR_SIZE)/FLASH_SECTOR_SIZE;

                        for(i=0;i<cpSectorNo;i++)
                        {
                            uf_ReadBuffer(pFS->type, pTempBuf, pFS->rptr, FLASH_SECTOR_SIZE);
                            uf_EraseSector(pFSDst->type,pFSDst->wptr);
                            uf_WriteBuffer(pFSDst->type, pTempBuf, pFSDst->wptr, FLASH_SECTOR_SIZE);

                            pFS->rptr += FLASH_SECTOR_SIZE;
                            pFSDst->wptr+= FLASH_SECTOR_SIZE;
                        }

                        crc32 = CRC_CalcBlockCRC((unsigned int *)(pFSDst->base+(FLASH_SECTOR_SIZE >> 1)), newFWSize);
                        if(crc32 == newFWCRC)
                        {
                            u8 ppTarget = ~ppSel; 

                            memcpy(fwInfo.MagicCode,"firm",4);
                            memcpy(fwInfo.MajorVersion,fwUpInfo.FW_Rev,4);
                            fwInfo.Size=newFWSize;
                            fwInfo.Crc=crc32;	

                            // Write the descriptor to the 1st sector of PING/PONG area
                            uf_EraseSector(pFSDst->type,pFSDst->base);
                            uf_WriteBuffer(pFSDst->type, (u8 *)&fwInfo, pFSDst->base, sizeof(FWInfo));
                            
                            if(ppSel == 0xff)
                                {
                                    uf_WriteBuffer(pFSSel->type, &ppTarget, pFSSel->base, 1);
                                }
                            else    // erase the sector
                                {
                                    uf_EraseSector(pFSSel->type, pFSSel->base);
                                }
                            // Send back successful response 
                            gasolineTransferAPPMessageResp(pAppHeader,sum_frame,this_frame,0);    

                            printf(" UPdate successfully, Reset CPU......\r\n");
                            
                            bsp_DelayMS(1000);
                            NVIC_SystemReset();                                    
                        }
                        else
                        {
                            printf("gasolineTransferAppMessageHandler: CRC error of Writeback to Target flash\r\n");
                            gasolineTransferAPPMessageResp(pAppHeader,sum_frame,this_frame,2);    
                        }                            
                        free(pTempBuf);
                    }
                }
            break;

            case    GASOLINE_MSG_CONTROL:
                {
                    u32 subCommand;
                    subCommand = *(u32 *)pdataBuf;
                    if(subCommand == 0xffff)
					{
						printf("gasolineTransferAppMessageHandler: Reset CPU following the command\r\n");
						NVIC_SystemReset();
					}
					else if(subCommand == 0xfffc)
					{
						uf_EraseChip(FLASH_SST25VF016B);
						bsp_DelayMS(1000);
						printf("gasolineTransferAppMessageHandler: Erased the external flash\r\n");						
						printf("gasolineTransferAppMessageHandler: Reset CPU, waiting .......\r\n");
						NVIC_SystemReset();
						
					}
					else
					{
						printf("gasolineTransferAppMessageHandler:Control sub-command not supported: %x\r\n",subCommand);
					}
					
                    gasolineTransferAPPMessageResp(pAppHeader,sum_frame,this_frame,0);
                }
            break;

            case   GASOLINE_MSG_PLC_MODULATION_REQUEST:
                {
                    u32 mode = plcGetModulationMode();
                    gasolineTransferAPPMessageSend(GASOLINE_ACK_PLC_MODULATION_REQUEST, 1, 1, (u8 *)&mode,4);
                    printf("gasolineTransferAppMessageHandler: Response TX Modulation Mode Query:%d\r\n",plcGetModulationMode());                 
                }
            break;


            case   GASOLINE_MSG_PLC_MODULATION_CONFIG:
                {
                    u32 mode;
                    mode =*(u32 *)pdataBuf;
                    printf("gasolineTransferAppMessageHandler: Config TX Modulation mode: %d\r\n",mode);
                    gasolineTransferAPPMessageResp(pAppHeader,sum_frame,this_frame,0);
                    plcStoreModulationMode(mode);
                }
            break;    

            default:
                printf("Message ID = %x\r\n",pAppHeader->msgId);
            break;
        }
  
}
/*************************************************************************************,
Function Name: gasolineTrasferAppProtocolInitialize
Description:
   Initialize this module, for example, intall callback funciton for plcMessager to handler app layer protocol.
Parameter:
    NULL
Return:
   NULL
***************************************************************************************/
void gasolineTrasferAppProtocolInitialize()
{
    plcMessagerInstallAppProtocolHandler(gasolineTransferAppMessageHandler);
}


errorCode gasolineAppUploadingTask()
{
    GasolineAppUploadingStateDes *pDes = &gasolineAppUploadstateDes;
    
    
	switch(pDes->state)
        {
            case GASOLINE_UPLOAD_STATE_IDLE:    // Waiting until upload network is ready
                {
                    if(plcMessagerGetChannelState() == PLC_MSG_STATE_NETWORK)
                    {
                        pDes->state = GASOLINE_UPLOAD_STATE_DELAY;
                        pDes->stateTimeoutCount = 0;
                        pDes->stateTotalCount = 0;
                        pDes->stateRetryCount = 0;
                        printf("gasolineAppUploadingTask: Change to PLC_DELAY state\r\n");
                    }
                }   
            break;

            case GASOLINE_UPLOAD_STATE_DELAY: // 建立连接后等待一定时间后再发送数据
                {
                    pDes->stateTotalCount++;
                    if( pDes->stateTotalCount==6)
                        {
                            pDes->state = GASOLINE_UPLOAD_STATE_FW_INFO;
                            pDes->stateTotalCount = 0;
                            printf("gasolineAppUploadingTask:Change to FW_INFO state\r\n");
                        }
                    printf("gasolineAppUploadingTask: PLC_DELAY state\r\n");                   
                }  
            break;
            
            case GASOLINE_UPLOAD_STATE_FW_INFO: // Send FW info to Concentrator
                {
                   
                    printf("gasolineAppUploadingTask:Report FW version\r\n");
                    
                    memcpy(arrsinfo.FW_Ver,getFwVersion(),4);
                    memcpy(arrsinfo.HWID,getBoardSerialID(),16);
                   
                   // if(getMeter422Mode()==1 || getMeterBoardMode() == METER_MODE) 
                    if(getMeterBoardMode() == METER_MODE) 
                      arrsinfo.ARRS_WorkMode=2;   //计量
                    else if(getMeterBoardMode() == GUN_SIGNAL_MODE)
                      arrsinfo.ARRS_WorkMode=1;   //枪信号抄报
                    else if(getMeterBoardMode() == FREE_MODE)
                      arrsinfo.ARRS_WorkMode=0;  //自由抄报
                    
                    
                    gasolineTransferAPPMessageSend(GASOLINE_CMD_FIRMWARE_VERSION, 1, 1, (u8*)&arrsinfo,sizeof(ARRSInfo));
                    pDes->state = GASOLINE_UPLOAD_STATE_FW_INFO_WAITING_RESP;
                }  
            break;

            case GASOLINE_UPLOAD_STATE_CLOCK:   // Sync clocking with Concentrator
                {
                    u8 c[4];
                    printf("gasolineAppUploadingTask:Request clock sync\r\n");
                    gasolineTransferAPPMessageSend(GASOLINE_CMD_CLOCK_SYNC,1, 1, c, 4);
                    pDes->state = GASOLINE_UPLOAD_STATE_CLOCK_WAITING_RESP;
                    pDes->stateTimeoutCount = 0;
                    pDes->stateTotalCount = 0;
                }
            break;   

    
            case GASOLINE_UPLOAD_STATE_NORMAL: // Actaully, this is normally working state
                {
                    static unsigned char ppFlag=0;
                                    
                    TaxOnceTransactionRecord record;
                    MeterOnceTransactionRecord  mrecord;                 
                    TaxDevInfo  taxInfoRecord;
                         
                    pDes->stateTotalCount++;
                                             
                    if(retrieveTaxDevInfoRecord(&taxInfoRecord) == E_SUCCESS)
                    {
                        printf("gasolineAppUploadingTask: Report Tax Dev Info Record @%x\r\n",getLocalTick());
                        taxInfoRecord.content.timeStamp = getLocalTick();
                        gasolineTransferAPPMessageSend(GASOLINE_CMD_TAXINFO,1,1,(u8 *)&taxInfoRecord.content,sizeof(TaxDevInfoContent));
                        pDes->state = GASOLINE_UPLOAD_STATE_TRANSACTION_WAITING_RESP;
                        pDes->stateTimeoutCount = 0;
                    }
                    else if(retrieveTransactionRecord(&record,TaxData) == E_SUCCESS)
                    {
                        TaxTraSendContent  TaxSendRecord;
                        
                        printf("gasolineAppUploadingTask:Report TaxTraction Record\r\n");
                        
                        MakeTraDataToSend(&record,&TaxSendRecord,TaxData);
                            
                        gasolineTransferAPPMessageSend(CASOLINE_CMD_CURRENT_TRANSACTION_DATA,1,1,(u8 *)&TaxSendRecord,sizeof(TaxSendRecord));
                        pDes->state = GASOLINE_UPLOAD_STATE_TRANSACTION_WAITING_RESP;
                        pDes->stateTimeoutCount = 0;
                    }
                    else if(retrieveTransactionRecord(&mrecord,MeterData) == E_SUCCESS)
                    {
                        MeterTraSendContent  MeterSendRecord;
                        
                        printf("gasolineAppUploadingTask:Report MeterTraction Record\r\n");
                        
                        MakeTraDataToSend(&mrecord,&MeterSendRecord,MeterData);
                        
                        gasolineTransferAPPMessageSend(CASOLINE_CMD_CURRENT_METER_TRANSACTION_DATA,1,1,(u8 *)&MeterSendRecord,sizeof(MeterSendRecord));
                        pDes->state = GASOLINE_UPLOAD_STATE_TRANSACTION_WAITING_RESP;
                        pDes->stateTimeoutCount = 0;
                    }
                    
                    
                    if(LogUpload_Flag==1)  
                    {
                        LogUpload_Flag=0;
                        
                        if(TimeSyn_Flag==1)//每6个小时与集中器同步一次时间
                        {
                            TimeSyn_Flag=0;
                            pDes->state = GASOLINE_UPLOAD_STATE_CLOCK;
                            pDes->stateRetryCount = 0;
                            pDes->stateTimeoutCount = 0;
                            pDes->stateTotalCount = 0;
                        }
                        // else if(ppFlag == 0)
//                        {
//                                unsigned char t[4];
//                                getBoardTemperature(&t[0],&t[1],(unsigned short *)&t[2]);                    
//                                printf("gasolineAppUploadingTask:Report Heartbeat: %d\r\n",t[1]);
//                                gasolineTransferAPPMessageSend(GASOLINE_CMD_TEMPERATURE_DATA,1, 1,t, 4);

//                                pDes->state = GASOLINE_UPLOAD_STATE_HEART_WAITING_RESP;
//                                pDes->stateTimeoutCount = 0;
//                                pDes->stateTotalCount = 0;
//                                ppFlag = 1;
//                        }
                        else
                        {
                                unsigned char *pLog;
                               // unsigned short size;
                                extern  u16 LogDataLEN;   
                            
                            #if Concentrator_OS == 0
                                logTask2(); // Logging task
                                pLog=logGetBuffer();
                                LogDataLEN=strlen(pLog);                              
                            #else
                                logTask();
                                pLog=logGetBuffer();
                            #endif
                                                       
                              //  size = strlen(pLog);
                                
                                gasolineTransferAPPMessageSend(GASOLINE_CMD_LOG_UPLOAD,1, 1,pLog, LogDataLEN);
                                printf("gasolineAppUploadingTask:Report log\r\n");

                                pDes->state = GASOLINE_UPLOAD_STATE_LOG_WAITING_RESP;
                                //pDes->state =GASOLINE_UPLOAD_STATE_NORMAL;
                                pDes->stateTimeoutCount = 0;
                                pDes->stateTotalCount = 0;
                                ppFlag = 0;
                         }
                    
                    }
                        
                     
             }
            
	     break;
				
            case GASOLINE_UPLOAD_STATE_FW_INFO_WAITING_RESP:
                {
                    pDes->stateTimeoutCount++;
                    pDes->stateTotalCount++;
                    
                    if(pDes->stateTimeoutCount >= 6)
                    {
                        pDes->state = GASOLINE_UPLOAD_STATE_FW_INFO;
                        pDes->stateTimeoutCount = 0;
                        pDes->stateRetryCount++;
                        if(pDes->stateRetryCount >= 5)
                        {
                            printf("gasolineAppUploadingTask:Reset PLC via FW_INFO Resp timeout\r\n");
                            pDes->state = GASOLINE_UPLOAD_STATE_IDLE;
                            plcMessagerReset();
                        }
                    }   
                }
            break; 

            case GASOLINE_UPLOAD_STATE_CLOCK_WAITING_RESP:
                {
                    pDes->stateTimeoutCount++;
                    pDes->stateTotalCount++;
              
                    if(pDes->stateTimeoutCount >= 30)
                    {
                        pDes->state = GASOLINE_UPLOAD_STATE_CLOCK;
                        pDes->stateTimeoutCount = 0;
                        pDes->stateRetryCount++;
                        if(pDes->stateRetryCount >= 3)
                        {
                            printf("gasolineAppUploadingTask:Reset PLC via CLock_Sync  Resp timeout\r\n");
                            pDes->state = GASOLINE_UPLOAD_STATE_IDLE;
                            plcMessagerReset();
                        }

                    }   
                }
            break; 
            
            case GASOLINE_UPLOAD_STATE_TRANSACTION_WAITING_RESP:
               {
                    pDes->stateTimeoutCount++;
                    pDes->stateTotalCount++;
                    
                    if(pDes->stateTimeoutCount >= 30)
                    {
                        pDes->state = GASOLINE_UPLOAD_STATE_NORMAL;
                        pDes->stateTimeoutCount = 0;
                        pDes->stateRetryCount++;
                        if(pDes->stateRetryCount >= 3)
                        {
                            printf("gasolineAppUploadingTask:Reset PLC via Transaction Record  Resp timeout\r\n");
                            pDes->state = GASOLINE_UPLOAD_STATE_IDLE;
                            plcMessagerReset();
                        }                            
                    }   
                }
            break;    

            case GASOLINE_UPLOAD_STATE_HEART_WAITING_RESP:
               {
                    pDes->stateTimeoutCount++;
                    pDes->stateTotalCount++;
                    
                    if(pDes->stateTimeoutCount >= 30)
                        {
                            pDes->state = GASOLINE_UPLOAD_STATE_NORMAL;
                            pDes->stateTimeoutCount = 0;
                            pDes->stateRetryCount++;
                            if(pDes->stateRetryCount >= 3)
                                {
                                    printf("gasolineAppUploadingTask:Reset PLC via Heartbeat Resp timeout\r\n");
                                    pDes->state = GASOLINE_UPLOAD_STATE_IDLE;
                                    plcMessagerReset();
                                }
                        }   
                }
            break;

            case    GASOLINE_UPLOAD_STATE_LOG_WAITING_RESP:
                {
                    pDes->stateTimeoutCount++;
                    pDes->stateTotalCount++;
                    
                    if(pDes->stateTimeoutCount >= 15)
                        {
                            pDes->state = GASOLINE_UPLOAD_STATE_NORMAL;
                            pDes->stateTimeoutCount = 0;
                            pDes->stateRetryCount++;
                            if(pDes->stateRetryCount >= 3)
                                {
                                    printf("gasolineAppUploadingTask:Reset PLC via Heartbeat Resp timeout\r\n");
                                    pDes->state = GASOLINE_UPLOAD_STATE_IDLE;
                                    plcMessagerReset();
                                }
                        }   

                }
            break;    
            
            default:
                printf("gasolineAppUploadingTask: Wrong state:%d\r\n",pDes->state);    
            break;    
        }
    return E_SUCCESS;
}


