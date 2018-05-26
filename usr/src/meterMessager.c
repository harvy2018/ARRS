/***************************************************************************************
File name: meterMessager.c
Date: 2016-11-11
Author: Guoku Yan
Description:
     This file contain all the interface for meter monitoring port to get transaction record and  "pick gun" event  and
"put gun" event, trigger tax control device to do controlled sampling,etc..
***************************************************************************************/
#include <string.h>
#include <stdlib.h>
#include "stm32f10x.h"
#include "uartPort.h"
#include "localTime.h"
#include "meterMessager.h"
#include "flashMgr.h"
#include "gasolineDataStore_Retrieve.h"
#include "controlledTaxSampling.h"
#include "board.h"
#include "bsp_i2c.h"

#define dbgPrintf	SEGGER_RTT_printf

static MeterMsgRcvDes   meterMsgRcvDes[2]={{0,METER_MSG_RCV_EMPTY,{0},{0},0},{0,METER_MSG_RCV_EMPTY,{0},{0},0}};
static  unsigned char msgSeq=0;
GunStatusManager gunstatusmgr[TAX_CONTROL_DEVICE_MAX_NUM]={{{GUN_STATE_PUT},255,METER_STATE_INVALID,{0}},{{GUN_STATE_PUT},255,METER_STATE_INVALID,{0}}};


MeterTransactionDataGroup MeterDataGroup[TAX_CONTROL_DEVICE_MAX_NUM]={0};


// We will apply METER mode for ever
MeterModeState MeterMode = METER_MODE_ON;
u8 Meter422Mode= 0;



typedef   enum  _I2CSlaveMeterMsgState
{
    I2C_SLAVE_METER_MSG_STATE_IDLE = 0,
    I2C_SLAVE_METER_MSG_STATE_RX,
    I2C_SLAVE_METER_MSG_STATE_TX,
    I2C_SLAVE_METER_MSG_STATE_MAX,
}I2CSlaveMeterMsgState;


typedef enum  _I2CSlaveRxState
{
    I2C_SLAVE_RX_INIT = 0,
    I2C_SLAVE_RX_PAYLOAD,
    I2C_SLAVE_RX_COMPLETE,
    I2C_SLAVE_RX_MAX
}I2CSlaveRxState;


static I2CSlaveMeterMsgState i2cState = I2C_SLAVE_METER_MSG_STATE_IDLE;
static I2CSlaveRxState i2cRxState = I2C_SLAVE_RX_INIT;

static u8  tBuf[I2C_LOCAL_BUFFER_SIZE];
static u8   rptr=0;
static u8   expSize = 0;
static u8   cc = 0;
static MeterI2cComResp   ackStatus = METER_I2C_COM_RESPONSE_FAIL;

static void messengerI2CSlaveRxProcessing(u8 c)    
{
    switch  (i2cRxState)
    {
        case    I2C_SLAVE_RX_INIT:
        {
            tBuf[rptr++] = c;     // put the data into temp buffer, and rptr++
            if(rptr >= I2C_LOCAL_BUFFER_SIZE)
                rptr = 0;       // must be something wrong, have to reset rptr.
            if(rptr == 3)
            {
                if((tBuf[0] == 'd') && (tBuf[1] == 'x') && (tBuf[2] <I2C_LOCAL_BUFFER_SIZE -3)) 
                {   // pass the sanity checking
                    expSize = tBuf[2];
                    cc = expSize;
                    i2cRxState = I2C_SLAVE_RX_PAYLOAD;
                }
                else
                {   // preamble error or len error, shift one byte left;
                    tBuf[0] =tBuf[1];
                    tBuf[1] = tBuf[2];
                    rptr--;
                } 
            }
        }
        break;

        case    I2C_SLAVE_RX_PAYLOAD:
        {
             cc = cc ^ c; 
             tBuf[rptr++] = c;
             expSize--;
             //printf("payload:%x rem:%d\n",c,expSize);
             if(expSize == 0)
             {
                if (cc == 0)        // Passed CC checking 
                {
                    ackStatus = METER_I2C_COM_RESPONSE_OK;
                  //  printf("good\n");
                    uartPutCharsIntoRcvBuffer(UART_METER_PORT_BASE,tBuf,rptr);
                  
                }
                else                // CC error
                {
                  //  printf("NG!!\n");
                    ackStatus = METER_I2C_COM_RESPONSE_FAIL;
                }

                rptr = 0;
                i2cRxState = I2C_SLAVE_RX_INIT;
                i2cState = I2C_SLAVE_METER_MSG_STATE_IDLE;
            }
        }
        break;

        case    I2C_SLAVE_RX_COMPLETE:
        default:    
        break;          
    }
}

/**************************************************************************************
Function Name: messengerI2CSlaveHandler
Description:
     This function is the I2C event interrupt handler to process the Slave receiver mode and slave transmiter mode
together, with this handler, slave side works w/ pure interrupt mode that can receive the message from the partner
and also response the result by global "ackStatus".
***************************************************************************************/
void messengerI2CSlaveHandler()
{
    extern u8 MGBoardTest;
    static u8 databuf[20]={0};
    static u8 datatemp[20]={0};
    
    __IO u32 SR1,SR2,SR;

    SR1 = I2C1->SR1;
    SR2 = I2C1->SR2;
    SR = ((SR2 << 16) | SR1) & 0x00ffffff;

     if((SR & I2C_EVENT_SLAVE_TRANSMITTER_ADDRESS_MATCHED)==I2C_EVENT_SLAVE_TRANSMITTER_ADDRESS_MATCHED)
        {   //   ST mode:    EV1  Occured and EV1
            I2C1->DR = ackStatus;
            ackStatus = METER_I2C_COM_RESPONSE_FAIL;
            //printf("stm1\n");
        }

     if((SR & I2C_EVENT_SLAVE_RECEIVER_ADDRESS_MATCHED)==I2C_EVENT_SLAVE_RECEIVER_ADDRESS_MATCHED)
        {
            //printf("srm2\n");
            rptr = 0;
            ackStatus = METER_I2C_COM_RESPONSE_FAIL;
        }

      // ST Mode: EV3_2, acknowledge failure, clear  
     if((SR & I2C_EVENT_SLAVE_ACK_FAILURE) ==I2C_EVENT_SLAVE_ACK_FAILURE)
        {   
            I2C_ClearFlag(I2C1,I2C_FLAG_AF); 
        }

    if((SR & I2C_EVENT_SLAVE_BYTE_RECEIVED) == I2C_EVENT_SLAVE_BYTE_RECEIVED)
    {
        __IO u8 c = I2C1->DR;                                                           // Read the data in.
        uartPutCharIntoRcvBuffer(UART_METER_PORT_BASE,c);   // store the data into buffer 
        rptr++;
        
        if(MGBoardTest==0)
        {
            if(rptr == 3)   // Get the packet length 
            {
                expSize = c;            // packet lentgh
                cc = c;
            }    

            if(rptr >3)                  // calculate cc
            {
                cc = cc ^ c;
                expSize--;

                if(expSize == 0)
                {
                    if(cc == 0) ackStatus = METER_I2C_COM_RESPONSE_OK;
                    else ackStatus = METER_I2C_COM_RESPONSE_FAIL;                                
                }
            }  
        } 
        else if(MGBoardTest==1)
        {
           databuf[rptr-1]=c;
           if(rptr==18 && databuf[rptr-2]=='T') 
           {
               if(!memcmp(databuf,"DXYS_ARRS_COMTEST",17))
               {
                  ackStatus = METER_I2C_COM_RESPONSE_OK; 
                  printf("计量口:%d测试通过!\r\n",databuf[rptr-1]); 
               }
               else
               {
                    printf("计量口:%d测试没通过!\r\n",databuf[rptr-1]); 
                   ackStatus =METER_I2C_COM_RESPONSE_FAIL;  
               }
           } 
           
        }
        else if(MGBoardTest==2) 
        {
           databuf[rptr-1]=c;
           if(rptr==10) 
           {
               if(!memcmp(databuf+6,"GUNS",4))
               {
                 
                  for(u8 i=0;i<6;i++)
                  {
                      if(databuf[i]==1 && databuf[i]!=datatemp[i])
                          printf("枪信号:%d 状态:挂枪! 请进行摘枪操作!\r\n",i);
                      else if(databuf[i]==0 && databuf[i]!=datatemp[i])
                          printf("枪信号:%d 状态:摘枪! 请进行挂枪操作!\r\n",i);  
                      else if(databuf[i]!=datatemp[i])
                          printf("枪信号:%d 状态:%d\r\n",i,databuf[i]);  

                     datatemp[i]=databuf[i];
                  }

                     
                  
               }
               
           } 
           
        }            
            
    }
    
    

    if((SR & I2C_EVENT_SLAVE_STOP_DETECTED) == I2C_EVENT_SLAVE_STOP_DETECTED)
    {
        I2C1->CR1 |= 0x0001;
    }


}
/**************************************************************************************
Function Name: meterAck
Description:
     This function intends to acknowledge the partner that we already received the message.
Parameter:
    port:[I], meter uart No;
    Seq:[I], sequence No, attention this # must be same as the received one.
    res:[I],  METER_ACK_SUCCESS: succeed  METER_ACK_FAIL: Fail
Return:
    Always E_SUCCESS.
**************************************************************************************/
static errorCode meterAck(unsigned char  port,unsigned char seq,MeterAckRes res)
{
//    MeterAck ack;
//    unsigned char cc=0;
//    
//    ack.head.preamble[0]='d';
//    ack.head.preamble[1]='x',
//    ack.head.len = sizeof(MeterAck)-3;
//    ack.head.msg = METER_ACK_ID;
//    ack.head.seqNo = seq;
//    ack.res = res;

//    cc = cc ^ ack.head.len;
//    cc = cc ^ ack.head.version;
//    cc = cc ^ ack.head.msg;
//    cc = cc ^ ack.head.seqNo;
//    cc = cc ^ ack.res;
//    ack.cc = cc;
//    
//    
//    #ifndef USE_I2C_FOR_METERBORAD
//      uartWriteBuffer(port+UART_METER_PORT_BASE, (unsigned char *)&ack, sizeof(MeterAck));         
//      uartSendBufferOut(port+UART_METER_PORT_BASE);
//    #endif
    
    return E_SUCCESS;  
    
}

/**************************************************************************************
Function Name: meterMsgProcess
Description:
     This function intends to print out the received message.
Parameter:
    pDes:[I], meter message descriptor;
Return:
    Always E_SUCCESS.
**************************************************************************************/
static void  meterMsgProcess(MeterMsgRcvDes *pDes)
{
    ControlPortEventHandler((unsigned char *)pDes);
}

/**************************************************************************************
Function Name: meterMessagerRetriever
Description:
     Pick out a complete message from uart buffer.
Parameter:
    port:[I], meter uart port.
    pDes:[I/O], meter message descriptor;
Return:
    Always E_SUCCESS.
**************************************************************************************/
static MeterMessageRcvState meterMessagerRetriever(unsigned char port, MeterMsgRcvDes *pDes)
{
    unsigned short  availBytes=0;
    short hL;
    short rN,rL;         // remaining needed, remaining len
    unsigned char i;
    unsigned char cc=0;
	
								
    hL = sizeof(MeterHeader);
   
    availBytes = uartGetAvailBufferedDataNum(port+UART_METER_PORT_BASE);
   
    switch(pDes[port].state)
        {
            case    METER_MSG_RCV_EMPTY:
                {
                    if(availBytes >= hL)
                        {
                           uartRead(port+UART_METER_PORT_BASE, (unsigned char *)&pDes->head, hL);
                           if(pDes->head.preamble[0]=='d' && pDes->head.preamble[1] =='x')
                            {
                                   if(((pDes->head.msg == METER_MSG_STATUS) && (pDes->head.len == (sizeof(MeterMsgStatus)-3)))  ||
                                    ((pDes->head.msg == METER_MSG_TRANSACTION) && (pDes->head.len == (sizeof(MeterMsgTransaction)-3))) ||
                                    ((pDes->head.msg == METER_RESP_STATUS) && (pDes->head.len == (sizeof(MeterResp)-3))) ||
                                    ((pDes->head.msg == METER_ACK_ID) && (pDes->head.len == (sizeof(MeterAck)-3))))
                                    {
                                        rL = availBytes - hL;
                                        rN= pDes->head.len - 3;
                                        if(rL >= rN)
                                        {
                                            uartRead(port+UART_METER_PORT_BASE, pDes->content, rN-1);
                                            uartRead(port+UART_METER_PORT_BASE, &pDes->cc, 1);
                                            cc = cc ^ pDes->head.len;
                                            cc = cc ^ pDes->head.msg;
                                            cc = cc ^ pDes->head.seqNo;
                                            cc = cc ^ pDes->head.version;

                                            for(i=0;i<pDes->head.len-4;i++)
                                                cc = cc ^ pDes->content[i];

                                             if(cc == pDes->cc)
                                                {
                                                    if(pDes->head.msg!=METER_RESP_STATUS)
                                                    meterAck(port,pDes->head.seqNo,METER_ACK_SUCCESS);													
                                                    pDes[port].state = METER_MSG_RCV_EMPTY;
                                                    return  METER_MSG_RCV_COMPLETE;
                                                }
                                             else
                                                {
                                                    if(pDes->head.msg!=METER_RESP_STATUS)
                                                    meterAck(port,pDes->head.seqNo,METER_ACK_FAIL);
                                                    printf("meterMessagerRetriever:cc error\r\n");
                                                    return METER_MSG_RCV_EMPTY;
                                                }
                                        }
                                        else
                                        {
                                            pDes->waitCount=0;
                                            pDes[port].state = METER_MSG_RCV_POSTHEAD;
                                           
                                            return METER_MSG_RCV_POSTHEAD;
                                        }
                                    }
                                else
                                {   // Here we can't ack since we are not sure the sequence No is correct or not, just emptify the buffer and go back to "EMPTY" state
                                    //uartEmptyRcvBuffer(port + UART_METER_PORT_BASE);
                                    printf("meterMessagerRetriever: msg or len error: msg:%x  len:%x\r\n",pDes->head.msg,pDes->head.len);
                                    return  METER_MSG_RCV_EMPTY;
                                }                                   
                            }
                           else
                            {
                                printf("meterMessagerRetriever: Preamble error:%x %x\r\n",pDes->head.preamble[0],pDes->head.preamble[1]);
                                pDes[port].state = METER_MSG_RCV_PREHEAD;
                                return METER_MSG_RCV_PREHEAD;
                            }                          
                        }
                    else
                        {
                            return  METER_MSG_RCV_EMPTY;
                        }
                }
            break;
 
            // We already received header-sized data in descriptor, but it is not correct, so, we need shift byte per byte based on preamble to look for
            // next anchor.
            case   METER_MSG_RCV_PREHEAD:
                {
                    int remain=availBytes;
                    unsigned char t;
                    unsigned char *pH;
                    int k=0;
                    int used=0;
 
                    while(remain > 0)
                        {
                            uartRead(port+UART_METER_PORT_BASE,&t,1);
                            used++;
                           
                            pH=(unsigned char *)&pDes->head;
                           
                            // Sequentially shift left one byte out.
                            for(k=0;k<sizeof(MeterHeader)-1;k++)
                                *(pH+k) = *(pH+k+1);
 
                            *(pH+k) = t;
                            
                            // Already shifted one byte, then we need check it again
                            if(pDes->head.preamble[0]=='d' && pDes->head.preamble[1] =='x')
                                {
                                    if(((pDes->head.msg == METER_MSG_STATUS) && (pDes->head.len == (sizeof(MeterMsgStatus)-3)))  ||
                                    ((pDes->head.msg == METER_MSG_TRANSACTION) && (pDes->head.len == (sizeof(MeterMsgTransaction)-3))) ||
                                    ((pDes->head.msg == METER_RESP_STATUS) && (pDes->head.len == (sizeof(MeterResp)-3))) ||
                                    ((pDes->head.msg == METER_ACK_ID) && (pDes->head.len == (sizeof(MeterAck)-3))))
                                     {
                                            rL = remain - used;    // changed
                                            rN= pDes->head.len -3;
                                            if(rL >= rN)
                                                {
                                                    uartRead(port+UART_METER_PORT_BASE, pDes->content, rN-1);
                                                    uartRead(port+UART_METER_PORT_BASE, &pDes->cc, 1);
                                                    cc = cc ^ pDes->head.len;
                                                    cc = cc ^ pDes->head.msg;
                                                    cc = cc ^ pDes->head.seqNo;
                                                    cc = cc ^ pDes->head.version;
 
                                                    for(i=0;i<pDes->head.len-4;i++)
                                                        cc = cc ^ pDes->content[i];
 
                                                     if(cc == pDes->cc)
                                                        {
                                                            if(pDes->head.msg!=METER_RESP_STATUS)
                                                            meterAck(port,pDes->head.seqNo,METER_ACK_SUCCESS);														
                                                            pDes[port].state = METER_MSG_RCV_EMPTY;
                                                            return  METER_MSG_RCV_COMPLETE;
                                                        }
                                                     else
                                                        {
                                                            printf("meterMessagerRetriever:cc error\r\n");
                                                            if(pDes->head.msg!=METER_RESP_STATUS)
                                                            meterAck(port,pDes->head.seqNo,METER_ACK_FAIL);
															
                                                            pDes[port].state = METER_MSG_RCV_EMPTY;
                                                            return METER_MSG_RCV_EMPTY;
                                                        }
                                                }
                                            else
                                                {
                                                    pDes->waitCount=0;
                                                    pDes[port].state = METER_MSG_RCV_POSTHEAD;
                                                    return METER_MSG_RCV_POSTHEAD;
                                                }
                                        }
                                    else
                                        {
                                            printf("meterMessagerRetriever: msg or len error: msg:%x  len:%x\r\n",pDes->head.msg,pDes->head.len);
                                            pDes[port].state = METER_MSG_RCV_EMPTY;
                                            return  METER_MSG_RCV_EMPTY;  // Changed
                                        }                                   
                                }                          
 
                          remain--; 
                        }                   
                }
            break;
 
            case   METER_MSG_RCV_POSTHEAD:
                {
                    pDes->waitCount++;
                   
                    rL = availBytes;
                    rN= pDes->head.len -3;
                    if(rL >= rN)
                        {
                                uartRead(port+UART_METER_PORT_BASE, pDes->content, rN-1);
                                uartRead(port+UART_METER_PORT_BASE, &pDes->cc, 1);
                                cc = cc ^ pDes->head.len;
                                cc = cc ^ pDes->head.msg;
                                cc = cc ^ pDes->head.seqNo;
                                cc = cc ^ pDes->head.version;
     
                                for(i=0;i<pDes->head.len-4;i++)
                                    cc = cc ^ pDes->content[i];
     
                                if(cc == pDes->cc)
                                    {
                                       if(pDes->head.msg!=METER_RESP_STATUS)
                                        meterAck(port,pDes->head.seqNo,METER_ACK_SUCCESS);
                                        pDes[port].state = METER_MSG_RCV_EMPTY;
                                        return  METER_MSG_RCV_COMPLETE;
                                    }
                                else
                                    {
                                       if(pDes->head.msg!=METER_RESP_STATUS)
                                        meterAck(port,pDes->head.seqNo,METER_ACK_FAIL);
                                       
                                        printf("meterMessagerRetriever:cc error\r\n");
                                        pDes[port].state  = METER_MSG_RCV_EMPTY;
                                        return METER_MSG_RCV_EMPTY;
                                    }
                        }
                    else
                        {
                            pDes->waitCount=0;
                            if(pDes->head.msg!=METER_RESP_STATUS)
                            meterAck(port,pDes->head.seqNo,METER_ACK_FAIL);
                            
                            printf("meterMessagerRetriever:Timeout\r\n");
                            pDes[port].state = METER_MSG_RCV_EMPTY;
                            return METER_MSG_RCV_EMPTY;
                        }                   
                }
            break;
 
            case   METER_MSG_RCV_COMPLETE:
                {
                    printf("meterMessagerRetriever:Complete!\r\n");
                    pDes[port].state = METER_MSG_RCV_EMPTY;
                }
            break;
 
            default:
            break;   
        }
                  
}

/**************************************************************************************
Function Name: meterSendQueryCmd
Description:
     Send query gun set status request to meter device.
Parameter:
    port:[I], meter uart port.
Return:
    Always E_SUCCESS.
**************************************************************************************/
errorCode meterSendQueryCmd(unsigned char  port)
{
    MeterCmd cmd;
    unsigned char cc=0;

    cmd.head.preamble[0]='d';
    cmd.head.preamble[1]='x',
    cmd.head.len = sizeof(MeterCmd)-3;
    cmd.head.msg = METER_CMD_STATUS;
    cmd.head.seqNo = msgSeq++;
    cmd.port = port;

    cc = cc ^ cmd.head.len;
    cc = cc ^ cmd.head.version;
    cc = cc ^ cmd.head.msg;
    cc = cc ^ cmd.head.seqNo;
    cc = cc ^ cmd.port;
	
	cmd.cc=cc;
    
    #ifndef USE_I2C_FOR_METERBORAD
    
      uartWriteBuffer(UART_METER_PORT_BASE, (unsigned char *)&cmd, sizeof(MeterCmd));  
      uartSendBufferOut(UART_METER_PORT_BASE);
      
    #endif
    
    return E_SUCCESS;
}

//#if Concentrator_OS == 0
#if 0
u8 MeterDataCmpTaxData(u8 port,u8 gunNo,u32 amount,u32 volume,u32 price,TaxOnceTransactionRecord *pRecord)
{
    

	u8 index,datacnt;
	s8 Sindex;
	u32 Mamount,Mvolume,Mprice;
	u32 Samount=0,Svolume=0;
    u8 tt[7]={0};
	u8 t[13]={0};
	long long TotalAmount=0;
	long long TotalVolume=0;
	LocalTimeStamp ltm;
  
	datacnt=MeterDataGroup[port].MeterTraDataGroup[gunNo].DataCnt;
	
	if(datacnt>0)
	{
		index=(MeterDataGroup[port].MeterTraDataGroup[gunNo].DataCnt-1)%METER_MSG_MAX_LEN;
		Mamount=MeterDataGroup[port].MeterTraDataGroup[gunNo].amount[index];
		Mvolume=MeterDataGroup[port].MeterTraDataGroup[gunNo].volume[index];
		Mprice=MeterDataGroup[port].MeterTraDataGroup[gunNo].price[index];
		
		if(Mamount==amount && Mvolume==volume && Mprice==price)
		{
			MeterDataGroup[port].MeterTraDataGroup[gunNo].DataCnt=0;
			storeTransactionRecord(pRecord);
			printf("MeterDataCmpTaxData:TaxData = MeterData!\r\n");
			return 1;
		}
		else
		{
			for(u8 i=0;i<datacnt;i++)
			{
				Sindex=index-i;
				if(Sindex<0)
					Sindex+=METER_MSG_MAX_LEN;
				
			  //Svolume+=MeterDataGroup[port].MeterTraDataGroup[gunNo].volume[Sindex];
				Samount+=MeterDataGroup[port].MeterTraDataGroup[gunNo].amount[Sindex];
				
				if(Samount==amount)
				{
					
					memcpy(t,pRecord->content.Totalamount,12);
					TotalAmount =atoll(t);
					
					memset(t,0,13);
					
					memcpy(t,pRecord->content.Totalvolume,12);
					TotalVolume =atoll(t);	
					
					//获取叠加前的总累数据
					TotalAmount-=amount;
					TotalVolume-=volume;

					
					
					for(u8 j=0;j<=i;j++)//把计量交易数据分别存储下来,当次交易信息的其他部分不变
					{
						
						if(Sindex==METER_MSG_MAX_LEN)
							Sindex=0;
						
						sprintf(tt,"%06d",MeterDataGroup[port].MeterTraDataGroup[gunNo].volume[Sindex]); // This volume
						memcpy(pRecord->content.volume,tt,6);
						 
						
						sprintf(tt,"%06d",MeterDataGroup[port].MeterTraDataGroup[gunNo].amount[Sindex]); // This amount
						memcpy(pRecord->content.amount,tt,6);

						sprintf(tt,"%04d",MeterDataGroup[port].MeterTraDataGroup[gunNo].price[Sindex]); 
						memcpy(pRecord->content.price,tt,4);
											
						pRecord->content.timeStamp=MeterDataGroup[port].MeterTraDataGroup[gunNo].timestamp[Sindex];
						
						
						//采用计量发来的时间作为加油时间
						getLocalFormatedTime(pRecord->content.timeStamp,&ltm);
					    memcpy(pRecord->content.date,ltm.date,2);    // Date
						memcpy(pRecord->content.hour,ltm.hour,2);    // Hour
						memcpy(pRecord->content.minute,ltm.minute,2);    // Minute
						
						//回溯总累油量
						memset(t,0,13);
						TotalVolume+=MeterDataGroup[port].MeterTraDataGroup[gunNo].volume[Sindex];
						sprintf(t,"%12lld",TotalVolume); 
						memcpy(pRecord->content.Totalvolume,t,12);

						//回溯总累金额
						memset(t,0,13);
						TotalAmount+=MeterDataGroup[port].MeterTraDataGroup[gunNo].amount[Sindex];
						sprintf(t,"%12lld",TotalAmount); 
						memcpy(pRecord->content.Totalamount,t,12);
						
						storeTransactionRecord(pRecord);
						
						
						printf("++++[%d] Port:%d  Gun:%d  Volume:%d, Amount:%d, Price:%d\r\n",j,port,gunNo,
									MeterDataGroup[port].MeterTraDataGroup[gunNo].volume[Sindex],
									MeterDataGroup[port].MeterTraDataGroup[gunNo].amount[Sindex],
									MeterDataGroup[port].MeterTraDataGroup[gunNo].price[Sindex]);
						
						Sindex++;
						
						bsp_DelayMS(1);
					    
						
					}
									
					MeterDataGroup[port].MeterTraDataGroup[gunNo].DataCnt=0;
					printf("MeterDataCmpTaxData:TaxData is splitted to %d cnt!\r\n",i+1);
					return 1+i;
					
				}
					
			}
            
			  MeterDataGroup[port].MeterTraDataGroup[gunNo].SplitFailCnt++;
			  storeTransactionRecord(pRecord);
			  printf("MeterDataCmpTaxData:TaxData can't be splitted!\r\n");
			  return 0;			
			
		}
		
		
	}
	else
	{
		storeTransactionRecord(pRecord);
		printf("MeterDataCmpTaxData:NO meter data!\r\n");
	}
		
    
    return 0;
}
//#else        
u8 MeterDataCmpTaxData(u8 port,u8 gunNo,u32 amount,u32 volume,u32 price,TaxOnceTransactionRecord *pRecord)
{
    

	u8 index,datacnt;
	s8 Sindex;
	u32 Mamount,Mvolume,Mprice;
	u32 Samount=0,Svolume=0;

	long long TotalAmount=0;
	long long TotalVolume=0;
  
	datacnt=MeterDataGroup[port].MeterTraDataGroup[gunNo].DataCnt;
	
	if(datacnt>0)
	{
		index=(MeterDataGroup[port].MeterTraDataGroup[gunNo].DataCnt-1)%METER_MSG_MAX_LEN;
		Mamount=MeterDataGroup[port].MeterTraDataGroup[gunNo].amount[index];
		Mvolume=MeterDataGroup[port].MeterTraDataGroup[gunNo].volume[index];
		Mprice=MeterDataGroup[port].MeterTraDataGroup[gunNo].price[index];
		
		if(Mamount==amount && Mvolume==volume && Mprice==price)
		{
			MeterDataGroup[port].MeterTraDataGroup[gunNo].DataCnt=0;
			storeTransactionRecord(pRecord);
			printf("MeterDataCmpTaxData:TaxData = MeterData!\r\n");
			return 1;
		}
		else
		{
			for(u8 i=0;i<datacnt;i++)
			{
				Sindex=index-i;
				if(Sindex<0)
					Sindex+=METER_MSG_MAX_LEN;
				
				Samount+=MeterDataGroup[port].MeterTraDataGroup[gunNo].amount[Sindex];
				
				if(Samount==amount)
				{
										
					TotalAmount = pRecord->content.Totalamount;
					TotalVolume = pRecord->content.Totalvolume;
					
					//获取叠加前的总累数据
					TotalAmount-=amount;
					TotalVolume-=volume;
										
					for(u8 j=0;j<=i;j++)//把计量交易数据分别存储下来,当次交易信息的其他部分不变
					{
						
						if(Sindex==METER_MSG_MAX_LEN)
							Sindex=0;
						
                        
                        pRecord->content.volume = MeterDataGroup[port].MeterTraDataGroup[gunNo].volume[Sindex];
						pRecord->content.amount = MeterDataGroup[port].MeterTraDataGroup[gunNo].amount[Sindex];
                        pRecord->content.price =  MeterDataGroup[port].MeterTraDataGroup[gunNo].price[Sindex];
                        pRecord->content.timeStamp = MeterDataGroup[port].MeterTraDataGroup[gunNo].timestamp[Sindex];
                        
                        //回溯总累油量
                        TotalVolume+=MeterDataGroup[port].MeterTraDataGroup[gunNo].volume[Sindex];
					    pRecord->content.Totalvolume = TotalVolume;
						//回溯总累金额
                        TotalAmount+=MeterDataGroup[port].MeterTraDataGroup[gunNo].amount[Sindex];						
                        pRecord->content.Totalamount = TotalAmount;
                       					
						
						storeTransactionRecord(pRecord);
						
						
						printf("++++[%d] Port:%d  Gun:%d  Volume:%d, Amount:%d, Price:%d\r\n",j,port,gunNo,
									MeterDataGroup[port].MeterTraDataGroup[gunNo].volume[Sindex],
									MeterDataGroup[port].MeterTraDataGroup[gunNo].amount[Sindex],
									MeterDataGroup[port].MeterTraDataGroup[gunNo].price[Sindex]);
						
						Sindex++;
						
						bsp_DelayMS(1);
					    					
					}
									
					MeterDataGroup[port].MeterTraDataGroup[gunNo].DataCnt=0;
					printf("MeterDataCmpTaxData:TaxData is splitted to %d cnt!\r\n",i+1);
					return 1+i;
					
				}
					
			}
            
			  MeterDataGroup[port].MeterTraDataGroup[gunNo].SplitFailCnt++;
			  storeTransactionRecord(pRecord);
			  printf("MeterDataCmpTaxData:TaxData can't be splitted!\r\n");
			  return 0;			
			
		}
		
		
	}
	else
	{
		storeTransactionRecord(pRecord);
		printf("MeterDataCmpTaxData:NO meter data!\r\n");
	}
		
    
    return 0;
}
#endif
         
/**************************************************************************************
Function Name: meterMessagerTask
Description:
     The task intends to periodically poll uart buffer to try to pick up a complete message and process it.
Parameter:
    NULL
Return:
    NULL.
**************************************************************************************/
void meterMessagerTask()
{
    unsigned char meterport=0;//对于抄报端来说只有一个计量口，此处meterport=0
    MeterMsgRcvDes *pDes=&meterMsgRcvDes[meterport];
    
    
    if(meterMessagerRetriever(meterport,pDes) == METER_MSG_RCV_COMPLETE)
    {
        //printf("meterMessagerTask: Successfully retrieve a message \r\n");
        pDes->state = METER_MSG_RCV_EMPTY;
        pDes->waitCount = 0;
        meterMsgProcess(pDes);
    }
 
    
}

void meterQueryTask(u8 taxport)
{
    
     meterSendQueryCmd(taxport);     					
	   	
}

/*
void setMeterMode(MeterModeState state)
{
	MeterMode=state;
}

MeterModeState getMeterMode(void)
{
	return MeterMode;
	
}
*/

void setMeter422Mode(u8 state)
{
	Meter422Mode =state;
}

u8 getMeter422Mode(void)
{
	return Meter422Mode;
	
}



MeterTransactionDataGroup* getMeterDataGroup(void)
{
	return MeterDataGroup;
}

u8 getGunPickNum(u8 TaxPort)
{
	return  gunstatusmgr[TaxPort].AnyGunPickUp;
}

GunStatusManager *getGunStatusManager(void)
{
	 return gunstatusmgr;
}



#ifdef  METER_DEBUG
/**************************************************************************************
Function Name: dbgSendGunStatusReport
Description:
      Simulate the partner side to send gun status to "this device"
Parameter:
      port:[I], meter uart port for this sending.
Return:
      Always E_SUCCESS.
**************************************************************************************/
static unsigned char dbgSeq=0;

errorCode    dbgSendGunStatusReport(unsigned char port, unsigned char gunNo, GunWorkingState status) 
{
    MeterMsgStatus gunStatus;
    unsigned char cc=0;

    gunStatus.head.preamble[0]='d';
    gunStatus.head.preamble[1]='x';
    gunStatus.head.len=6;
    gunStatus.head.msg = METER_MSG_STATUS;
    gunStatus.head.seqNo = dbgSeq++;
    gunStatus.port = 0;
    gunStatus.gunNo = gunNo;
    gunStatus.status = status;

    cc = cc ^ gunStatus.head.len;
    cc = cc ^ gunStatus.head.msg;
    cc = cc ^ gunStatus.head.seqNo;
    cc = cc ^ gunStatus.port;
    cc = cc ^ gunStatus.gunNo;
    cc = cc ^ gunStatus.status;

    gunStatus.cc = cc;

    uartWriteBuffer(port+UART_METER_PORT_BASE, (unsigned char *)&gunStatus, sizeof(MeterMsgStatus));
    uartSendBufferOut(port+UART_METER_PORT_BASE);

    return E_SUCCESS;    
}

static unsigned short price=500;
static unsigned int volume = 100;
static unsigned int amount=0;

errorCode dbgSendGunTransactionReport(unsigned char port,unsigned char gunNo)
{
    MeterMsgTransaction gunTransaction;
    unsigned char cc=0;

    gunTransaction.head.preamble[0]='d';
    gunTransaction.head.preamble[1]='x';
    gunTransaction.head.len=15;
    gunTransaction.head.msg = METER_MSG_TRANSACTION;
    gunTransaction.head.seqNo = dbgSeq++;
    gunTransaction.port = 0;
    gunTransaction.gunNo = gunNo;

    amount = volume * price;
    gunTransaction.price[0] = price >> 8;
    gunTransaction.price[1] = (price & 0x00ff);

    gunTransaction.volume[0] = (volume & 0xff000000) >> 24;
    gunTransaction.volume[1] = (volume & 0x00ffffff) >> 16;
    gunTransaction.volume[2] = (volume & 0x0000ffff) >> 8;
    gunTransaction.volume[3] = (volume & 0x000000ff);

    gunTransaction.amount[0] = (amount & 0xff000000) >> 24;
    gunTransaction.amount[1] = (amount & 0x00ffffff) >> 16;
    gunTransaction.amount[2] = (amount & 0x0000ffff) >> 8;
    gunTransaction.amount[3] = (amount & 0x000000ff);

    cc = cc ^ gunTransaction.head.len;
    cc = cc ^ gunTransaction.head.msg;
    cc = cc ^ gunTransaction.head.seqNo;
    cc = cc ^ gunTransaction.port;
    cc = cc ^ gunTransaction.gunNo;
    
    cc = cc ^ gunTransaction.price[0];
    cc = cc ^ gunTransaction.price[1];
    
    cc = cc ^ gunTransaction.volume[0];
    cc = cc ^ gunTransaction.volume[1];
    cc = cc ^ gunTransaction.volume[2];
    cc = cc ^ gunTransaction.volume[3];
    
    cc = cc ^ gunTransaction.amount[0];
    cc = cc ^ gunTransaction.amount[1];
    cc = cc ^ gunTransaction.amount[2];
    cc = cc ^ gunTransaction.amount[3];

    gunTransaction.cc = cc;

    uartWriteBuffer(port+UART_METER_PORT_BASE, (unsigned char *)&gunTransaction, sizeof(MeterMsgTransaction));
    uartSendBufferOut(port+UART_METER_PORT_BASE);

    printf("dbgSendGunTransactionReport: Send Transaction via Port:%d,  Gun:%d  price:%d  volume:%d  amount:%d\r\n",port,gunNo,price,volume,amount);
    volume += 100;
}
#endif
