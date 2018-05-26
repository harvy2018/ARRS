/***************************************************************************************
File name: meterCPCC.c
Date: 2017-3-27
Author: Guoku Yan
Description:
     This file impelements the protocol parser for CPCC(China Petroleum and Chemical Corporation) protocol,
 capture the protocol from gasoline machine's communication port to FC(Front Court), retrieve the "Pick up",
 "Put down" and "Transaction Info" events to help/guide tax control port sampling.
     As per info, all the gasoline machine for CPCC must abide by this protocal, which is lucky for us.
***************************************************************************************/
//
#include "stm32f10x.h"
#include "core_cm3.h"
#include "meterCPCC.h"
#include "controlledTaxSampling.h"
#include "uartPort.h"
#include "board.h"

unsigned char MeterSeq=0;

CPCCMsgRcvDes CPCCMsgRcv[4];

GunState_Meter GunStateMgr[TAX_CONTROL_DEVICE_MAX_NUM];//һ����˰���µ�����ǹ״̬����

MeterMsgTransactionMgr MMsgT[TAX_CONTROL_DEVICE_MAX_NUM];//���潻����Ϣ


void GunStateMgrInit(void)
{
	for(u8 i=0;i<TAX_CONTROL_DEVICE_MAX_NUM;i++)
	{
		memset((u8*)&GunStateMgr[i],0,sizeof(GunState_Meter));
		GunStateMgr[i].gunst.mark=1;
		GunStateMgr[i].gunst.head.msg=METER_RESP_STATUS;
		GunStateMgr[i].preMark=1;
		
		//GunStateMgr[i].NeedReport=1;
	}
}

GunState_Meter* getGunStateMgrPtr(u8 port)
{
   return &GunStateMgr[port];
}

unsigned int CRC16_Calc( u8 *arr_buff, u16 len)
{
 u8 i;
 u16 crc=0,j;
 u16 Polynomial=0xA001;
 for (j=0; j<len;j++)
 {
   crc^=*arr_buff++;
   for ( i=0; i<8; i++)
   {
       if( ( crc&0x0001) >0)
       {
           crc=crc>>1;
           crc^=Polynomial;
        }
      else
          crc=crc>>1;
   }   
 }
  return crc;
}

u8 CompressBCD8toHEX(u8 BCD)
{
	u8 value;
	
	value=BCD&0x0F;	
	value+=((BCD>>4)&0x0F)*10;
	return value;
	
}

u16 CompressBCD16toHEX(u16 BCD)
{
	u16 value;
	
	value=BCD&0x000F;	
	value+=((BCD>>4)&0x000F)*10;
	value+=((BCD>>8)&0x000F)*100;
	value+=((BCD>>12)&0x000F)*1000;
	return value;
	
}

u8 ESCjudge(u8 *buf,u8 len)
{
	u8 i=0,j;
	u8 ESC_CNT=0;
	
	if(len<=1)
	  return ESC_CNT;
	
	while(i+2<=len)
	{
		if(buf[i]==0xFA && buf[i+1]==0xFA)
		{
			if(i+2<len)
			{
			 // buf[i+1]=buf[i+2];
              for(j=i;j<len;j++)
				buf[j]=buf[j+1];
				
			}				
	
			ESC_CNT++;			
		}
		i++;
	}	
	
	return ESC_CNT;//����ת���ַ�����
	
}


void ESCProcess(u8 port,u8 *buff, CPCCMsgRcvDes *pMsgRcvDes)
{

	u8 *pbuf;
	u8 ESCnt,ret;
	u16 len_t;
	
	pbuf=buff+sizeof(CPCCHead);
	//datalen=CompressBCD16toHEX(__REV16(pMsgRcvDes->CPCCMsg.head.frameLen));
	len_t=pMsgRcvDes->DataLen;
	ret=uartRead(port,pbuf,len_t);
	if(ret)
	{
		 uartClearRcvBuffer(port);
		 pMsgRcvDes->state=CPCC_MSG_RCV_EMPTY;
	     return ;	
	}	
	
	 while(1)
	 {
	   ESCnt=ESCjudge(pbuf,len_t);
	   len_t-=ESCnt;
	   pbuf+=len_t;
		 
	   if(ESCnt) 
	   {				    
		 ret=uartRead(port,pbuf,ESCnt);
		 if(ret)
		 {
			 uartClearRcvBuffer(port);
			 pMsgRcvDes->state=CPCC_MSG_RCV_EMPTY;
			 return ;			
		 }	
		 len_t=ESCnt; 
	   }					   
	   else
		 break;
	 }
	 
	 ret=uartRead(port,pbuf,2);	//��ȡCRC16	
 	 if(ret)
	 {
		 uartClearRcvBuffer(port);
		 pMsgRcvDes->state=CPCC_MSG_RCV_EMPTY;
		 return ;			
	 }	
	 
	 ESCnt=ESCjudge(pbuf,2);
	 
	 if(ESCnt)
	 {
		pbuf-=ESCnt; 
		ret=uartRead(port,pbuf+2,1); 
		 if(ret)
		 {
			 uartClearRcvBuffer(port);
			 pMsgRcvDes->state=CPCC_MSG_RCV_EMPTY;
			 return ;			
		 }	
	 }
	 
	 ///////////////���÷��վ���ֵļ���CRC ERR////////////////////////////////
	     
	     pbuf=buff+sizeof(CPCCHead);
	     len_t=pMsgRcvDes->DataLen+2;
	 
	     ESCnt=ESCjudge(pbuf,len_t);
	 
		 if(ESCnt)
		 {
			len_t-=ESCnt;			 
			pbuf+=len_t; 
			 
			ret=uartRead(port,pbuf,ESCnt); 
			 if(ret)
			 {
				 uartClearRcvBuffer(port);
				 pMsgRcvDes->state=CPCC_MSG_RCV_EMPTY;
				 return ;			
			 }	
		 }
	 
	 
	 
	 ////////////////////////////////////////////////////////////////////////
	 
	 
	 memcpy(buff,(u8 *)&(pMsgRcvDes->CPCCMsg.head),sizeof(CPCCHead)); 
	 memcpy(pMsgRcvDes->CPCCMsg.content,buff+sizeof(CPCCHead),pMsgRcvDes->DataLen);
	 memcpy((u8*)&pMsgRcvDes->CPCCMsg.cc,buff+sizeof(CPCCHead)+pMsgRcvDes->DataLen,2);
	 
	 
}


static CPCCMessageRcvState CPCCMessagerRetriever(unsigned char port,CPCCMsgRcvDes *pMsgRcvDes)
{
	 u16 availBytes=0;
	 u16 cc,datalen;
	 u8  buff[CPCC_PROTOCOL_MAX_SIZE];
	 u8  ESCnt,ret;
	 u8 PackF=0,PackS=0,cnt=0;
			
	
	 CPCCHead *phead;
	 phead=&pMsgRcvDes->CPCCMsg.head;
	 availBytes = uartGetAvailBufferedDataNum(port);
	
    switch(pMsgRcvDes->state)
	{
	  case 	CPCC_MSG_RCV_EMPTY:
			 
		 if(availBytes>=CPCC_PROTOCOL_MIN_PACK_SIZE)
		 {
			ret=uartRead(port, (u8 *)&(pMsgRcvDes->CPCCMsg.head),sizeof(CPCCHead));	
			 
			if(ret)
			{
				 uartClearRcvBuffer(port);
				 pMsgRcvDes->state=CPCC_MSG_RCV_EMPTY;
				 return CPCC_MSG_RCV_ERR;			
			}	
			
			datalen=CompressBCD16toHEX(__REV16(phead->frameLen));			 
			pMsgRcvDes->DataLen=datalen; 
			 
			if(phead->preamable==0xFA && phead->dstAddr!=0xFA)//
			{ 
			  if(availBytes>=datalen+sizeof(CPCCHead)+2)//Э��ͷ+���ݳ���+2�ֽ�CRC	
			  {			
				 ESCProcess(port,buff,pMsgRcvDes);			 			  				  
				 cc= CRC16_Calc((u8*)(buff+1),datalen+sizeof(CPCCHead)-1);//ǰ���벻����CRC									
				 
				 if(cc==__REV16(*(u16*)(buff+sizeof(CPCCHead)+datalen)))
				 {													
					 pMsgRcvDes->waitCount=0;
					 pMsgRcvDes->state=CPCC_MSG_RCV_EMPTY;
					 return CPCC_MSG_RCV_COMPLETE;					 
				 }
				 else
				 {					
					 pMsgRcvDes->waitCount=0;
					 pMsgRcvDes->state=CPCC_MSG_RCV_EMPTY;
					 uartClearRcvBuffer(port);
					 printf("CPCCMessagerRetriever crc err!\r\n");
					 return CPCC_MSG_RCV_ERR;
				 }
				 
			  }
			  else	  
			  {
				  pMsgRcvDes->waitCount=0;  				  
				  pMsgRcvDes->state=CPCC_MSG_RCV_HEAD;
				  return CPCC_MSG_RCV_HEAD;
				  
			  }			  
			 
		   }
		   else //��ǰ����������������
		   {			   	  
				u8 i=0,j=0,FAindex; 
				pMsgRcvDes->waitCount=0;  						
				memcpy(buff,(u8 *)&(pMsgRcvDes->CPCCMsg.head),sizeof(CPCCHead));
			   				
				while(i<=sizeof(CPCCHead))
				{
					if(buff[i]==0xFA && buff[i+1]!=0xFA && i<=sizeof(CPCCHead)) //�����������FA
					   break;
					else if(buff[i]==0xFA && i==sizeof(CPCCHead))		//���һ��FA			
					   break;
					
					i++;
				}
				
			   if(i>sizeof(CPCCHead))  /*2017-4-16 09:29:56*/  
			   {				
			
				  while(cnt<=(availBytes-sizeof(CPCCHead)))
				  {
					  ret=uartRead(port,&PackF,1);				   
					  cnt++;						  
					  if(PackF==0xFA)
					  {
						 
						  ret=uartRead(port,&PackS,1);				   
						  if(ret==E_SUCCESS)
						  {	
							  cnt++; 						  
							  if(PackS!=0xFA)
							  {
								  buff[1]=PackS;	
								  break;
							  }								  
							  
						  }
					  }
					  else if(PackS==0xFA)
					  {
						  buff[1]=PackF;
						  break;
					  }
					  				  
					
			      }
				  
				   printf("FA cnt:%d\r\n",cnt);
				   FAindex=cnt+sizeof(CPCCHead);	//FA��λ���Ѿ�����ǰ6���ֽ�����
				   buff[0]=0xFA;
				  
				   
			   }
			   else
			   {
				  FAindex=i;				
			
				  for(j=0;j<(sizeof(CPCCHead)-FAindex);j++)
				  {
					 buff[j]=buff[i] ;
					  i++;				  
				  }
				   
			   }
			   
		     	
			  
			  if(availBytes-FAindex>=sizeof(CPCCHead))
			  {
				  if(FAindex<=sizeof(CPCCHead))//FA��ǰ6�ֽ���
				  {
					  ret=uartRead(port,buff+sizeof(CPCCHead)-FAindex,FAindex);
					  if(ret)
					  {
						 uartClearRcvBuffer(port);
						 pMsgRcvDes->waitCount=0;
						 pMsgRcvDes->state=CPCC_MSG_RCV_EMPTY;
						 return CPCC_MSG_RCV_ERR;			
					  }	
					  
				  }
				  else
				  {
					  				 					  
					  ret=uartRead(port,buff+2,sizeof(CPCCHead)-2); 
					  if(ret)
					  {
						 uartClearRcvBuffer(port);
						 pMsgRcvDes->waitCount=0;
						 pMsgRcvDes->state=CPCC_MSG_RCV_EMPTY;
						 return CPCC_MSG_RCV_ERR;			
					  }	
				  }					  
				  
				  

				  memcpy((u8 *)&(pMsgRcvDes->CPCCMsg.head),buff,sizeof(CPCCHead));
				  datalen=CompressBCD16toHEX(__REV16(phead->frameLen));			 
			      pMsgRcvDes->DataLen=datalen; //��ȡ����
				  
				  if(availBytes-FAindex>=datalen+sizeof(CPCCHead)+2)//Э��ͷ+���ݳ���+2�ֽ�CRC	
				  {					
					 ESCProcess(port,buff,pMsgRcvDes);												  
					 cc= CRC16_Calc((u8*)(buff+1),datalen+sizeof(CPCCHead)-1);//ǰ���벻����CRC				 					
					 if(cc==__REV16(*(u16*)(buff+sizeof(CPCCHead)+datalen)))
					 {								
						 pMsgRcvDes->waitCount=0;
						 pMsgRcvDes->state=CPCC_MSG_RCV_EMPTY;
						// uartClearRcvBuffer(port);
						 return CPCC_MSG_RCV_COMPLETE;
						 
					 }
					 else
					 {
						 pMsgRcvDes->waitCount=0;
						 pMsgRcvDes->state=CPCC_MSG_RCV_EMPTY;
						  uartClearRcvBuffer(port);
						 printf("prepacket has data: crc err!\r\n");
						 return CPCC_MSG_RCV_ERR;
					 }
					 
				  }
				  else
				  {
					   pMsgRcvDes->waitCount=0;  				  
					   pMsgRcvDes->state=CPCC_MSG_RCV_HEAD;
					   printf("prepacket has data:only get head!availBytes:%d,FAindex:%d,datalen:%d\r\n",availBytes,FAindex,datalen);
					   return CPCC_MSG_RCV_HEAD;
				  }
			  }
			  else
			  {
				  if(FAindex<=sizeof(CPCCHead))//FA��ǰ6�ֽ���
				  {
					  ret=uartRead(port,buff+sizeof(CPCCHead)-FAindex, availBytes-FAindex);
					  if(ret!=E_EMPTY || ret!=E_SUCCESS)
					  {
						 uartClearRcvBuffer(port);
						 pMsgRcvDes->waitCount=0;
						 pMsgRcvDes->state=CPCC_MSG_RCV_EMPTY;
						 return CPCC_MSG_RCV_ERR;			
					  }
					   pMsgRcvDes->HaveGotLen=availBytes-FAindex;
					   memcpy((u8 *)&(pMsgRcvDes->CPCCMsg.head),buff,pMsgRcvDes->HaveGotLen);				  
					   pMsgRcvDes->state=CPCC_MSG_RCV_NOHEAD;
					   printf("prepacket has data:not get head! HaveGotLen��%d\r\n",pMsgRcvDes->HaveGotLen);
			      }	
				  else
				  {
					 					  
					  ret=uartRead(port,buff+2,availBytes-FAindex); 
					  if(ret!=E_EMPTY || ret!=E_SUCCESS)
					  {
						 uartClearRcvBuffer(port);
						 pMsgRcvDes->waitCount=0;
						 pMsgRcvDes->state=CPCC_MSG_RCV_EMPTY;
						 return CPCC_MSG_RCV_ERR;			
					  }	
					  
					  if(availBytes-FAindex>=0)
					  {
						  pMsgRcvDes->HaveGotLen=availBytes-FAindex+2;
						  memcpy((u8 *)&(pMsgRcvDes->CPCCMsg.head),buff,pMsgRcvDes->HaveGotLen);				  
						  pMsgRcvDes->state=CPCC_MSG_RCV_NOHEAD;
						  printf("prepacket has data:not get head! HaveGotLen��%d\r\n",pMsgRcvDes->HaveGotLen);
				  
					  }	
					  
				  }					  
				  
				 
				  return CPCC_MSG_RCV_NOHEAD;
			  }
			   
		   }
			
		 }
	  break;
		 		 
	 case 	CPCC_MSG_RCV_HEAD:
		 
		if(availBytes<pMsgRcvDes->DataLen+2) //Э��ͷ�Ѿ�ȡ�ߣ�availBytes���Ȳ�����Э��ͷ����		  
		{
		  pMsgRcvDes->waitCount++;   
		 
		  if(pMsgRcvDes->waitCount>CPCC_PROTOCOL_WAITCNT)
		  {
			  pMsgRcvDes->waitCount=0;
			  pMsgRcvDes->state=CPCC_MSG_RCV_EMPTY; 
			  uartClearRcvBuffer(port);
			  return CPCC_MSG_RCV_EMPTY;
		  }
		  else
		  {
			  pMsgRcvDes->state=CPCC_MSG_RCV_HEAD;
			  return CPCC_MSG_RCV_HEAD;
		  }
		  		  			  
		}
		else
		{
		   	 ESCProcess(port,buff,pMsgRcvDes);				 			  				  
			 cc= CRC16_Calc((u8*)(buff+1),pMsgRcvDes->DataLen+sizeof(CPCCHead)-1);//ǰ���벻����CRC			 			
			 if(cc==__REV16(*(u16*)(buff+sizeof(CPCCHead)+pMsgRcvDes->DataLen)))
			 {								
				 pMsgRcvDes->waitCount=0;
				 pMsgRcvDes->state=CPCC_MSG_RCV_EMPTY;
				 return CPCC_MSG_RCV_COMPLETE;				 
			 }
			 else
			 {
				 pMsgRcvDes->waitCount=0;
				 pMsgRcvDes->state=CPCC_MSG_RCV_EMPTY;
				 uartClearRcvBuffer(port);
				 printf("CPCC_MSG_RCV_HEAD: crc err!\r\n");
				 return CPCC_MSG_RCV_ERR;
			 }					
		}
     
     break;	 
		
	 case CPCC_MSG_RCV_NOHEAD:
          if((availBytes+pMsgRcvDes->HaveGotLen)>=CPCC_PROTOCOL_MIN_PACK_SIZE)
		  {
			  //�����ͷ
			  ret=uartRead(port,((u8 *)&(pMsgRcvDes->CPCCMsg.head)+pMsgRcvDes->HaveGotLen),sizeof(CPCCHead)-pMsgRcvDes->HaveGotLen);
			  if(ret)
			  {
				 uartClearRcvBuffer(port);
				 pMsgRcvDes->state=CPCC_MSG_RCV_EMPTY;
				 return CPCC_MSG_RCV_ERR;			
			  }	
			  
			  datalen=CompressBCD16toHEX(__REV16(phead->frameLen));
			  
			  if(availBytes+pMsgRcvDes->HaveGotLen>=datalen+sizeof(CPCCHead)+2)//Э��ͷ+���ݳ���+2�ֽ�CRC	
			  {								
				 ESCProcess(port,buff,pMsgRcvDes);				 			  				  
				 cc= CRC16_Calc((u8*)(buff+1),datalen+sizeof(CPCCHead)-1);//ǰ���벻����CRC				 
				
				 if(cc==__REV16(*(u16*)(buff+sizeof(CPCCHead)+datalen)))
				 {								
					 pMsgRcvDes->waitCount=0;
					 pMsgRcvDes->HaveGotLen=0;
					 pMsgRcvDes->state=CPCC_MSG_RCV_EMPTY;
					 return CPCC_MSG_RCV_COMPLETE;
					 
				 }
				 else
				 {
					 pMsgRcvDes->waitCount=0;
					 pMsgRcvDes->HaveGotLen=0;
					 pMsgRcvDes->state=CPCC_MSG_RCV_EMPTY;
					 uartClearRcvBuffer(port);
					 printf("CPCC_MSG_RCV_NOHEAD: crc err!\r\n");
					 return CPCC_MSG_RCV_ERR;
				 }				 		  
			  
			  }
			  else	  
			  {
				  pMsgRcvDes->waitCount=0;  
				  pMsgRcvDes->HaveGotLen=0;				  
				  pMsgRcvDes->state=CPCC_MSG_RCV_HEAD;
				  return CPCC_MSG_RCV_HEAD;				  
			  } 
		  }
		  else
		  {
			  pMsgRcvDes->waitCount++;
			  if(pMsgRcvDes->waitCount>CPCC_PROTOCOL_WAITCNT)
			  {
				  pMsgRcvDes->waitCount=0;
				  pMsgRcvDes->state=CPCC_MSG_RCV_EMPTY; 
				  uartClearRcvBuffer(port);
				  return CPCC_MSG_RCV_EMPTY;
			  }
			  else
			  {
				  pMsgRcvDes->state=CPCC_MSG_RCV_NOHEAD;
				  return CPCC_MSG_RCV_NOHEAD;			
			  }
		  }		  	 
	 break;	 
	
	}
	
}

CPCCTransactionInfo *CPCCtrainfo;
CPCCContentCardInsert *CPCCcardInsert;
CPCCContentFueling *CPCCfueling;
//u32 Tmac[GunNumPerMeter]={0};

void CPCCMsgProcess(u8 meterport,CPCCMsgRcvDes *pDes)
{
	u8 RTInfoCnt,RTInfoST;
	u8 cc=0;
	static u8 delay_send[GunNumPerMeter]={0};
	static u8 FrameNo[GunNumPerMeter]={0};
	u8 *pbuf;
	u32 amount,volume;
	static u32 MeterTrancstionInfoFrameNO[GunNumPerMeter]={0};
    u8 GunMapNum=0;
    
    
    ControlPortSetMgr  *pControlPortSetMgr = getControlPortMgr();
    ControlPortStruct  *pControlPortStruct = 0;    
    GunStatusManager   *pGunStatusMgr = getGunStatusManager();
	u8 mode = 0;   // Seperate gun mode and meter mode
    u8 port = 0;     // port number  
    u8 MeterGunNo=0; //����Э���д�������ǹ��
 //   unsigned char TaxGunNo=0; //��˰�ڶ�Ӧ������ǹ��
    u8 gPx = 0; /* ǹ�ź�ģʽ��gpx��ǹ�ź�λ�ã�����ģʽ���Ǽ���Э���д�����ǹ�ŶԱ�˰����֧�ֵ���ǹ��ȡģ�Ľ������һ������ӳ�� */
    

    
    
    
    
	MeterMsgTransaction *gunTransaction;
		
	
	switch(pDes->CPCCMsg.content[0])
	{
		case CPCC_PROTOCOL_CMD_TRANSACTION://������Ϣ
			CPCCtrainfo=(CPCCTransactionInfo *)pDes->CPCCMsg.content;	
			
			amount=(CPCCtrainfo->AMN[0]);
			amount<<=8;
			amount+=CPCCtrainfo->AMN[1];
			amount<<=8;
			amount+=CPCCtrainfo->AMN[2];
			
			volume=(CPCCtrainfo->VOL[0]);
			volume<<=8;
			volume+=(CPCCtrainfo->VOL[1]);
			volume<<=8;
			volume+=(CPCCtrainfo->VOL[2]);			
			
			printf("%02x%02x-%02x-%02x %02x:%02x:%02x\r\n",CPCCtrainfo->TIME[0],CPCCtrainfo->TIME[1],
		    CPCCtrainfo->TIME[2],CPCCtrainfo->TIME[3],CPCCtrainfo->TIME[4],CPCCtrainfo->TIME[5],CPCCtrainfo->TIME[6]);					
		
		    printf("PORT:%d,GUN:%d,AMN:%d,VOL:%d,PRC:%d\r\n",meterport-1,CPCCtrainfo->NZN,amount,volume,__REV16(CPCCtrainfo->PRC));
			printf("GCODE:%02x%02x\r\n",CPCCtrainfo->G_CODE[0],CPCCtrainfo->G_CODE[1]);
		    
            GunMapNum= CPCCtrainfo->NZN % GunNumPerMeter;
            
          //  CPCCtrainfo->NZN=0;//���嵥ǹ��ǹ��ǿ��0���ݲ�֧�ֶ�ǹ
            
            
		    gunTransaction=&MMsgT[meterport-1].MeterMsgTRSMgr[GunMapNum];
			pbuf=(u8*)gunTransaction;			
			pbuf+=2; //����ǰ����
		
		
		    gunTransaction->head.preamble[0]='d';
			gunTransaction->head.preamble[1]='x';
			gunTransaction->head.len=sizeof(MeterMsgTransaction)-3;
			
			gunTransaction->head.version=MeterProtocolRev;
						
			gunTransaction->head.msg = METER_MSG_TRANSACTION;
			gunTransaction->head.seqNo = MeterSeq++;
			gunTransaction->port = meterport-1;
			gunTransaction->gunNo = CPCCtrainfo->NZN;
			
			gunTransaction->price[0] =(CPCCtrainfo->PRC & 0x00ff);
			gunTransaction->price[1] = CPCCtrainfo->PRC>>8;

			gunTransaction->volume[0] = 0;
			gunTransaction->volume[1] = CPCCtrainfo->VOL[0];
			gunTransaction->volume[2] = CPCCtrainfo->VOL[1];
			gunTransaction->volume[3] = CPCCtrainfo->VOL[2];

			gunTransaction->amount[0] = 0;
			gunTransaction->amount[1] = CPCCtrainfo->AMN[0];
			gunTransaction->amount[2] = CPCCtrainfo->AMN[1];
			gunTransaction->amount[3] = CPCCtrainfo->AMN[2];
			
			for(u8 i=0;i<sizeof(MeterMsgTransaction)-3;i++)  //ǰ�����cc,������У��
			  cc^= *pbuf++;
			  
			gunTransaction->cc=cc;	


            pDes->gun[GunMapNum]=GUN_STATE_PUT;
            GunStateMgr[meterport-1].gunst.status&=~(1<<GunMapNum);


            if(MeterTrancstionInfoFrameNO[GunMapNum]!=CPCCtrainfo->POS_TTC)
			{
			//	MMsgT[meterport-1].HaveMessageNeedReport[GunMapNum]=1;
				MeterTrancstionInfoFrameNO[GunMapNum]=CPCCtrainfo->POS_TTC;
			//	SendGunTransactionReport(PORTtoCB,CPCCtrainfo->NZN,gunTransaction);	//���ͽ�����Ϣ
			//	SendGunStateReport(getWorkMode(),meterport-1, CPCCtrainfo->NZN, GUN_STATE_PUT);//����ǹ״̬��Ϣ	
				
			}
/*			
			SendGunTransactionReport(PORTtoCB,CPCCtrainfo->NZN,gunTransaction);	//���ͽ�����Ϣ	
			SendGunStatusReport(meterport-1, CPCCtrainfo->NZN, GUN_STATE_PUT);//����ǹ״̬��Ϣ	
*/
						
		break;
		
		case CPCC_PROTOCOL_CMD_REALTIME_INFO:  //ʵʱ������Ϣ
			RTInfoCnt=pDes->CPCCMsg.content[1];
		    RTInfoST=pDes->CPCCMsg.content[2];
			if(RTInfoCnt==1)//COUNTֵ
			{    
				if(RTInfoST==1)//��������Ϣ
				{
					 CPCCcardInsert=(CPCCContentCardInsert*)&pDes->CPCCMsg.content[2];
					 printf("PORT:%d,Card Insert! CardStuts:%02X%02X CardNO:",meterport-1,(u8)(CPCCcardInsert->cardSt&0xFF),(u8)((CPCCcardInsert->cardSt>>8)&0xFF));
					 for(u8 i=0;i<10;i++)
						 printf("%02x ",CPCCcardInsert->asn[i]);
					
                    printf("Money:%u\r\n",__REV(CPCCcardInsert->bal));					 
					
				}
				if(RTInfoST==2)//̧ǹ�������
				{
					CPCCfueling=(CPCCContentFueling *)&pDes->CPCCMsg.content[2];
				
					amount=(CPCCfueling->AMN[0]);
					amount<<=8;
					amount+=CPCCfueling->AMN[1];
					amount<<=8;
					amount+=CPCCfueling->AMN[2];

					volume=(CPCCfueling->VOL[0]);
					volume<<=8;
					volume+=(CPCCfueling->VOL[1]);
					volume<<=8;
					volume+=(CPCCfueling->VOL[2]);
								
					
					if((CPCCfueling->punit&0x01)==0)	
						printf("PORT:%d,GUN:%d,AMN:%d,VOL:%d,PRC:%d\r\n",meterport-1,CPCCfueling->nzn,amount,volume,__REV16(CPCCfueling->PRC));					
					else						
						printf("NOT CASH TRADE! PORT:%d,GUN:%d,AMN:%d,VOL:%d,PRC:%d\r\n",meterport-1,CPCCfueling->nzn,amount,volume,__REV16(CPCCfueling->PRC));				
					
                    
					//CPCCfueling->nzn=0;//���嵥ǹ��ǹ��ǿ��0���ݲ�֧�ֶ�ǹ
                    GunMapNum= CPCCfueling->nzn % GunNumPerMeter;
                    
                    
					GunStateMgr[meterport-1].gunst.status|=(1<<GunMapNum);
					GunStateMgr[meterport-1].gunst.mark=0; 
					if(GunStateMgr[meterport-1].preMark!=GunStateMgr[meterport-1].gunst.mark)//�����ڵ����������ϱ�ǹ״̬
					{
						GunStateMgr[meterport-1].NeedReport=1;
						GunStateMgr[meterport-1].preMark=GunStateMgr[meterport-1].gunst.mark;
					}
									
					if(pDes->gun[GunMapNum]==GUN_STATE_PUT)
					{
						pDes->gun[GunMapNum]=GUN_STATE_PICK;
				//		SendGunStateReport(getWorkMode(),meterport-1, CPCCfueling->nzn, GUN_STATE_PICK);//����ǹ״̬��Ϣ
					}
																			
				}												
				
			}
			else if(RTInfoCnt!=0)
			{
				
				printf("PORT:%d,PickUp!2 Messages!\r\n",meterport-1);
                
                if(RTInfoST==1)//��������Ϣ
				{
					 CPCCcardInsert=(CPCCContentCardInsert*)&pDes->CPCCMsg.content[2];			
					 printf("PORT:%d,Card Insert! CardStus:%02X%02X CardNO:",meterport-1,(u8)(CPCCcardInsert->cardSt&0xFF),(u8)((CPCCcardInsert->cardSt>>8)&0xFF));
					 for(u8 i=0;i<10;i++)
						 printf("%02x ",CPCCcardInsert->asn[i]);
					
                     printf("Money:%u\r\n",__REV(CPCCcardInsert->bal));		


                    //�ڶ�����Ϣ����
                     
					CPCCfueling=(CPCCContentFueling *)&pDes->CPCCMsg.content[1+sizeof(CPCCContentCardInsert)];
				
					amount=(CPCCfueling->AMN[0]);
					amount<<=8;
					amount+=CPCCfueling->AMN[1];
					amount<<=8;
					amount+=CPCCfueling->AMN[2];

					volume=(CPCCfueling->VOL[0]);
					volume<<=8;
					volume+=(CPCCfueling->VOL[1]);
					volume<<=8;
					volume+=(CPCCfueling->VOL[2]);
								
					
					if((CPCCfueling->punit&0x01)==0)	
						printf("PORT:%d,GUN:%d,AMN:%d,VOL:%d,PRC:%d\r\n",meterport-1,CPCCfueling->nzn,amount,volume,__REV16(CPCCfueling->PRC));					
					else						
						printf("NOT CASH TRADE! PORT:%d,GUN:%d,AMN:%d,VOL:%d,PRC:%d\r\n",meterport-1,CPCCfueling->nzn,amount,volume,__REV16(CPCCfueling->PRC));				
					 
                
				}

                if(RTInfoST==2)//̧ǹ�������
                {
                    CPCCfueling=(CPCCContentFueling *)&pDes->CPCCMsg.content[2];
				
					amount=(CPCCfueling->AMN[0]);
					amount<<=8;
					amount+=CPCCfueling->AMN[1];
					amount<<=8;
					amount+=CPCCfueling->AMN[2];

					volume=(CPCCfueling->VOL[0]);
					volume<<=8;
					volume+=(CPCCfueling->VOL[1]);
					volume<<=8;
					volume+=(CPCCfueling->VOL[2]);
					
					
					if((CPCCfueling->punit&0x01)==0)	
						printf("PORT:%d,GUN:%d,AMN:%d,VOL:%d,PRC:%d\r\n",meterport-1,CPCCfueling->nzn,amount,volume,__REV16(CPCCfueling->PRC));					
					else						
						printf("NOT CASH TRADE! PORT:%d,GUN:%d,AMN:%d,VOL:%d,PRC:%d\r\n",meterport-1,CPCCfueling->nzn,amount,volume,__REV16(CPCCfueling->PRC));				
					               
                
                   
                    //�ڶ�����Ϣ����
                    CPCCcardInsert=(CPCCContentCardInsert*)&pDes->CPCCMsg.content[1+sizeof(CPCCContentFueling)];
                    
                    printf("PORT:%d,Card Insert! CardStus:%02X%02X CardNO:",meterport-1,(u8)(CPCCcardInsert->cardSt&0xFF),(u8)((CPCCcardInsert->cardSt>>8)&0xFF));
					 for(u8 i=0;i<10;i++)
						 printf("%02x ",CPCCcardInsert->asn[i]);
					
                    printf("Money:%u\r\n",__REV(CPCCcardInsert->bal));			
                            
                }
                
               // CPCCfueling->nzn=0;//���嵥ǹ��ǹ��ǿ��0���ݲ�֧�ֶ�ǹ
                GunMapNum= CPCCfueling->nzn % GunNumPerMeter;
                GunStateMgr[meterport-1].gunst.status|=(1<<GunMapNum);
                GunStateMgr[meterport-1].gunst.mark=0; 
				
                if(GunStateMgr[meterport-1].preMark!=GunStateMgr[meterport-1].gunst.mark)//�����ڵ����������ϱ�ǹ״̬
                {
                    GunStateMgr[meterport-1].NeedReport=1;
                    GunStateMgr[meterport-1].preMark=GunStateMgr[meterport-1].gunst.mark;
                }
                                
                if(pDes->gun[GunMapNum]==GUN_STATE_PUT)
                {
                    pDes->gun[GunMapNum]=GUN_STATE_PICK;
              //      SendGunStateReport(getWorkMode(),meterport-1, CPCCfueling->nzn, GUN_STATE_PICK);//����ǹ״̬��Ϣ
                }
                              
                                            
			}
            else  
            {
                 printf("PORT:%d,NO Card Insert,State hold!\r\n",meterport-1);
              /*  
                 GunStateMgr[meterport-1].gunst.status=0;//
                 GunStateMgr[meterport-1].gunst.mark=0;  
                 
                 if(GunStateMgr[meterport-1].preMark!=GunStateMgr[meterport-1].gunst.mark)//�����ڵ����������ϱ�ǹ״̬
                 {
                    GunStateMgr[meterport-1].NeedReport=1;
                    GunStateMgr[meterport-1].preMark=GunStateMgr[meterport-1].gunst.mark;
                 }
                */ 
            }                
			
		break;
			
		case CPCC_PROTOCOL_CMD_COMMON_QUERY: //��ͨ��ѯ����ǹ״̬��
				
		     printf("PORT:%d,common requry! State hold!\r\n",meterport-1);
             /*
		   //GunStateMgr[meterport-1].status&=~(1<<CPCCfueling->nzn);
		     GunStateMgr[meterport-1].gunst.status=0;//0x30ָ���в���ǹ�ţ��˴��Զ�ǹ״̬�޷��жϣ���
		     GunStateMgr[meterport-1].gunst.mark=0;  
			 
		     if(GunStateMgr[meterport-1].preMark!=GunStateMgr[meterport-1].gunst.mark)//�����ڵ����������ϱ�ǹ״̬
			 {
				GunStateMgr[meterport-1].NeedReport=1;
				GunStateMgr[meterport-1].preMark=GunStateMgr[meterport-1].gunst.mark;
			 }
			 */
			
/*������Ĺ�ǹ�жϣ�������
             for(u8 i=0;i<GunNumPerMeter;i++)
			 {
				 if(MMsgT[meterport-1].HaveMessageNeedReport[i]==1)
				 {
					 if((pDes->CPCCMsg.head.frameNo==FrameNo[i]+1) || FrameNo[i]==0)
					 {
						 FrameNo[i]=pDes->CPCCMsg.head.frameNo;						
						 delay_send[i]++;
						 
						 if(delay_send[i]>0)//�յ�1��0X30�ͷ���ǹ
						 {
							 delay_send[i]=0;
							 FrameNo[i]=0;
							 MMsgT[meterport-1].HaveMessageNeedReport[i]=0;
							// SendGunTransactionReport(PORTtoCB,i,&MMsgT[meterport-1].MeterMsgTRSMgr[i]);	//���ͽ�����Ϣ	
							 SendGunStatusReport(getWorkMode(),meterport-1, i, GUN_STATE_PUT);//����ǹ״̬��Ϣ	
						 }
				     }
					 else
					 {
						  delay_send[i]=0;
						  FrameNo[i]=0;
					 }
				 }
			 }
*/			 
		   
		break;
		default:
			//printf("T:%d ",getSysTick());
        printf("PORT:%d,other cmd:0x%02X\r\n",meterport-1,pDes->CPCCMsg.content[0]);
		break;
	}
	
}





void CPCCMessagerTask(void)
{
    unsigned char meterport=UART_METER_PORT_BASE;//�˴�meterport�ǳ���������ͻ������ڵĴ��ںţ������屾�����֧��1�����ͻ�������
	GunState_Meter *GunStatePtr;
	u8 *pbuf;
	u8 cc=0;
	
           
		CPCCMsgRcvDes *pDes=&CPCCMsgRcv[meterport];
		
		if(CPCCMessagerRetriever(meterport,pDes) == CPCC_MSG_RCV_COMPLETE)
		{
			
			pDes->state = CPCC_MSG_RCV_EMPTY;
			pDes->waitCount = 0;
						
			CPCCMsgProcess(meterport,pDes);   
            GunStateMgr[meterport-1].waitcnt=0;			
		}
		else//�������߼��
		{
			 GunStateMgr[meterport-1].waitcnt++;
			 if(GunStateMgr[meterport-1].waitcnt>=50)//5������û���յ����������ݣ���Ϊ�����ڶϿ�
			 {	 
				 GunStateMgr[meterport-1].gunst.mark=1;
				
				 GunStateMgr[meterport-1].waitcnt=0;
				 GunStateMgr[meterport-1].NeedReport=1;
			 }				 
		}
		//���´������ڼ���������״̬�����ı�ʱ����״̬��Ϣ
 /*
		if(GunStateMgr[meterport-1].NeedReport &&  GunStateMgr[meterport-1].preMark==0)
		{
				 GunStatePtr = &GunStateMgr[meterport-1];

				 GunStatePtr->gunst.head.preamble[0]='d';
			     GunStatePtr->gunst.head.preamble[1]='x';
				 GunStatePtr->gunst.head.len=sizeof(MeterResp)-3;	//ǰ����ͳ���λ,�����볤��
                 GunStatePtr->gunst.head.version=MeterProtocolRev;
			     GunStatePtr->gunst.head.msg=METER_RESP_STATUS;
			     GunStatePtr->gunst.head.seqNo=0;
			     GunStatePtr->gunst.port=meterport-1;	
			     GunStatePtr->gunst.mode=0;//getWorkMode();
				 GunStatePtr->preMark=GunStatePtr->gunst.mark;  
			
			     pbuf=(u8*)GunStatePtr;
			     pbuf+=2;//����ǰ����
			
			     for(u8 i=0;i<sizeof(MeterResp)-3;i++)  //ǰ�����cc,������У��
			       cc^= *pbuf++;
			
			     GunStatePtr->gunst.cc=cc;
			
				 uartWriteBuffer(PORTtoCB, (unsigned char *)GunStatePtr, sizeof(MeterResp));
			     uartSendBufferOut(PORTtoCB);
			
			     GunStateMgr[meterport-1].NeedReport=0;
		}	
*/		
		
    
                   
}



