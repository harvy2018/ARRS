/***************************************************************************************
File name: taxControlDev.c
Date: 2016-8-24
Author: Wangwei
Description:
     This file contain all the interface for taxcontroldevice to read initialization info and gasoline transaction.
     The lower support layer is uartPort.c since the datalink to this device is usart[0,3].
***************************************************************************************/
#include <string.h>
#include <stdlib.h>
#include "SEGGER_RTT.h"
#include "errorCode.h"
#include "gasolineDataStore_Retrieve.h"
#include "taxControlInterface.h"
#include "flashMgr.h"
#include "uartPort.h"
#include "board.h"
#include "localTime.h"
#include "time.h"
#include "transactionHelper.h"
#include "meterMessager.h"
#include "bsp_rtc.h"
#include "controlledTaxSampling.h"

#define   OneBoardGunNum  6


#define dbgPrintf   SEGGER_RTT_printf
#define MIN(a,b)    ((a<=b)?a:b)
#define MAX(a,b)    ((a>=b)?a:b)
#define OPPPSITE(a) ((a==0)?1:0)

// Static Argumnent Definition 
static TaxControlDevManager taxControlDevManager;
static TaxDevInitializationInfoMgr taxDevInitializationInfoMgr;
/************************************************************************************************
Function Name: getTaxControlDevManager()
Description:
     Get the pointer of "TaxControlDevManager"
Para:
     NULL
Return:
     The pointer.
************************************************************************************************/
TaxControlDevManager *  getTaxControlDevManager()
{
    return &taxControlDevManager;
}
/************************************************************************************************
Function Name: taxControlDeviceGetGunNoById
Description:
	Intends to get the # of gun controled by the tax control device ID
Parameter:
       id: MSB of tax control device ID(20 bytes of ASCII string according to SPEC)
return:
	Number of gun controled bu this device, 0 means unrecognized control device.

Infomation:
	KTK naming rule(CPUID, TAXID):    
	    (1) AXXX XXXX XXXX XXXX XXXX,	1 gun
	    (2) BXXX XXXX XXXX XXXX XXXX,	2 guns
	    (3) DXXX XXXX XXXX XXXX XXXX,	4 guns
	    (4) FXXX XXXX XXXX XXXX XXXX,	6 guns
	    (5) HXXX XXXX XXXX XXXX XXXX,	8 guns

	YTSF:(Not very solid definition)
	    (1) [0175600000,2007700000)    1 gun
	    (2) [2007700000,4001900000)    2 guns
	    (3) [4001900000,9999999999]    4 guns

*************************************************************************************************/
static  unsigned char taxControlDeviceGetGunNoById(unsigned char *pFactorySerialId)
{
	if(pFactorySerialId == 0)
		{
			return 0;
		}

	if((pFactorySerialId[0] >= 'A') && (pFactorySerialId[0] <='H'))	// KeTaiKang taxControlDevice ID naming SPEC
	{
		switch	(pFactorySerialId[0])
		{
			case 'A':
				return 1;
			case 'B':
				return 2;
			case 'D':
				return 4;
			case 'F':
				return 6;
			case 'H':
				return 8;
			default:				// Currently don't know yet.(2016-8-31)
				printf("taxControlDeviceGetGunNoById: Got invlid KTK taxID information\n");
				return 0;
		}		
	}
    else if((pFactorySerialId[0] >= '0') && (pFactorySerialId[0] <= '9'))
        {
            long long t;
            unsigned char c[11];
            c[10] = 0;
            memcpy(c,pFactorySerialId,10);
            t=atoll(c);

            if(t >= 4001900000LL)
                {
                    return 4;
                }    
            else if(t >= 2007700000)
                {
                    return 2;
                }
            else if(t >= 0175600000)
                {
                    return 1;
                }
            else
                {
                    return 0;
                }
                
        }
    else
        {
            return 0;
        }
	
}
/************************************************************************************************
Function Name: taxControlDeviceGetBrandById
Description:
	Intends to get the device branc by the 1st character of tax control device ID
Parameter:
      pFactorySerialId:[I], the unique ID of tax control device
return:
	Brand of device.

Infomation:
	KTK naming rule(CPUID, TAXID):    
	    (1) AXXX XXXX XXXX XXXX XXXX,	1 gun
	    (2) BXXX XXXX XXXX XXXX XXXX,	2 guns
	    (3) DXXX XXXX XXXX XXXX XXXX,	4 guns
	    (4) FXXX XXXX XXXX XXXX XXXX,	6 guns
	    (5) HXXX XXXX XXXX XXXX XXXX,	8 guns

	YTSF:(Not very solid definition)
	    (1) [0175600000,2007700000)    1 gun
	    (2) [2007700000,4001900000)    2 guns
	    (3) [4001900000,9999999999]    4 guns

*************************************************************************************************/
static  TaxControlDevBrand taxControlDeviceGetBrandById(unsigned char *pFactorySerialId)
{
    if(pFactorySerialId == 0)
		{
			return 0;
		}

    if((pFactorySerialId[0] >= 'A') && (pFactorySerialId[0] <='H'))
        return  TAX_CONTROL_DEVICE_KTK;
    else if((pFactorySerialId[0] >= '0') && (pFactorySerialId[0] <='9'))
        return  TAX_CONTROL_DEVICE_YTSF;
    else
        {
            //printf("taxControlDeviceGetBrandById: Got unsupported brand tax control device\r\n");            
            return  TAX_CONTROL_DEVICE_MAX;
        }    
}
/*************************************************************************************************
Name: taxControlCmdCreate
Description: Generate Tax control device query command
Para:
	cmd:[I], tax control device query command
	gunNo:[I], target gun number
	buf:[I/O], buffer to accomodate the generated command frame
	size:[I/O], the bytes number of generated command frame 
**************************************************************************************************/
static errorCode taxControlCmdCreate(unsigned char cmd, unsigned char gunNo,unsigned char *buf, unsigned short *size, unsigned char rtnMethode)
{
    LocalTimeStamp ltm;
    u32 timestick;
    struct tm *t;
    
    int	i=0;
	if((buf == 0) || (size == 0 ) || (gunNo >= TAX_CONTROL_DEVICE_GUNNO_MAX))
		return  E_INPUT_PARA;
	
	switch	(cmd)
		{
			case TAX_CONTROL_CMD_QUERY_INFO:
				{
					TaxInitInfoQueryCmd *pInit= (TaxInitInfoQueryCmd *)buf;
					memset((char *)pInit,0,sizeof(TaxInitInfoQueryCmd));
					pInit->header.cmd = TAX_CONTROL_CMD_QUERY_INFO;
					pInit->header.frameNo = 0xFF;
					pInit->header.preamable = TAX_CONTROL_PREAMABLE;
					pInit->header.length = sizeof(TaxInitInfoQueryCmd) - 2;    // length doesn't count preamable and itself
					pInit->gunNo = 0;									   // Query taxinfo by gunNo=0 is safe since each taxcontrol device at least has one gun
					pInit->cc = 0x0;

					for(i=2;i<(sizeof(TaxInitInfoQueryCmd)-1);i++)		// cc doesn't caculate preamable,length and itself
						pInit->cc = pInit->cc ^ *(buf+i); 	
					
					*size = sizeof(TaxInitInfoQueryCmd); 
					break;
				}
			
			case TAX_CONTROL_CMD_QUERY_ONCE:
				{
					TaxOnceQueryCmd* pOnce= (TaxOnceQueryCmd *)buf;
					memset((char *)pOnce,0,sizeof(TaxOnceQueryCmd));
					pOnce->header.cmd = TAX_CONTROL_CMD_QUERY_ONCE;
					pOnce->header.frameNo = 0xFF;
					pOnce->header.preamable = TAX_CONTROL_PREAMABLE;
					pOnce->header.length = sizeof(TaxOnceQueryCmd) - 2;    // length doesn't count preamable and itself
					pOnce->gunNo = gunNo;
					pOnce->rtnMethod = rtnMethode;
					pOnce->cc = 0x0;

					for(i=2;i<(sizeof(TaxOnceQueryCmd)-1);i++)		// cc doesn't caculate preamable,length and itself
						pOnce->cc = pOnce->cc ^ *(buf+i); 	
					
					*size = sizeof(TaxOnceQueryCmd); 	
					break;
				}	
			case TAX_CONTROL_CMD_QUERY_DAY:					// Not deploy currently, not need implement
			case TAX_CONTROL_CMD_QUERY_MONTH:      //wawe 2016-11-2 14:23:30
				{
                                    TaxMonthlyQueryCmd* pMonth= (TaxMonthlyQueryCmd *)buf;
                                    timestick=getLocalTick();
                                    timestick+=LOCAL_TIME_ZONE_SECOND_OFFSET;
                                    t = localtime((time_t *)&timestick);
                    
					if(t->tm_mon==0)                   
					{
						sprintf(ltm.year,"%4d", t->tm_year+REFERENCE_YEAR_BASE-1);
						sprintf(ltm.month,"%2d",12);  //²éµÄÊÇÉÏÔÂÀÛ¼Æ
					}               
					else
					{
						sprintf(ltm.year,"%4d", t->tm_year+REFERENCE_YEAR_BASE);
						sprintf(ltm.month,"%2d",t->tm_mon);  //²éµÄÊÇÉÏÔÂÀÛ¼Æ
					}
                                    memset((char *)pMonth,0,sizeof(TaxMonthlyQueryCmd));
                                    pMonth->header.cmd = TAX_CONTROL_CMD_QUERY_MONTH;
                                    pMonth->header.frameNo = 0xFF;
                                    pMonth->header.preamable = TAX_CONTROL_PREAMABLE;
                                    pMonth->header.length = sizeof(TaxMonthlyQueryCmd) - 2;    // length doesn't count preamable and itself
                                    pMonth->gunNo = gunNo;
                                    memcpy(pMonth->year,ltm.year,4);
                                    memcpy(pMonth->month,ltm.month,2);//²éÉÏÔÂÀÛ¼ÆÊý¾Ý
                                              
					pMonth->rtnMethod = rtnMethode;
					pMonth->cc = 0x00;

					for(i=2;i<(sizeof(TaxMonthlyQueryCmd)-1);i++)		// cc doesn't caculate preamable,length and itself
						pMonth->cc = pMonth->cc ^ (*(buf+i)); 	
					
					*size = sizeof(TaxMonthlyQueryCmd); 	
					break;
                    
				}
			case TAX_CONTROL_CMD_QUERY_TOTAL:
				{
					TaxTotalQueryCmd* pTotal= (TaxTotalQueryCmd *)buf;
					memset((char *)pTotal,0,sizeof(TaxTotalQueryCmd));
					pTotal->header.cmd = TAX_CONTROL_CMD_QUERY_TOTAL;
					pTotal->header.frameNo = 0xFF;
					pTotal->header.preamable = TAX_CONTROL_PREAMABLE;
					pTotal->header.length = sizeof(TaxTotalQueryCmd) - 2;    // length doesn't count preamable and itself
					pTotal->gunNo = gunNo;
					pTotal->amountMode = TAX_TOTAL_ACCOUNT_MODE_ACTUAL;
					pTotal->rtnMethod = rtnMethode;
					pTotal->cc = 0x00;

					for(i=2;i<(sizeof(TaxTotalQueryCmd)-1);i++)		// cc doesn't caculate preamable,length and itself
						pTotal->cc = pTotal->cc ^ (*(buf+i)); 	
					
					*size = sizeof(TaxTotalQueryCmd); 	
					break;
				}	
			
			default:
				return	E_NOT_SUPPORT;
			break;
			
		}
	return E_SUCCESS;
};

/*********************************************************************************************
Function Name: taxControlResponseRetriever
Description:
	This function intends to retrieve tax control's response.
Parameter:
	port:[I], uart port number;
	pRespBuf:[I/O], response message;
	len: Expected data length
Return:
	E_SUCCESS: when fetch number of data
	E_EMPTY:  the buffer is empty
	E_INPUT_PARA: At lease one input para is wrong. 
	E_FAIL: when fail to fetch expected number of data
**********************************************************************************************/
errorCode  taxControlResponseRetriever(unsigned port, void *pRespBuf,unsigned short len)
{	
	return (uartRead(port, pRespBuf, len)); 
}

/*******************************************************************************************************************************
Function Name: taxControlDevToPortBinding
Description:
      This function intends to bind tax control device to uart port after
      1. CPU reset;
      2. Detected uart port occurs re-connecting event.

      Currently, based on system design, only at most supports 2 tax control device and the uart port must be 0,1. After bind,  Private(static) "taxControlDevManager"
variable will be initialized and waiting for GTC sync, then ready for sampling.      

Parameter:
    bReq:[I], Bing request for PORT0,PORT1 or both of them
Return:
	E_SUCCESS:   Always return successfully.
	E_DEVICE:      Doesn't found any valid tax control device.  
*******************************************************************************************************************************/
errorCode taxControlDevToPortBinding(TaxControlDevBindingRequest bReq)
{
    unsigned char start,end;
    int i,k,n,port;
    unsigned short size,availBytes;
    unsigned char pCmdBuf[64];
    unsigned char pRespBuf[128];
    UartPortState *pUartPortState;
    TaxOfficialInitInfoQueryResp *pOfficialTaxResp;
    TaxFactoryInitInfoQueryResp *pFactoryTaxResp;
    TaxTotalQueryResp *pTotalResp;

    TaxControlBindingState bindingState[TAX_CONTROL_DEVICE_MAX_NUM];
    
    TaxControlDevManager *pTaxControlDevManager;
    TaxDevInitializationInfoMgr *pTaxDevInitializationInfoMgr =(TaxDevInitializationInfoMgr *)&taxDevInitializationInfoMgr;
        
    int retryCount[TAX_CONTROL_DEVICE_MAX_NUM]={0};
    int waitCount[TAX_CONTROL_DEVICE_MAX_NUM]={0};
    TaxHistoryTotalRecordInfo *pTaxHistoryTotalRecordInfo = getTaxHistoryTotalRecordInfo();
    unsigned char gunBaseNo = 0;   
    unsigned char gunInitialized[UART_TAX_PORT_NUM] ={0}; 
    char gunRepeatCount[UART_TAX_PORT_NUM] = {0xff};

    pTaxControlDevManager = &taxControlDevManager;
    pUartPortState = uartGetPortState();


    // based on the binding request, allocate the target start and end for uart_port/device handler
    switch(bReq)
        {
            case  TAX_CONTROL_DEV_BINDING_REQ_TOTAL:
                {
                    errorCode err=E_SUCCESS;
                    
                    start = 0;
                    end  = 1;
                  
                    bindingState[0] = TAX_CONTROL_BINDING_STATE_IDLE;
                    bindingState[1] = TAX_CONTROL_BINDING_STATE_IDLE;

                    pUartPortState[0].bState = PORT_UNBOUND;
                    pUartPortState[1].bState = PORT_UNBOUND;

                    pTaxControlDevManager->taxControlDevInfo[0].bind = TAX_CONTROL_DEVICE_UNBOUND;
                    pTaxControlDevManager->taxControlDevInfo[1].bind = TAX_CONTROL_DEVICE_UNBOUND;

                    pTaxControlDevManager->taxControlDevInfo[0].gunBaseNumber = TAX_CONTROL_MACHINE_GUNNO_MAX;
                    pTaxControlDevManager->taxControlDevInfo[1].gunBaseNumber = TAX_CONTROL_MACHINE_GUNNO_MAX;


                    err = RetrieveTaxDevInitializationInfo(0,(unsigned char *)&pTaxDevInitializationInfoMgr->dev[0],sizeof(TaxDevInitializationInfo));
                    if(err == E_SUCCESS)
                        {
                            if(!memcmp(pTaxDevInitializationInfoMgr->dev[0].magic,"TINI",4))
                                {
                                    bindingState[0] = TAX_CONTROL_BINDING_STATE_RETRIEVE;
                                    printf("taxControlDevToPortBinding: Successfully Retrieve device 0's initialization info from flash");
                                }    
                        }
                    else
                        {
                            printf("taxControlDevToPortBinding: Can't  Retrieve device 0's initialization info from flash");                   
                        }

                    
                    err = RetrieveTaxDevInitializationInfo(1,(unsigned char *)&pTaxDevInitializationInfoMgr->dev[1],sizeof(TaxDevInitializationInfo));
                    if(err == E_SUCCESS)
                        {
                            if(!memcmp(pTaxDevInitializationInfoMgr->dev[1].magic,"TINI",4))
                                {
                                    bindingState[1] = TAX_CONTROL_BINDING_STATE_RETRIEVE;
                                    printf("taxControlDevToPortBinding: Successfully Retrieve device 1's initialization info from flash");
                                }    
                        }
                    else
                        {
                            printf("taxControlDevToPortBinding: Can't  Retrieve device 1's initialization info from flash");
                        }                
                    }
            break;

            case  TAX_CONTROL_DEV_BINDING_REQ_PORT0:
                {
                    errorCode   err = E_SUCCESS;
                    
                    start =0;
                    end  =0;
                    bindingState[0] = TAX_CONTROL_BINDING_STATE_IDLE;
                    bindingState[1] = TAX_CONTROL_BINDING_STATE_COMPLETE;

                    pUartPortState[0].bState = PORT_UNBOUND;
                    pTaxControlDevManager->taxControlDevInfo[0].bind = TAX_CONTROL_DEVICE_UNBOUND;
                    pTaxControlDevManager->taxControlDevInfo[0].gunBaseNumber = TAX_CONTROL_MACHINE_GUNNO_MAX;

                    err = RetrieveTaxDevInitializationInfo(0,(unsigned char *)&pTaxDevInitializationInfoMgr->dev[0],sizeof(TaxDevInitializationInfo));
                    if(err == E_SUCCESS)
                        {
                            if(!memcmp(pTaxDevInitializationInfoMgr->dev[0].magic,"TINI",4))
                                {
                                    bindingState[0] = TAX_CONTROL_BINDING_STATE_RETRIEVE;
                                    printf("taxControlDevToPortBinding: Successfully Retrieve device 0's initialization info from flash");
                                }    
                        }
                    else
                        {
                            printf("taxControlDevToPortBinding: Can't  Retrieve device 0's initialization info from flash");                   
                        }
                }
            break;

            case  TAX_CONTROL_DEV_BINDING_REQ_PORT1:
                {
                    errorCode   err = E_SUCCESS;
                    
                    start = 1;
                    end =  1;
                    bindingState[1] = TAX_CONTROL_BINDING_STATE_IDLE;
                    bindingState[0] = TAX_CONTROL_BINDING_STATE_COMPLETE;
                    
                    pUartPortState[1].bState = PORT_UNBOUND;
                    pTaxControlDevManager->taxControlDevInfo[1].bind = TAX_CONTROL_DEVICE_UNBOUND;
                    pTaxControlDevManager->taxControlDevInfo[1].gunBaseNumber = TAX_CONTROL_MACHINE_GUNNO_MAX;

                    err = RetrieveTaxDevInitializationInfo(1,(unsigned char *)&pTaxDevInitializationInfoMgr->dev[1],sizeof(TaxDevInitializationInfo));
                    if(err == E_SUCCESS)
                        {
                            if(!memcmp(pTaxDevInitializationInfoMgr->dev[1].magic,"TINI",4))
                                {
                                    bindingState[1] = TAX_CONTROL_BINDING_STATE_RETRIEVE;
                                    printf("taxControlDevToPortBinding: Successfully Retrieve device 1's initialization info from flash");
                                }    

                        }
                    else
                        {
                            printf("taxControlDevToPortBinding: Can't  Retrieve device 1's initialization info from flash");
                        }
                }
            break;

            default:
                {
                    printf("taxControlDevToPortBinding: Unsupported binding request:%d\r\n",bReq);
                    return E_FAIL;
                }            
            break;    
        }

    printf("taxControlDevToPortBinding: Request:%d start.........\r\n",bReq);
    while((bindingState[0] != TAX_CONTROL_BINDING_STATE_COMPLETE) || (bindingState[1] != TAX_CONTROL_BINDING_STATE_COMPLETE))
        {
            for(port=start;port<=end;port++)
                {               
                     switch  (bindingState[port])
                      {
                            case TAX_CONTROL_BINDING_STATE_IDLE:
                                {
                                    taxControlCmdCreate(TAX_CONTROL_CMD_QUERY_INFO,0,pCmdBuf,&size,TAX_RESPONSE_RETURN_PLAINTEXT);
                                    uartWriteBuffer(port, pCmdBuf,size);
                                    uartSendBufferOut(port);
                                    bindingState[port] = TAX_CONTROL_BINDING_STATE_NEW;
                                                                                 
                                    bsp_DelayMS(1000);
                                      
                                     // Send out a "QUERY ONCE" Command intends to recover the display
                                     for(i=0;i<OneBoardGunNum;i++)
                                      {
                                        taxControlCmdCreate(TAX_CONTROL_CMD_QUERY_ONCE,i,pCmdBuf,&size,TAX_RESPONSE_RETURN_DISPLAY);
                                        uartWriteBuffer(port, pCmdBuf,size);
                                        uartSendBufferOut(port);	
                                        bsp_DelayMS(1000);  
                                      }                                    
                                   
                                        printf("taxControlDevToPortBinding: Port %d sent query_tax_control_info command\r\n",port);
                                }
                            break;

                              
                        case    TAX_CONTROL_BINDING_STATE_NEW:
                                {
                                    waitCount[port]++;
                                    availBytes=uartGetAvailBufferedDataNum(port);
                                    if((availBytes >= sizeof(TaxOfficialInitInfoQueryResp)) || (availBytes>=sizeof(TaxFactoryInitInfoQueryResp)))
                                    {
                                            taxControlResponseRetriever(port,pRespBuf,availBytes);
                                            if((availBytes >= sizeof(TaxOfficialInitInfoQueryResp)) && (*(pRespBuf+sizeof(TaxControlHeader)) == 1))//status==1,±íÃ÷ÒÑ¾­³õÊ¼»¯¹ý
                                            {
                                                pOfficialTaxResp= (TaxOfficialInitInfoQueryResp *)pRespBuf;
                                                unsigned char cc = 0x00;
                                          
                                               
                                                for(i=0;i<pOfficialTaxResp->header.length-1;i++)
                                                    cc = cc ^ pRespBuf[i+2];
                                                 
                                                
                                                if((pOfficialTaxResp->header.cmd == TAX_CONTROL_CMD_QUERY_INFO) && (pOfficialTaxResp->taxOrgInitInfo.cc1== cc))	
                                                {

                                                    unsigned char tt[11]={0};   
                                                    pTaxControlDevManager->taxControlDevInfo[port].bind = TAX_CONTROL_DEVICE_BOUND;
                                                    pTaxControlDevManager->taxControlDevInfo[port].port = port;
                                                    pTaxControlDevManager->taxControlDevInfo[port].initLevel = TAX_CONTROL_DEVICE_INIT_OFFICIAL;
                                                    memcpy(pTaxControlDevManager->taxControlDevInfo[port].serialID,pOfficialTaxResp->taxOrgInitInfo.factorySerialNo,10);
                                                    memcpy(pTaxControlDevManager->taxControlDevInfo[port].taxUniqueID,pOfficialTaxResp->taxOrgInitInfo.taxCode,19);
                                                    pTaxControlDevManager->taxControlDevInfo[port].taxUniqueID[19]=pOfficialTaxResp->taxOrgInitInfo.taxCode20;                                                    
                                                    pTaxControlDevManager->taxControlDevInfo[port].brand = taxControlDeviceGetBrandById(pTaxControlDevManager->taxControlDevInfo[port].serialID);
                                                    pTaxControlDevManager->taxControlDevInfo[port].gunTotal = taxControlDeviceGetGunNoById(pTaxControlDevManager->taxControlDevInfo[port].serialID);
                                                    pTaxControlDevManager->taxControlDevInfo[port].gunCurrent = 0;
                                                    pTaxControlDevManager->taxControlDevInfo[port].curCmd = TAX_CONTROL_CMD_QUERY_TOTAL;
                                                    pTaxControlDevManager->taxControlDevInfo[port].stateCount = 0;
                                                    pTaxControlDevManager->taxControlDevInfo[port].sampleState= TAX_CONTROL_SAMPLING_STATE_INIT;
                                                    pTaxControlDevManager->taxControlDevInfo[port].gunRepeatCount = -1;

                                                    memcpy(tt,pTaxControlDevManager->taxControlDevInfo[port].serialID,10);
                                                    printf("taxControlDevToPortBinding: Port:%d TotalGun:%d,get official tax info:%s\r\n",port,pTaxControlDevManager->taxControlDevInfo[port].gunTotal,tt);                                                   
                                                    
                                                    for(k=0;k<TAX_CONTROL_MACHINE_GUNNO_MAX;k++)//²éÕÒË°¿ØIDÏàÍ¬µÄ¼ÇÂ¼
                                                    {
                                                        if(pTaxHistoryTotalRecordInfo[k].valid == TAX_CONTROL_HISTORY_RECORD_VALID)
                                                        {                                                                                
                                                            
                                                            if(!memcmp(pTaxHistoryTotalRecordInfo[k].record.serialID,pOfficialTaxResp->taxOrgInitInfo.factorySerialNo,10))                                  
                                                                break;
                                                         
                                                        }
                                                    }
                                                          
                                                   if(k<TAX_CONTROL_MACHINE_GUNNO_MAX) // Found matched record, then we can bind current dev control instance to the current port.
                                                    {                          
                                                     
                                                        // Fresh device, need initialize the previous "total" data to an predefinced invalid value
                                                        for(n=0;n<pTaxControlDevManager->taxControlDevInfo[port].gunTotal;n++)
                                                        {
                                                             pTaxControlDevManager->taxControlDevInfo[port].gunSubState[n]= TAX_CONTROL_PROCESS_SUB_STATE_INIT;                                                               
                                                        }                                                                     
                                                        
                                                         //¼ÆÁ¿Ä£Ê½ÏÂ²»´ÓFLASHÖÐ¶ÁÈ¡ÀúÊ·×ÜÀÛ
                                                        if( pTaxHistoryTotalRecordInfo[k].record.gunNo == 0 && getMeterBoardMode()!=METER_MODE) 
                                                        {   // Successfully restore the total data from the flash, so, we set the gun initialized.
                                                            for(n=0;n<pTaxControlDevManager->taxControlDevInfo[port].gunTotal;n++)
                                                            {
                                                                if(pTaxHistoryTotalRecordInfo[k+n].valid == TAX_CONTROL_HISTORY_RECORD_VALID)
                                                                {
                                                                    pTaxControlDevManager->taxControlDevInfo[port].preData[n].totalAmount = pTaxHistoryTotalRecordInfo[k+n].record.totalAmount;
                                                                    pTaxControlDevManager->taxControlDevInfo[port].preData[n].totalVolume = pTaxHistoryTotalRecordInfo[k+n].record.totalVolume;
                                                                    pTaxControlDevManager->taxControlDevInfo[port].preData[n].timeStamp =pTaxHistoryTotalRecordInfo[k+n].record.timeStamp;
                                                                    pTaxControlDevManager->taxControlDevInfo[port].gunSubState[n]= TAX_CONTROL_PROCESS_SUB_STATE_NORMAL;
                                                                    printf("taxControlDevToPortBinding:Port:%d Gun:%d recovered _Total_",port,n);
                                                                    printf("[Volume:%lld",pTaxControlDevManager->taxControlDevInfo[port].preData[n].totalVolume);
                                                                    printf(" Amount:%lld]\r\n",pTaxControlDevManager->taxControlDevInfo[port].preData[n].totalAmount);                                                                              
                                                                 }                                                                      
                                                             }
                                                            pTaxControlDevManager->taxControlDevInfo[port].gunBaseNumber = k;
                                                        }
                                                        else if(getMeterBoardMode()==METER_MODE)
                                                        {
                                                            printf("taxControlDevToPortBinding: MeterMode don't use flash _total_data!\r\n");
                                                        }
                                                        else
                                                        {
                                                            printf("taxControlDevToPortBinding: But Gun No doesn't match, _total_data was not recovered,k=%d,\r\n",k);
                                                        }
                                                                                                                                                         
                                                 
                                                    }
                                                    else
                                                    {
                                                            TaxDevInfo  devInfo;
                                                            printf("taxControlDevToPortBinding: New Tax Control Device founded k=%d,\r\n",k);
                                                            devInfo.head.mark = DIRTY;
                                                            devInfo.head.type  = 0;
                                                            devInfo.head.reserved1 = 0;
                                                            devInfo.head.reserved2 = 0;

                                                            memcpy(devInfo.content.serialNum, pOfficialTaxResp->taxOrgInitInfo.factorySerialNo,10);
                                                            memcpy(devInfo.content.gunNum,pOfficialTaxResp->taxOrgInitInfo.gunCode,2);
                                                            memcpy(devInfo.content.taxerRegNum,pOfficialTaxResp->taxOrgInitInfo.taxCode,19);
                                                            devInfo.content.taxerRegNum[19]= pOfficialTaxResp->taxOrgInitInfo.taxCode20;
                                                            memcpy(devInfo.content.oilGrade,pOfficialTaxResp->taxOrgInitInfo.oilGrade,4);
                                                            memcpy(devInfo.content.year,pOfficialTaxResp->taxOrgInitInfo.year,4);                                                        
                                                            memcpy(devInfo.content.month,pOfficialTaxResp->taxOrgInitInfo.month,2);                                                        
                                                            memcpy(devInfo.content.day,pOfficialTaxResp->taxOrgInitInfo.day,2);                                                        
                                                            memcpy(devInfo.content.hour,pOfficialTaxResp->taxOrgInitInfo.hour,2);                                                        
                                                            memcpy(devInfo.content.minute,pOfficialTaxResp->taxOrgInitInfo.minute,2);                                                        
                                                            devInfo.content.PortNum = port;
                                                            devInfo.content.timeStamp = 0;

                                                             // Store the new tax device's info into flash   
                                                            storeTaxDevInfoRecord(&devInfo);                               
                                                            printf("taxControlDevToPortBinding: New Tax Control Device founded k=%d\r\n",k);
                                                    }                                                
                                                    pUartPortState[port].bState= PORT_BOUNDED;
                                                    bindingState[port]=TAX_CONTROL_BINDING_STATE_COMPLETE;

                                                    // Store this device's initialization info flash for retrieveing in the future if reset
                                                    memcpy(pTaxDevInitializationInfoMgr->dev[port].factorySerialNo, pTaxControlDevManager->taxControlDevInfo[port].serialID,10);
                                                    memcpy(pTaxDevInitializationInfoMgr->dev[port].taxCode,pTaxControlDevManager->taxControlDevInfo[port].taxUniqueID,20);
                                                    memcpy(pTaxDevInitializationInfoMgr->dev[port].gunCode,pOfficialTaxResp->taxOrgInitInfo.gunCode,2);
                                                    memcpy(pTaxDevInitializationInfoMgr->dev[port].oilGrade,pOfficialTaxResp->taxOrgInitInfo.oilGrade,4);
                                                    memcpy(pTaxDevInitializationInfoMgr->dev[port].minute,pOfficialTaxResp->taxOrgInitInfo.minute,2);
                                                    memcpy(pTaxDevInitializationInfoMgr->dev[port].hour,pOfficialTaxResp->taxOrgInitInfo.hour,2);
                                                    memcpy(pTaxDevInitializationInfoMgr->dev[port].day,pOfficialTaxResp->taxOrgInitInfo.day,2);
                                                    memcpy(pTaxDevInitializationInfoMgr->dev[port].month,pOfficialTaxResp->taxOrgInitInfo.month,2);
                                                    memcpy(pTaxDevInitializationInfoMgr->dev[port].year,pOfficialTaxResp->taxOrgInitInfo.year,4);
                                                    memcpy(pTaxDevInitializationInfoMgr->dev[port].magic,"TINI",4);

                                                    StoreTaxDevInitializationInfo(port,(unsigned char *)&pTaxDevInitializationInfoMgr->dev[port], sizeof(TaxDevInitializationInfo));
                                                    
                                            }
                                            else
                                            {
                                                waitCount[port] = 0;
                                                retryCount[port]++;
                                                if(retryCount[port] >=3)
                                                {
                                                        printf("taxControlDevToPortBinding: Port %d meet 3 times CC/Messagetype error @ NEW state, Stop binding\r\n",port);
                                                        bindingState[port] = TAX_CONTROL_BINDING_STATE_COMPLETE;                                                                
                                                }
                                                else
                                                {
                                                        waitCount[port] = 0;
                                                        taxControlCmdCreate(TAX_CONTROL_CMD_QUERY_INFO,0,pCmdBuf,&size,TAX_RESPONSE_RETURN_PLAINTEXT);
                                                        uartWriteBuffer(port, pCmdBuf,size);
                                                        uartSendBufferOut(port);
                                                    
                                                        bsp_DelayMS(1000);
                                                        for(i=0;i<OneBoardGunNum;i++)
                                                          {
                                                             taxControlCmdCreate(TAX_CONTROL_CMD_QUERY_ONCE,i,pCmdBuf,&size,TAX_RESPONSE_RETURN_DISPLAY);
                                                             uartWriteBuffer(port, pCmdBuf,size);
                                                             uartSendBufferOut(port);	
                                                             bsp_DelayMS(1000);  
                                                          }    
                                                    
                                                        printf("taxControlDevToPortBinding: Port %d meet CC/Messagetype error @ NEW state, try more\r\n",port);
                                                }
                                                 
                                            }                                           
                                            
                                        }
                                        else//Ë°¿ØÎ´³õÊ¼»¯¹ý
                                        {
                                            pFactoryTaxResp= (TaxFactoryInitInfoQueryResp *)pRespBuf;
                                            unsigned char cc = 0x00;
                                            for(i=0;i<pFactoryTaxResp->header.length-1;i++)
                                                    cc = cc ^ pRespBuf[i+2];
                                             								
    										
                                            if((pFactoryTaxResp->header.cmd == TAX_CONTROL_CMD_QUERY_INFO) && (pFactoryTaxResp->cc== cc))	
                                                {
                                                    unsigned char tt[11]={0};   

                                                    pTaxControlDevManager->taxControlDevInfo[port].bind = TAX_CONTROL_DEVICE_BOUND;
                                                    pTaxControlDevManager->taxControlDevInfo[port].port = port;
                                                    pTaxControlDevManager->taxControlDevInfo[port].initLevel = TAX_CONTROL_DEVICE_INIT_FACTORY;
                                                    memcpy(pTaxControlDevManager->taxControlDevInfo[port].serialID,pFactoryTaxResp->factorySerialNo,10);
                                                    memcpy(pTaxControlDevManager->taxControlDevInfo[port].taxUniqueID,TAX_CONTROL_DEVICE_DXYS_DEBUG_TAX_ID,20);
                                                    pTaxControlDevManager->taxControlDevInfo[port].brand = taxControlDeviceGetBrandById(pTaxControlDevManager->taxControlDevInfo[port].serialID);
                                                    pTaxControlDevManager->taxControlDevInfo[port].gunTotal = taxControlDeviceGetGunNoById(pTaxControlDevManager->taxControlDevInfo[port].serialID);
                                                    pTaxControlDevManager->taxControlDevInfo[port].gunCurrent = 0;
                                                    pTaxControlDevManager->taxControlDevInfo[port].curCmd = TAX_CONTROL_CMD_QUERY_TOTAL;
                                                    pTaxControlDevManager->taxControlDevInfo[port].stateCount = 0;
                                                    pTaxControlDevManager->taxControlDevInfo[port].sampleState= TAX_CONTROL_SAMPLING_STATE_INIT;
                                                    pTaxControlDevManager->taxControlDevInfo[port].gunRepeatCount = -1;

                                                    memcpy(tt,pTaxControlDevManager->taxControlDevInfo[port].serialID,10);
                                                    
                                                   // printf("taxControlDevToPortBinding: Port:%d get factory tax info:%s \r\n",port,tt);
                                                    printf("taxControlDevToPortBinding: Port:%d TotalGun:%d,get factory tax info:%s\r\n",port,pTaxControlDevManager->taxControlDevInfo[port].gunTotal,tt);                                                   
                                                   
                                                    for(k=0;k<TAX_CONTROL_MACHINE_GUNNO_MAX;k++)
                                                        {
                                                            if(pTaxHistoryTotalRecordInfo[k].valid == TAX_CONTROL_HISTORY_RECORD_VALID)
                                                                {                                                                    
                                                                    if(!memcmp(pTaxHistoryTotalRecordInfo[k].record.serialID,pFactoryTaxResp->factorySerialNo,10))                                  
                                                                        break;
                                                                 
                                                               }
                                                        }                                                                                                       

                                                   if(k<TAX_CONTROL_MACHINE_GUNNO_MAX) // Found matched record, then we can bind current dev control instance to the current port.
                                                       {
                                                            
                                                             // Here, not read the 1st "total" data out, so set "uninitialized" here
                                                            for(n=0;n<pTaxControlDevManager->taxControlDevInfo[port].gunTotal;n++)
                                                            {
                                                                        pTaxControlDevManager->taxControlDevInfo[port].gunSubState[n]= TAX_CONTROL_PROCESS_SUB_STATE_INIT;
                                                            }
                                                            
                                                             //¼ÆÁ¿Ä£Ê½ÏÂ²»´ÓFLASHÖÐ¶ÁÈ¡ÀúÊ·×ÜÀÛ 
                                                            if( pTaxHistoryTotalRecordInfo[k].record.gunNo == 0 && getMeterBoardMode()!=METER_MODE)
                                                            {   // Successfully restore the total data from the flash, so, we set the gun initialized.
                                                                for(n=0;n<pTaxControlDevManager->taxControlDevInfo[port].gunTotal;n++)
                                                                    {
                                                                        if(pTaxHistoryTotalRecordInfo[k+n].valid == TAX_CONTROL_HISTORY_RECORD_VALID)
                                                                            {
                                                                                    pTaxControlDevManager->taxControlDevInfo[port].preData[n].totalAmount = pTaxHistoryTotalRecordInfo[k+n].record.totalAmount;
                                                                                    pTaxControlDevManager->taxControlDevInfo[port].preData[n].totalVolume = pTaxHistoryTotalRecordInfo[k+n].record.totalVolume;
                                                                                    pTaxControlDevManager->taxControlDevInfo[port].preData[n].timeStamp =pTaxHistoryTotalRecordInfo[k+n].record.timeStamp;
                                                                                    pTaxControlDevManager->taxControlDevInfo[port].gunSubState[n]= TAX_CONTROL_PROCESS_SUB_STATE_NORMAL;
                                                                                    printf("taxControlDevToPortBinding:Port:%d Gun:%d recovered _Total_",port,n);
                                                                                    printf("[Volume:%lld",pTaxControlDevManager->taxControlDevInfo[port].preData[n].totalVolume);
                                                                                    printf(" Amount:%lld]\r\n",pTaxControlDevManager->taxControlDevInfo[port].preData[n].totalAmount);
                                                                             }                                      
                                                                     }
                                                                pTaxControlDevManager->taxControlDevInfo[port].gunBaseNumber = k;    // start sector to store the history _total_data for the guns of this device 
                                                            }
                                                            else if(getMeterBoardMode()==METER_MODE)
                                                            {
                                                                printf("taxControlDevToPortBinding: MeterMode don't use flash _total_data!\r\n");
                                                            }
                                                            else
                                                            {
                                                                 printf("taxControlDevToPortBinding: But Gun No doesn't match, _total_data was not recovered,k=%d,\r\n",k);
                                                            }                                                                                               
                                                                                                                       
                                                        } 
                                                    else
                                                        {
                                                             TaxDevInfo  devInfo;
                                                             printf("taxControlDevToPortBinding: New Tax Control Device founded k=%d\r\n",k);
                                                             devInfo.head.mark = DIRTY;
                                                             devInfo.head.type  = 0;
                                                             devInfo.head.reserved1 = 0;
                                                             devInfo.head.reserved2 = 0;

                                                             memcpy(devInfo.content.serialNum, pFactoryTaxResp->factorySerialNo,10);
                                                             devInfo.content.gunNum[0]=0;
                                                             devInfo.content.gunNum[1]=1;
                                                             memcpy(devInfo.content.taxerRegNum,TAX_CONTROL_DEVICE_DXYS_DEBUG_TAX_ID,20);
                                                             memset(devInfo.content.oilGrade,0x0,4);

                                                             // Default to 2016.1.1 00:01
                                                             devInfo.content.year[0]=0x32;
                                                             devInfo.content.year[1]=0x30;
                                                             devInfo.content.year[2]=0x31;
                                                             devInfo.content.year[3]=0x36;

                                                             devInfo.content.month[0]=0x30;
                                                             devInfo.content.month[1]=0x31;
                                                             
                                                             devInfo.content.day[0]=0x30;
                                                             devInfo.content.day[1]=0x31;
                                                             
                                                             devInfo.content.hour[0]=0x30;
                                                             devInfo.content.hour[1]=0x30;
                                                             
                                                             devInfo.content.minute[0]=0x30;
                                                             devInfo.content.minute[1]=0x31;
                                                             devInfo.content.PortNum = port;
                                                             devInfo.content.timeStamp = 0;

                                                             // Store the new tax device's info into flash   
                                                             storeTaxDevInfoRecord(&devInfo);
                                                        }  
                                                    
                                                    pUartPortState[port].bState= PORT_BOUNDED;
                                                    bindingState[port]=TAX_CONTROL_BINDING_STATE_COMPLETE;


                                                     // Store this device's initialization info flash for retrieveing in the future if reset
                                                    memcpy(pTaxDevInitializationInfoMgr->dev[port].factorySerialNo, pTaxControlDevManager->taxControlDevInfo[port].serialID,10);
                                                    memcpy(pTaxDevInitializationInfoMgr->dev[port].taxCode,TAX_CONTROL_DEVICE_DXYS_DEBUG_TAX_ID,20);
                                                    memset(pTaxDevInitializationInfoMgr->dev[port].gunCode,0,2);
                                                    memset(pTaxDevInitializationInfoMgr->dev[port].oilGrade,0,4);
                                                    pTaxDevInitializationInfoMgr->dev[port].minute[0] =0x30;
                                                    pTaxDevInitializationInfoMgr->dev[port].minute[1] =0x31;

                                                    pTaxDevInitializationInfoMgr->dev[port].hour[0] = 0x30;
                                                    pTaxDevInitializationInfoMgr->dev[port].hour[1] = 0x31;
                                                    
                                                    pTaxDevInitializationInfoMgr->dev[port].day[0]  = 0x30;
                                                    pTaxDevInitializationInfoMgr->dev[port].day[0]  = 0x31;
                                                    
                                                    pTaxDevInitializationInfoMgr->dev[port].month[0] = 0x30;
                                                    pTaxDevInitializationInfoMgr->dev[port].month[1] = 0x31;
                                                    
                                                    pTaxDevInitializationInfoMgr->dev[port].year[0] = 0x32;
                                                    pTaxDevInitializationInfoMgr->dev[port].year[1] = 0x30;
                                                    pTaxDevInitializationInfoMgr->dev[port].year[2] = 0x31;
                                                    pTaxDevInitializationInfoMgr->dev[port].year[3] = 0x36;
                                                    
                                                    memcpy(pTaxDevInitializationInfoMgr->dev[port].magic,"TINI",4);

                                                    StoreTaxDevInitializationInfo(port,(unsigned char *)&pTaxDevInitializationInfoMgr->dev[port], sizeof(TaxDevInitializationInfo));
                                                        
                                                }
                                            else
                                                {
                                                    waitCount[port] = 0;
                                                    retryCount[port]++;
                                                    if(retryCount[port] >=3)
                                                        {
                                                                printf("taxControlDevToPortBinding: Port %d meet 3 times CC/Messagetype error @ NEW state, Stop binding\r\n",port);
                                                                bindingState[port] = TAX_CONTROL_BINDING_STATE_COMPLETE;                                                                
                                                        }
                                                    else
                                                        {
                                                                waitCount[port] = 0;
                                                                taxControlCmdCreate(TAX_CONTROL_CMD_QUERY_INFO,0,pCmdBuf,&size,TAX_RESPONSE_RETURN_PLAINTEXT);
                                                                uartWriteBuffer(port, pCmdBuf,size);
                                                                uartSendBufferOut(port);
                                                            
                                                              bsp_DelayMS(1000);

                                                              for(i=0;i<OneBoardGunNum;i++)
                                                              {
                                                                taxControlCmdCreate(TAX_CONTROL_CMD_QUERY_ONCE,i,pCmdBuf,&size,TAX_RESPONSE_RETURN_DISPLAY);
                                                                uartWriteBuffer(port, pCmdBuf,size);
                                                                uartSendBufferOut(port);	
                                                                bsp_DelayMS(1000);
                                                              }   
                                                              printf("taxControlDevToPortBinding: Port %d meet CC/Messagetype error @ NEW state, try more\r\n",port);
                                                        }
                                                }
                                          }
                    																		
                    																				
                    			}
                                else
                                {
                                    if(waitCount[port]>=2)
                                        {
                                            retryCount[port]++;
                                            waitCount[port] = 0;
                                            if(retryCount[port] >=3)
                                                {
                                                        printf("taxControlDevToPortBinding: Busy 3 times @ NEW state, Stop binding this port and device\r\n",port);
                                                        bindingState[port]= TAX_CONTROL_BINDING_STATE_COMPLETE;
                                                }
                                            else
                                                {
                                                        waitCount[port] = 0;
                                                        taxControlCmdCreate(TAX_CONTROL_CMD_QUERY_INFO,0,pCmdBuf,&size,TAX_RESPONSE_RETURN_PLAINTEXT);
                                                        uartWriteBuffer(port, pCmdBuf,size);
                                                        uartSendBufferOut(port);
                                                    
                                                         bsp_DelayMS(1000);
                                                         
                                                         for(i=0;i<OneBoardGunNum;i++)
                                                          {
                                                             taxControlCmdCreate(TAX_CONTROL_CMD_QUERY_ONCE,i,pCmdBuf,&size,TAX_RESPONSE_RETURN_DISPLAY);
                                                             uartWriteBuffer(port, pCmdBuf,size);
                                                             uartSendBufferOut(port);	
                                                             bsp_DelayMS(1000);  
                                                          }    			
                                                
                                                        printf("taxControlDevToPortBinding: Busy @ NEW state, Port %d will try more within NEW state\r\n",port);                                                        
                                                }
                                        }
                                }                                
                        }
                       break;

                       case    TAX_CONTROL_BINDING_STATE_RETRIEVE:
                            {
                                unsigned char tt[11]={0};
                                pTaxControlDevManager->taxControlDevInfo[port].bind = TAX_CONTROL_DEVICE_BOUND;
                                pTaxControlDevManager->taxControlDevInfo[port].port = port;
                                pTaxControlDevManager->taxControlDevInfo[port].initLevel = TAX_CONTROL_DEVICE_INIT_OFFICIAL;
                                memcpy(pTaxControlDevManager->taxControlDevInfo[port].serialID,pTaxDevInitializationInfoMgr->dev[port].factorySerialNo,10);
                                memcpy(pTaxControlDevManager->taxControlDevInfo[port].taxUniqueID,pTaxDevInitializationInfoMgr->dev[port].taxCode,20);
                                pTaxControlDevManager->taxControlDevInfo[port].brand = taxControlDeviceGetBrandById(pTaxControlDevManager->taxControlDevInfo[port].serialID);
                                pTaxControlDevManager->taxControlDevInfo[port].gunTotal = taxControlDeviceGetGunNoById(pTaxControlDevManager->taxControlDevInfo[port].serialID);
                                pTaxControlDevManager->taxControlDevInfo[port].gunCurrent = 0;
                                pTaxControlDevManager->taxControlDevInfo[port].curCmd = TAX_CONTROL_CMD_QUERY_TOTAL;
                                pTaxControlDevManager->taxControlDevInfo[port].stateCount = 0;
                                pTaxControlDevManager->taxControlDevInfo[port].sampleState= TAX_CONTROL_SAMPLING_STATE_INIT;
                                pTaxControlDevManager->taxControlDevInfo[port].gunRepeatCount = -1;
                                memcpy(tt,pTaxControlDevManager->taxControlDevInfo[port].serialID,10);
                                printf("taxControlDevToPortBinding[Retrieve]: Port:%d TotalGun:%d,get official tax info:%s\r\n",port,pTaxControlDevManager->taxControlDevInfo[port].gunTotal,tt);

                                for(k=0;k<TAX_CONTROL_MACHINE_GUNNO_MAX;k++)//²éÕÒË°¿ØIDÏàÍ¬µÄ¼ÇÂ¼
                                    {
                                        if(pTaxHistoryTotalRecordInfo[k].valid == TAX_CONTROL_HISTORY_RECORD_VALID)
                                            {
                                                if(!memcmp(pTaxHistoryTotalRecordInfo[k].record.serialID,pTaxControlDevManager->taxControlDevInfo[port].serialID,10))
                                                    break;
                                            }
                                    }
                                
                                if(k<TAX_CONTROL_MACHINE_GUNNO_MAX) // Found matched record, then we can bind current dev control instance to the current port.
                                    {
                                        // Fresh device, need initialize the previous "total" data to an predefinced invalid value
                                        for(n=0;n<pTaxControlDevManager->taxControlDevInfo[port].gunTotal;n++)
                                            {
                                                pTaxControlDevManager->taxControlDevInfo[port].gunSubState[n]= TAX_CONTROL_PROCESS_SUB_STATE_INIT;
                                            }

                                        if( pTaxHistoryTotalRecordInfo[k].record.gunNo == 0 && getMeterBoardMode()!=METER_MODE)
                                        {   // Successfully restore the total data from the flash, so, we set the gun initialized.
                                                for(n=0;n<pTaxControlDevManager->taxControlDevInfo[port].gunTotal;n++)
                                                    {
                                                        if(pTaxHistoryTotalRecordInfo[k+n].valid == TAX_CONTROL_HISTORY_RECORD_VALID)
                                                            {
                                                                pTaxControlDevManager->taxControlDevInfo[port].preData[n].totalAmount = pTaxHistoryTotalRecordInfo[k+n].record.totalAmount;
                                                                pTaxControlDevManager->taxControlDevInfo[port].preData[n].totalVolume = pTaxHistoryTotalRecordInfo[k+n].record.totalVolume;
                                                                pTaxControlDevManager->taxControlDevInfo[port].preData[n].timeStamp =pTaxHistoryTotalRecordInfo[k+n].record.timeStamp;
                                                                pTaxControlDevManager->taxControlDevInfo[port].gunSubState[n]= TAX_CONTROL_PROCESS_SUB_STATE_NORMAL;
                                                                printf("taxControlDevToPortBinding[Retrieve]:Port:%d Gun:%d recovered _Total_",port,n);
                                                                printf("[Volume:%lld",pTaxControlDevManager->taxControlDevInfo[port].preData[n].totalVolume);
                                                                printf(" Amount:%lld]\r\n",pTaxControlDevManager->taxControlDevInfo[port].preData[n].totalAmount);
                                                            }
                                                    }
                                                pTaxControlDevManager->taxControlDevInfo[port].gunBaseNumber = k;
                                        }
                                        else if(getMeterBoardMode()==METER_MODE)
                                        {
                                            printf("taxControlDevToPortBinding[Retrieve]: MeterMode don't use flash _total_data!\r\n");
                                        }
                                        else
                                        {
                                            printf("taxControlDevToPortBinding[Retrieve]: But Gun No doesn't match, _total_data was not recovered,k=%d,\r\n",k);
                                        }
                                    }                                  
                                pUartPortState[port].bState= PORT_BOUNDED;
                                bindingState[port]=TAX_CONTROL_BINDING_STATE_COMPLETE;                                         
                            }
                       break; 

                       case    TAX_CONTROL_BINDING_STATE_COMPLETE:
                       break;    
                     }            
               }
          bsp_DelayMS(1500);
       }        

     // Need balance how to allocate history_total_data sector allocations
    pTaxControlDevManager->taxControlDevInfo[0].gunBaseNumber = 0;
    pTaxControlDevManager->taxControlDevInfo[1].gunBaseNumber = 8;
    
    printf("taxControlDevToPortBinding: dev0 gunbase:%d  dev1 gunbase:%d\r\n",pTaxControlDevManager->taxControlDevInfo[0].gunBaseNumber,pTaxControlDevManager->taxControlDevInfo[1].gunBaseNumber);
    printf("Binding Result:  UART0: %d   UART1:%d\r\n",pUartPortState[0].bState,pUartPortState[1].bState);


    // TODO: Review whether this ie the proper place to call.
    initControlPortSetManager();

    // TODO: fix me, to look for a proper place to reset meter board. 
    MeterResetHigh;
    bsp_DelayMS(500);
    MeterResetLow;

    return E_SUCCESS;

}
/*******************************************************************************************************************************
Function Name:
Description:
    This task should be called in main task loop to detect the uart port connecting status, return rebindRequestType.
    1. Previously connected but unbounded
    2. reconnect

Note:
    1. If binded after 1st installation, the mapping from port to tax dev can't be changed and this device can't be installed on any other gasoline machine unless
clean flash. In other word, the port and tax device are static binded and the configuration has been stored in the flash.
    2. During bootup, this design will try to retrieve the mapping configuration from flash. if success, then no need to access tax device again. otherwise, need to
access tax device to read out the initialization infomation, this case is normally the 1st installation case.

Parameter:
    NULL
Return:
    E_SUCCESS: Always return E_SUCCESS forever. 
*******************************************************************************************************************************/
errorCode   taxPortDetectAndRebindCheckTask(void)
{
    UartPortState *prevState;
    UartPortState *currentState;
    int port,dev;
    unsigned char toBindFlag[UART_TAX_PORT_NUM]={0};
    TaxControlDevManager *pDevMgr = &taxControlDevManager;

    prevState = uartGetPortState();
    currentState=uartGetPortCurrentState();
		
    for(port=0;port<UART_TAX_PORT_NUM;port++)
    {
        if(currentState[port].cState == CONNECTION_STATE_CONNECT)       // currently connected
        {
            if(prevState[port].cState != CONNECTION_STATE_CONNECT)  // previously not connected, absolutely need rebind
            {
                toBindFlag[port] = 1;
                prevState[port].cState = CONNECTION_STATE_CONNECT;
            }
            else if((prevState[port].cState == CONNECTION_STATE_CONNECT) &&(prevState[port].bState == PORT_UNBOUND))  // previously connected too, but not bounded, absolutely need rebind
            {
                toBindFlag[port] = 1;
            }
        }
        else    // currently not connected. we need mark it unbound now.
        {
            prevState[port].cState = CONNECTION_STATE_INIT;
            prevState[port].bState = PORT_UNBOUND;
            for(dev=0;dev<TAX_CONTROL_DEVICE_MAX_NUM;dev++)
            {
                if((pDevMgr->taxControlDevInfo[dev].bind == TAX_CONTROL_DEVICE_BOUND) && (pDevMgr->taxControlDevInfo[dev].port == port))
                    pDevMgr->taxControlDevInfo[dev].bind = TAX_CONTROL_DEVICE_UNBOUND;
            }
        }
    }

    if(toBindFlag[0] || toBindFlag[1])
	{
		retrieveTotalHistoryRecord();      
	}
    
    if(toBindFlag[0] && toBindFlag[1])
        {
            taxControlDevToPortBinding(TAX_CONTROL_DEV_BINDING_REQ_TOTAL);
            printf("taxPortConnectingDetectAndRebindTask: Bind both Port0 and Port1 finished\r\n");
        }
    else
        {
            if(toBindFlag[0])
                {
                    taxControlDevToPortBinding(TAX_CONTROL_DEV_BINDING_REQ_PORT0);
                    printf("taxPortConnectingDetectAndRebindTask: Bind Port0 finished\r\n");                    
                }
            else if(toBindFlag[1])
                {
                    taxControlDevToPortBinding(TAX_CONTROL_DEV_BINDING_REQ_PORT1);
                    printf("taxPortConnectingDetectAndRebindTask: Bind Port1 finished\r\n");                    
                }
        }

    
    return E_SUCCESS;
}

/*******************************************************************************************************************************
Function Name: taxControlDevSamplingOnInit
Description:
      If local RTC clock has synced w/ GTC, then change the current device to "IDLE" state which can start polling transaction info. if clock not synced, then 
still stay in "INIT" state.

Parameter:
	pDev:[I/O], ponits to device infomation structure.
	clkState:[I], local clock sync state
Return:
	E_SUCCESS: Always successfully
*******************************************************************************************************************************/
static errorCode taxControlDevSamplingOnInit(TaxControlDevInfo *pDev, ClockSyncState clkState)
{
    unsigned char buf[128];
	
		
    if(clkState == CLOCK_GTC)
    {
        unsigned short availBytes;
        availBytes = uartGetAvailBufferedDataNum(pDev->port);
        if(availBytes > 0)
        {
            if(availBytes > 128)
                uartRead(pDev->port,buf, 128);
            else
                uartRead(pDev->port,buf, availBytes);
        }
        if(getMeterBoardMode()!= FREE_MODE)
            pDev->sampleState =TAX_CONTROL_SAMPLING_STATE_FREE;
        else
            pDev->sampleState = TAX_CONTROL_SAMPLING_STATE_IDLE;
    }
  
    return E_SUCCESS;
}
/*******************************************************************************************************************************
Function Name: taxControlSamplingOnIdle
Description:
      Start the new loop polling. Request  "TAX_CONTROL_CMD_QUERY_TOTAL" Response, clear all the counts and set the state to the next one "TAX_CONTROL_
CMD_TRACE_STATE_QUERY1".
Parameter:
	pDev:[I], points to device infomation structure
Return:
	E_SUCCESS: when complete successfully
*******************************************************************************************************************************/
static errorCode taxControlDevSamplingOnIdle(TaxControlDevInfo *pDev)
{
    errorCode error=E_SUCCESS;
    unsigned char pCmdBuf[64];
    unsigned short size;
    
    switch(pDev->curCmd)
        {
            case TAX_CONTROL_CMD_QUERY_TOTAL:
                {
                    size = sizeof(TaxTotalQueryCmd);    // Send commnad to TaxControlDevice to query current gun's "total" information.
                    error=taxControlCmdCreate(pDev->curCmd,pDev->gunCurrent,pCmdBuf,&size,TAX_RESPONSE_RETURN_PLAINTEXT);
                    error=uartWriteBuffer(pDev->port, pCmdBuf,size);
                    error=uartSendBufferOut(pDev->port);
                    pDev->statistic[pDev->gunCurrent].totalTryCount++;
                }
            break;   
        }
    return error;				
}


#if Concentrator_OS == 0
void TaxTransactionDataRecord(TaxOnceTransactionRecord *record,TaxControlDevInfo *pDev,TaxTotalQueryResp *pTotalResp,u64 diffV,u64 diffA,u32 price)
{
    LocalTimeStamp ltm;
    unsigned char tt[7];
    
    
    memset(record,0xff,sizeof(TaxOnceTransactionRecord));
    record->head.mark= DIRTY;                          // make it dirty after write to flash, read function will make it done
    
    record->head.data_type=TaxData;//Ë°¿ØÊý¾Ý
    
    
    record->content.gunNo = (pDev->gunCurrent << 8) + pDev->port;    // This gun #   

    record->content.timeStamp= pDev->dataStore[pDev->gunCurrent].timeStamp;
    memcpy(record->content.factorySerialNo,pDev->serialID,10);   // FactoryID

    getLocalFormatedTime(record->content.timeStamp,&ltm);
    memcpy(record->content.date,ltm.date,2);    // Date
    memcpy(record->content.hour,ltm.hour,2);    // Hour
    memcpy(record->content.minute,ltm.minute,2);    // Minute
     
    sprintf(tt,"%06d",diffV);                   // This volume
    memcpy(record->content.volume,tt,6);
     
     //itoa(aa,(int)diffA,10);
    sprintf(tt,"%06d",diffA);                   // This amount
    memcpy(record->content.amount,tt,6);

    sprintf(tt,"%04d",price);
    memcpy(record->content.price,tt,4);

    memcpy(record->content.Totalvolume,pTotalResp->volume,12);//±¾´Î×ÜÀà¼ÓÓÍÁ¿´æÈë
    memcpy(record->content.Totalamount,pTotalResp->amount,12);//±¾´Î×ÜÀà½ð¶î´æ
    
}
#else

void TaxTransactionDataRecord(TaxOnceTransactionRecord *record,TaxControlDevInfo *pDev,TaxTotalQueryResp *pTotalResp,u64 diffV,u64 diffA,u32 price)
{
   
    memset(record,0xff,sizeof(TaxOnceTransactionRecord));
    record->head.mark= DIRTY;                          // make it dirty after write to flash, read function will make it done
    
    record->head.data_type=TaxData;//Ë°¿ØÊý¾Ý
    
    record->content.volume=diffV;
    record->content.amount=diffA;
    record->content.price=price;
    
    record->content.Totalvolume=pDev->preData[pDev->gunCurrent].totalVolume;
    record->content.Totalamount=pDev->preData[pDev->gunCurrent].totalAmount;;
    record->content.timeStamp= pDev->preData[pDev->gunCurrent].timeStamp;
    record->content.gunNo = pDev->gunCurrent;    // This gun #   
    record->content.port = pDev->port; 
    memcpy(record->content.factorySerialNo,pDev->serialID,10);   // FactoryID
}

#endif
/*******************************************************************************************************************************
Function Name: taxControlDevSamplingOnProcessing
Description:
      Process the "total" transaction information, store to the flash, else change to BUSY state if timeout or turn to CONFIRM state once need query price. 
Parameter:
	pDev:[I], points to device infomation structure
Return:
	E_SUCCESS: when complete successfully but without transaction.
	E_SUCCESS_TRANSACTION: when complete and with transaction
	E_AMBIGUITY: Can't calculate gasoline price, need further processing.
	E_CHECK_CODE: Check code error.
	E_MESSAGE_TYPE: Got unexpected data message
	E_WAITING: Still need wait for device's response.
	E_DEVICE_BUSY: Device is busy, can't repsonce within TIMEOUT
	E_GUN_INITIALIZING:  Device's current gun's data is not initialized yet 
	E_FAIL:  Total wrong, need restart sampling.
	E_WAIT_COFIRM: Need query again to double confirm
*******************************************************************************************************************************/
static errorCode taxControlDevSamplingOnProcessing(TaxControlDevInfo *pDev)
{
    errorCode error=E_SUCCESS;
    TaxTotalQueryResp *pTotalResp;
    unsigned char pCmdBuf[64];
    unsigned char pRespBuf[128];
    unsigned char availBytes=0;
    unsigned short size=0;
    unsigned char cc=0x00;
    unsigned char i;
    int price;
    TaxOnceTransactionRecord record;

    if(pDev == 0)
        return E_INPUT_PARA;
    
    availBytes = uartGetAvailBufferedDataNum(pDev->port);
    
    if(availBytes >= sizeof(TaxTotalQueryResp))	// Got expected # of bytes, need parse them
	{
		taxControlResponseRetriever(pDev->port,pRespBuf,availBytes);
		pTotalResp = (TaxTotalQueryResp *)pRespBuf;
		if(pTotalResp->header.cmd == pDev->curCmd)
		{
            for(i=2;i<sizeof(TaxTotalQueryResp)-1;i++)
                cc = cc ^ pRespBuf[i];
            
            if(cc == pTotalResp->cc)
                {
                    unsigned char t[13];
                    long long diffV=0;
                    long long diffA=0;
                    long long minorVol;
                    long long thisV,thisA;
                    unsigned currentTick= getLocalTick();

                    // Covert "volume" and "amount" from "ASCII" to "long long" to get thisV and thisA for judgement
                    t[12]=0;
                    memcpy(t,pTotalResp->volume,12);
                    thisV =atoll(t);

                    t[12]=0;
                    memcpy(t,pTotalResp->amount,12);
                    thisA=atoll(t);


                    switch (pDev->gunSubState[pDev->gunCurrent])
                     {
                                        // This case make sure that each gun's 1st _total_data_confirmed 3 times to have confidence that it is correct.
                        case  TAX_CONTROL_PROCESS_SUB_STATE_INIT:
                        {
                             if(pDev->gunRepeatCount == 0xff)
                             {
                                pDev->preData[pDev->gunCurrent].totalVolume  = thisV;
                                pDev->preData[pDev->gunCurrent].totalAmount = thisA;
                                pDev->preData[pDev->gunCurrent].timeStamp = currentTick;
                                pDev->gunRepeatCount++;
                                printf("taxControlDevSamplingOnProcessing: Port:%d Gun:%d Initializing Start: %d\r\n",pDev->port,pDev->gunCurrent,pDev->gunRepeatCount);
                                
                                return E_GUN_INITIALIZE;
                              }
                              else
                              {
                                 if(pDev->gunRepeatCount < (TAX_GUN_DATA_INITIALIZE_TIME -1))
                                 {
                                    if(thisV ==pDev->preData[pDev->gunCurrent].totalVolume)
                                    {
                                        pDev->gunRepeatCount++;
                                        return  E_GUN_INITIALIZE;
                                    }
                                    else
                                    {
                                        pDev->preData[pDev->gunCurrent].totalVolume = thisV;
                                        pDev->preData[pDev->gunCurrent].totalAmount = thisA;
                                        pDev->preData[pDev->gunCurrent].timeStamp = currentTick;
                                        pDev->gunRepeatCount = 0;
                                        printf("taxControlDevSamplingOnProcessing: Port:%d Gun:%d Initializing Restart: %d\r\n",pDev->port,pDev->gunCurrent,pDev->gunRepeatCount);
                                        
                                        return E_GUN_INITIALIZE;
                                    }
                                 }
                                 else
                                 {
                                        pDev->gunRepeatCount = 0xff;
                                        pDev->gunSubState[pDev->gunCurrent] = TAX_CONTROL_PROCESS_SUB_STATE_NORMAL;

                                        storeTotalHistoryRecord(pDev->port, pDev->gunCurrent, pDev->gunBaseNumber, thisV, thisA, pDev->serialID, pDev->taxUniqueID);                                                                            
                                        
                                        if(getMeterBoardMode()==METER_MODE)
                                        {
                                            TaxTransactionDataRecord(&record, pDev, pTotalResp, 0,  0,  0);
                                            storeTransactionRecord(&record,TaxData);
                                        }
                                     
                                        printf("taxControlDevSamplingOnProcessing: [INIT] Port:%d Gun:%d Intializing Complete,Total Data[",pDev->port,pDev->gunCurrent);
                                        printf("Volume:%lld  ",thisV);
                                        printf("Amount:%lld ]\r\n",thisA);
                                        
                                        return E_SUCCESS;
                                 }                                                                
                               }                                                                    
                            }
                            break;

                        case    TAX_CONTROL_PROCESS_SUB_STATE_NORMAL:
                            {
                                if(pDev->gunTotal == 1)     // One_Gun's tax control device doesn't have gun ambiguity issue, directly store the current transaction
                                    {
                                        diffV = thisV-pDev->preData[pDev->gunCurrent].totalVolume;
                                        diffA = thisA-pDev->preData[pDev->gunCurrent].totalAmount;

                                        // Same as previous, drop it.
                                        if(diffV == 0LL)
                                            {
                                                return E_SUCCESS;
                                            }
                                        else if(diffV < 0LL)    // Smaller than previous one, something crash, reset system 
                                            {                                                                            
                                                 printf("!!!!taxControlDevSamplingOnProcessing: Port:%d Gun:%d Single gun, this _total_data_ < previous _total_data_\r\n",pDev->port,pDev->gunCurrent);
                                                 return E_FAIL;
                                             }
                                        else
                                            {
                                                pDev->dataStore[pDev->gunCurrent].totalVolume = thisV;
                                                pDev->dataStore[pDev->gunCurrent].totalAmount = thisA;
                                                pDev->dataStore[pDev->gunCurrent].timeStamp = currentTick;
                                                goto final_decision;
                                            }                                                                        
                                    }
                                   else if(pDev->gunTotal == 2)     // Two_Guns tax control deivce has ambiguity issues, deploy simple decision
                                    {
                                        pDev->dataStore[pDev->gunCurrent].totalVolume = thisV;
                                        pDev->dataStore[pDev->gunCurrent].totalAmount = thisA;
                                        pDev->dataStore[pDev->gunCurrent].timeStamp = currentTick;

                                        diffV = thisV-pDev->preData[pDev->gunCurrent].totalVolume;
                                        diffA = thisA-pDev->preData[pDev->gunCurrent].totalAmount;

                                        minorVol = MIN(pDev->preData[0].totalVolume,pDev->preData[1].totalVolume);
                                        
                                        // Sn < MINOR(S0,S1)
                                        if(thisV < minorVol)
                                            {
                                                printf("!!!! taxControlDevSamplingOnProcessing: Port:%d Gun:%d The _total_data_ is smaller than both previous data, Drop!\r\n",pDev->port,pDev->gunCurrent);
                                                return E_FAIL;
                                            }

                                        // Good, Sn= Spre_this
                                        if(thisV == pDev->preData[pDev->gunCurrent].totalVolume)
                                            {
                                                return E_SUCCESS;
                                            }

                                        // a little ambiguious, need confirm
                                        if(thisV == pDev->preData[OPPPSITE(pDev->gunCurrent)].totalVolume)
                                            {
                                                printf("!!!! taxControlDevSamplingOnProcessing: Port:%d Gun:%d The _total_data_ is same the opposite, Need confirm\r\n",pDev->port,pDev->gunCurrent);
                                                pDev->gunSubState[pDev->gunCurrent] = TAX_CONTROL_PROCESS_SUB_STATE_PEND;
                                                return E_WAIT_CONFIRM;
                                            }
                                        
                                       
                                           //  S(this)< Sn < S(opposite)    Good
                                        if(thisV > pDev->preData[pDev->gunCurrent].totalVolume && thisV < pDev->preData[OPPPSITE(pDev->gunCurrent)].totalVolume)
                                            {
                                                goto final_decision;
                                            }

                                            // S(opposite) < Sn < S(this)  
                                        if(thisV > pDev->preData[OPPPSITE(pDev->gunCurrent)].totalVolume && thisV < pDev->preData[pDev->gunCurrent].totalVolume)
                                            {
                                                printf("!!!! taxControlDevSamplingOnProcessing: Port:%d Gun:%d The _total_data_  < this  >opposite,Need confirm\r\n",pDev->port,pDev->gunCurrent);
                                                return E_FAIL;
                                            }

                                            // A little bad, need check speed.
                                        if(thisV > MAX(pDev->preData[0].totalVolume,pDev->preData[1].totalVolume))
                                            {
                                                long long maxVol[2];
                                                unsigned char targetGun;
                                                // Maxium speed: Here set 90L/minute 
                                                maxVol[0] = (getLocalTick() - pDev->preData[0].timeStamp) * TAX_CONTROL_GUN_MAX_SPEED;
                                                maxVol[1] = (getLocalTick() - pDev->preData[1].timeStamp) * TAX_CONTROL_GUN_MAX_SPEED;            

                                                if(diffV >= MAX(maxVol[0],maxVol[1]))
                                                    {
                                                        printf("!!!!taxControlDevSamplingOnProcessing: Port:%d Gun:%d @%x Speed is bigger than limit on both gun\r\n",pDev->port,pDev->gunCurrent,getSysTick());                                                                    
                                                        return E_FAIL;
                                                    }
                                                else if(diffV > maxVol[0] && diffV <= maxVol[1])
                                                    {
                                                        if(pDev->gunCurrent == 1)
                                                            {
                                                                goto final_decision;
                                                            }
                                                        else
                                                            {
                                                                printf("!!!!taxControlDevSamplingOnProcessing: Port:%d Gun:%x @%d Speed judge NG\r\n",pDev->port,pDev->gunCurrent,getSysTick());                                                                            
                                                                return E_FAIL;
                                                            }
                                                    }
                                                else if(diffV > maxVol[1] && diffV <= maxVol[0])
                                                    {
                                                        if(pDev->gunCurrent == 0)
                                                            {
                                                                goto final_decision;
                                                            }
                                                        else
                                                            {
                                                                printf("!!!!taxControlDevSamplingOnProcessing: Port:%d Gun:%d @%x Speed judge NG\r\n",pDev->port,pDev->gunCurrent,getSysTick());                                                                            
                                                                return E_FAIL;
                                                             }
                                                    }
                                                else
                                                    {
                                                        printf("!!!!taxControlDevSamplingOnProcessing: Port:%d Gun:%d @%x Can't judge by Simple judgemeb, Query again\r\n",pDev->port,pDev->gunCurrent,getSysTick());
                                                        // Send query command again and change the substate to "pending"
                                                        error=taxControlCmdCreate(TAX_CONTROL_CMD_QUERY_TOTAL,pDev->gunCurrent,pCmdBuf,&size,TAX_RESPONSE_RETURN_PLAINTEXT);
                                                        error=uartWriteBuffer(pDev->port, pCmdBuf,size);
                                                        error=uartSendBufferOut(pDev->port);

                                                        pDev->gunSubState[pDev->gunCurrent] = TAX_CONTROL_PROCESS_SUB_STATE_PEND;
                                                        return E_WAIT_CONFIRM;
                                                    }
                                
                                             }

                                     }
                                    else    // For 3 and 3 more gun's tax control, we need doube confirm the _total_data by reading it next time
                                        {
                                            pDev->dataStore[pDev->gunCurrent].totalVolume = thisV;
                                            pDev->dataStore[pDev->gunCurrent].totalAmount = thisA;
                                            pDev->dataStore[pDev->gunCurrent].timeStamp = currentTick;                                                                        

                                            // Send query command again and change the substate to "pending"
                                            error=taxControlCmdCreate(TAX_CONTROL_CMD_QUERY_TOTAL,pDev->gunCurrent,pCmdBuf,&size,TAX_RESPONSE_RETURN_PLAINTEXT);
                                            error=uartWriteBuffer(pDev->port, pCmdBuf,size);
                                            error=uartSendBufferOut(pDev->port);
                                            
                                            pDev->gunSubState[pDev->gunCurrent] = TAX_CONTROL_PROCESS_SUB_STATE_PEND;
                                            printf("taxControlDevSamplingOnProcessing:  Port:%d Gun:%d Got 1st _total_data_, Need confirm\r\n",pDev->port,pDev->gunCurrent);
                                            return E_WAIT_CONFIRM;
                                        }
        final_decision:                                                
                                                                        
                                {
                                    LocalTimeStamp ltm;
                                    unsigned char tt[7];

                                    pDev->gunSubState[pDev->gunCurrent] = TAX_CONTROL_PROCESS_SUB_STATE_NORMAL;

                                    // We got a new "total", we need store them into flash whatever need query price or not
//                                    storeTotalHistoryRecord(pDev->port, pDev->gunCurrent, pDev->gunBaseNumber,
//                                                                             pDev->dataStore[pDev->gunCurrent].totalVolume,pDev->dataStore[pDev->gunCurrent].totalAmount,pDev->serialID, pDev->taxUniqueID);
                                    
                                    printf("$$[NORMAL] Port:%d  Gun:%d Store New Total data into flash\r\n",pDev->port,pDev->gunCurrent);
                                    printf("$$[NORMAL] New Total Volume:%lld\r\n",pDev->dataStore[pDev->gunCurrent].totalVolume);
                                    printf("$$[NORMAL] New Total Amount:%lld\r\n",pDev->dataStore[pDev->gunCurrent].totalAmount);
                                    printf("$$[NORMAL] Pre Total Volume:%lld\r\n",pDev->preData[pDev->gunCurrent].totalVolume);
                                    printf("$$[NORMAL] Pre Total Amount:%lld\r\n",pDev->preData[pDev->gunCurrent].totalAmount);
                                    printf("$$[NORMAL] This Volume:%lld\r\n",diffV);
                                    printf("$$[NORMAL] This Amount:%lld\r\n",diffA);
                                    printf("$$[NORMAL] TimeStamp:%d\r\n",pDev->dataStore[pDev->gunCurrent].timeStamp);
                                    
                                    price=taxControlPriceJudegmenet((float)diffV,(float)diffA);

                                    if(price > 0)
                                    {
                                        
                                        storeTotalHistoryRecord(pDev->port, pDev->gunCurrent, pDev->gunBaseNumber,
                                                                pDev->dataStore[pDev->gunCurrent].totalVolume,pDev->dataStore[pDev->gunCurrent].totalAmount,pDev->serialID, pDev->taxUniqueID);
                                    
                                        
                                        pDev->preData[pDev->gunCurrent].totalVolume  = pDev->dataStore[pDev->gunCurrent].totalVolume;
                                        pDev->preData[pDev->gunCurrent].totalAmount  =pDev->dataStore[pDev->gunCurrent].totalAmount;
                                        pDev->preData[pDev->gunCurrent].timeStamp = pDev->dataStore[pDev->gunCurrent].timeStamp;

                                        printf("$$taxControlDevSamplingOnProcessing: Fine! Port:%d Gun:%d got Volume:%lld,Final.\r\n",pDev->port,pDev->gunCurrent,diffV);
/*
                                        memset(&record,0xff,sizeof(TaxOnceTransactionRecord));
                                        record.head.mark= DIRTY;                          // make it dirty after write to flash, read function will make it done
                                        record.content.gunNo = (pDev->gunCurrent << 8) + pDev->port;    // This gun #   

                                        record.content.timeStamp= pDev->dataStore[pDev->gunCurrent].timeStamp;
                                        memcpy(record.content.factorySerialNo,pDev->serialID,10);   // FactoryID

                                        getLocalFormatedTime(record.content.timeStamp,&ltm);
                                        memcpy(record.content.date,ltm.date,2);    // Date
                                        memcpy(record.content.hour,ltm.hour,2);    // Hour
                                        memcpy(record.content.minute,ltm.minute,2);    // Minute
                                         
                                        sprintf(tt,"%06d",diffV);                   // This volume
                                        memcpy(record.content.volume,tt,6);
                                         
                                         //itoa(aa,(int)diffA,10);
                                        sprintf(tt,"%06d",diffA);                   // This amount
                                        memcpy(record.content.amount,tt,6);

                                        sprintf(tt,"%04d",price);
                                        memcpy(record.content.price,tt,4);
                                        
                                        memcpy(record.content.Totalvolume,pTotalResp->volume,12);//±¾´Î×ÜÀà¼ÓÓÍÁ¿´æÈë
                                        memcpy(record.content.Totalamount,pTotalResp->amount,12);//±¾´Î×ÜÀà½ð¶î´æ
 */
 
                                        TaxTransactionDataRecord(&record, pDev, pTotalResp, diffV,  diffA,  price);

                                        storeTransactionRecord(&record,TaxData);
                                        
                                        pDev->statistic[pDev->gunCurrent].successCount++;
                                        
//                                    	u8 ret;
//                                        ret=MeterDataCmpTaxData(pDev->port,pDev->gunCurrent,diffA,diffV,price,&record);
//                                        
//                                        if(ret<=1)								
//                                        {								
//                                            pDev->statistic[pDev->gunCurrent].successCount++;
//                                        }
//                                        else
//                                        {
//                                            pDev->statistic[pDev->gunCurrent].successCount+=ret;
//                             
//                                        }
                                                                                                                     
                                        return E_SUCCESS_TRANSACTION;
                                        
                                    }
                                    else    // Need query price to double confirm
                                    {
                                        printf("$$[NORMAL] Still Need Query Price\r\n");
                                        pDev->confirmState = TAX_CONTROL_CONFIRM_SUB_STATE_INIT;
                                            
                                        error=taxControlCmdCreate(TAX_CONTROL_CMD_QUERY_ONCE,pDev->gunCurrent,pCmdBuf,&size,TAX_RESPONSE_RETURN_PLAINTEXT);
                                        error=uartWriteBuffer(pDev->port, pCmdBuf,size);
                                        error=uartSendBufferOut(pDev->port);

                                        return E_AMBIGUITY;
                                    }
                                }

                        }
                    break;

                    case    TAX_CONTROL_PROCESS_SUB_STATE_PEND:
                        {                                                              
                            if(thisV == pDev->dataStore[pDev->gunCurrent].totalVolume)  // Cofirmed, we got continious 2 same _total_data
                                {
                                    LocalTimeStamp ltm;
                                    unsigned char tt[7];

                                    diffV = thisV - pDev->preData[pDev->gunCurrent].totalVolume;
                                    diffA = thisA - pDev->preData[pDev->gunCurrent].totalAmount;
                                                                                                                  
                                    pDev->gunSubState[pDev->gunCurrent] = TAX_CONTROL_PROCESS_SUB_STATE_NORMAL;
                                    
                                    if(diffV==0)
                                       return E_SUCCESS;    

                                    // We got a new "total", we need store them into flash whatever need query price or not
//                                    storeTotalHistoryRecord(pDev->port, pDev->gunCurrent, pDev->gunBaseNumber,
//                                                                             pDev->dataStore[pDev->gunCurrent].totalVolume,pDev->dataStore[pDev->gunCurrent].totalAmount,pDev->serialID, pDev->taxUniqueID);
//                                    
                                    printf("$$[PEND] Port:%d  Gun:%d Store New Total data into flash\r\n",pDev->port,pDev->gunCurrent);
                                    printf("$$[PEND] New Total Volume:%lld\r\n",pDev->dataStore[pDev->gunCurrent].totalVolume);
                                    printf("$$[PEND] New Total Amount:%lld\r\n",pDev->dataStore[pDev->gunCurrent].totalAmount);
                                    printf("$$[PEND] Pre Total Volume:%lld\r\n",pDev->preData[pDev->gunCurrent].totalVolume);
                                    printf("$$[PEND] Pre Total Amount:%lld\r\n",pDev->preData[pDev->gunCurrent].totalAmount);
                                    printf("$$[PEND] This Volume:%lld\r\n",diffV);
                                    printf("$$[PEND] This Amount:%lld\r\n",diffA);
                                    printf("$$[PEND] TimeStamp:%d\r\n",pDev->dataStore[pDev->gunCurrent].timeStamp);
                                    
                                    price=taxControlPriceJudegmenet((float)diffV,(float)diffA);

                                    if(price > 0)
                                     {
                                            storeTotalHistoryRecord(pDev->port, pDev->gunCurrent, pDev->gunBaseNumber,
                                                                     pDev->dataStore[pDev->gunCurrent].totalVolume,pDev->dataStore[pDev->gunCurrent].totalAmount,pDev->serialID, pDev->taxUniqueID);
                                   
                                            
                                            pDev->preData[pDev->gunCurrent].totalVolume  = pDev->dataStore[pDev->gunCurrent].totalVolume;
                                            pDev->preData[pDev->gunCurrent].totalAmount  =pDev->dataStore[pDev->gunCurrent].totalAmount;
                                            pDev->preData[pDev->gunCurrent].timeStamp = pDev->dataStore[pDev->gunCurrent].timeStamp;

/*
                                            memset(&record,0xff,sizeof(TaxOnceTransactionRecord));
                                            record.head.mark= DIRTY;                          // make it dirty after write to flash, read function will make it done
                                            record.content.gunNo = (pDev->gunCurrent << 8) + pDev->port;    // This gun #   

                                            record.content.timeStamp= pDev->dataStore[pDev->gunCurrent].timeStamp;
                                            memcpy(record.content.factorySerialNo,pDev->serialID,10);   // FactoryID

                                            getLocalFormatedTime(record.content.timeStamp,&ltm);
                                            memcpy(record.content.date,ltm.date,2);    // Date
                                            memcpy(record.content.hour,ltm.hour,2);    // Hour
                                            memcpy(record.content.minute,ltm.minute,2);    // Minute
                                             
                                            sprintf(tt,"%06d",diffV);                   // This volume
                                            memcpy(record.content.volume,tt,6);
                                             
                                             //itoa(aa,(int)diffA,10);
                                            sprintf(tt,"%06d",diffA);                   // This amount
                                            memcpy(record.content.amount,tt,6);

                                            sprintf(tt,"%04d",price);
                                            memcpy(record.content.price,tt,4);
                                            
                                            memcpy(record.content.Totalvolume,pTotalResp->volume,12);//±¾´Î×ÜÀà¼ÓÓÍÁ¿´æÈë
                                            memcpy(record.content.Totalamount,pTotalResp->amount,12);//±¾´Î×ÜÀà½ð¶î´æÈ
*/
                                            
                                            TaxTransactionDataRecord(&record, pDev, pTotalResp, diffV,  diffA,  price);
                                            
                                            storeTransactionRecord(&record,TaxData);
                                            
                                            pDev->statistic[pDev->gunCurrent].successCount++;
//                                            u8 ret;
//                                            ret=MeterDataCmpTaxData(pDev->port,pDev->gunCurrent,diffA,diffV,price,&record);
//                                            
//                                            if(ret<=1)								
//                                            {								
//                                                pDev->statistic[pDev->gunCurrent].successCount++;
//                                            }
//                                            else
//                                            {
//                                                pDev->statistic[pDev->gunCurrent].successCount+=ret;
//                                 
//                                            }
                                            
                                        }
                                    else    // Need query price to double confirm
                                        {
                                            printf("$$taxControlDevSamplingOnProcessing: [PEND] Still Need Query Price\r\n");
                                            pDev->confirmState = TAX_CONTROL_CONFIRM_SUB_STATE_INIT;
                                                
                                            error=taxControlCmdCreate(TAX_CONTROL_CMD_QUERY_ONCE,pDev->gunCurrent,pCmdBuf,&size,TAX_RESPONSE_RETURN_PLAINTEXT);
                                            error=uartWriteBuffer(pDev->port, pCmdBuf,size);
                                            error=uartSendBufferOut(pDev->port);

                                            return E_AMBIGUITY;
                                        }

                                   return  E_SUCCESS_TRANSACTION;
                                }
                            else        // No, we got 2 continious different _total_data, need reconfirm again
                                {
                                    pDev->dataStore[pDev->gunCurrent].totalVolume = thisV;
                                    pDev->dataStore[pDev->gunCurrent].totalAmount = thisA;
                                    pDev->dataStore[pDev->gunCurrent].timeStamp = currentTick;
                                    error=taxControlCmdCreate(TAX_CONTROL_CMD_QUERY_TOTAL,pDev->gunCurrent,pCmdBuf,&size,TAX_RESPONSE_RETURN_PLAINTEXT);
                                    error=uartWriteBuffer(pDev->port, pCmdBuf,size);
                                    error=uartSendBufferOut(pDev->port);
                                    return E_WAIT_CONFIRM;
                                }                                                    
                            
                        }
                        break;

                        default:
                            {
                                while(1)
                                {
                                    printf("taxControlDevSamplingOnProcessing: Port:%d Gun:%d  SubState is totaly wrong\r\n");
                                }   
                            }
                            break;
                       }                                                    
                }
            else		//checksum error, restart the loop. 
                {
                    return E_CHECK_CODE;			

                }
	   }
       else
           {		
                return E_MESSAGE_TYPE;			
           }
    }
    else
    {
        // Incrment the cmdWaitCount
        if(pDev->stateCount<= TAX_CONTROL_CMD_WAIT_TIMEOUT)	       // Not reach timeout, need wait more.
            {
                return E_DEVICE_BUSY;
            }
        else		// Wait 3 seconds, doesn't get any response, seems taxControlDevice is busy, change state to Idle,  restart polling loop.
            {
                if(availBytes > 0)
                    uartEmptyRcvBuffer(pDev->port);
                
                   error=taxControlCmdCreate(TAX_CONTROL_CMD_QUERY_TOTAL,pDev->gunCurrent,pCmdBuf,&size,TAX_RESPONSE_RETURN_PLAINTEXT);
                   error=uartWriteBuffer(pDev->port, pCmdBuf,size);
                   error=uartSendBufferOut(pDev->port);

                return E_DEVICE_BUSY;
            }
	}
	return E_SUCCESS;
}

/*******************************************************************************************************************************
Function Name: taxControlDevSamplingOnConfirm
Description:
      This function processes "price" response message. 
Parameter:
	pDev:[I], points to device infomation structure
Return:
	E_SUCCESS: when complete successfully without transaction.
	E_SUCCESS_TRANSACTION: when complete successfully with transaction 
	E_CHECK_CODE: Check code error.
	E_MESSAGE_TYPE: Got unexpected data message
	E_WAITING: Still need wait for device's response.
	E_DEVICE_BUSY: Device is busy, can't repsonce within TIMEOUT	
*******************************************************************************************************************************/
static errorCode taxControlDevSamplingOnConfirm(TaxControlDevInfo *pDev)
{
    TaxOnceQueryResp *ptaxOnceResp;
    errorCode error=E_SUCCESS;
    unsigned short size=0;
    unsigned char pCmdBuf[64];
    unsigned char pRespBuf[128];
    unsigned char availBytes=0;
    unsigned char cc=0x00;
    unsigned char i,checkcode;
    
    TaxOnceTransactionRecord record;
   
    availBytes = uartGetAvailBufferedDataNum(pDev->port);
    if(availBytes >= sizeof(TaxOnceQueryResp))	// Got expected # of bytes, need parse them.
    	{
            taxControlResponseRetriever(pDev->port,pRespBuf,availBytes);
           
            ptaxOnceResp = (TaxOnceQueryResp *)pRespBuf;
            
            checkcode=*(pRespBuf+ptaxOnceResp->header.length+1);
                  
                   
            for(i=0;i<ptaxOnceResp->header.length-1;i++)
                  cc = cc ^ pRespBuf[i+2];

            ptaxOnceResp->cc=checkcode;
            if(ptaxOnceResp->header.cmd == pDev->curCmd)    // && (cc == ptaxOnceResp->cc))
    		{
    		    if(cc == ptaxOnceResp->cc)
                    {            
                        long long diffV=0;
                        long long diffA=0;
                        LocalTimeStamp ltm;
                        unsigned char tt[7]={0};
                        unsigned char ss[13]={0};
                        switch (pDev->confirmState)
                            {
                                case TAX_CONTROL_CONFIRM_SUB_STATE_INIT:
                                    {
                                        memcpy(pDev->prePrice,ptaxOnceResp->price,4);
                                        pDev->confirmState = TAX_CONTROL_CONFIRM_SUB_STATE_CONFIRM;
                                        size = sizeof(TaxOnceQueryCmd);
                                        error=taxControlCmdCreate(TAX_CONTROL_CMD_QUERY_ONCE,pDev->gunCurrent,pCmdBuf,&size,TAX_RESPONSE_RETURN_PLAINTEXT);
                                        error=uartWriteBuffer(pDev->port, pCmdBuf,size);
                                        error=uartSendBufferOut(pDev->port);
                                        return E_WAIT_CONFIRM;
                                    }
                                break;
                                case TAX_CONTROL_CONFIRM_SUB_STATE_CONFIRM:
                                    {
                                        if(memcmp(pDev->prePrice,ptaxOnceResp->price,4))   // Not same as previous queried price, need confirm again
                                            {
                                                memcpy(pDev->prePrice,ptaxOnceResp->price,4);
                                                size = sizeof(TaxOnceQueryCmd);
                                                 error=taxControlCmdCreate(TAX_CONTROL_CMD_QUERY_ONCE,pDev->gunCurrent,pCmdBuf,&size,TAX_RESPONSE_RETURN_PLAINTEXT);
                                                 error=uartWriteBuffer(pDev->port, pCmdBuf,size);
                                                 error=uartSendBufferOut(pDev->port);
                                                 return E_WAIT_CONFIRM;                                                       
                                            }
                                        else   // We got the same 2 continuous price, we have confidence that the price is correct, save the record to flash for uploading
                                            {                                                    
                                                // Whatever recover the substate to the initial state
                                                pDev->confirmState = TAX_CONTROL_CONFIRM_SUB_STATE_INIT;             

                                                diffV = pDev->dataStore[pDev->gunCurrent].totalVolume - pDev->preData[pDev->gunCurrent].totalVolume;
                                                diffA = pDev->dataStore[pDev->gunCurrent].totalAmount-pDev->preData[pDev->gunCurrent].totalAmount;
                                                
                                                printf("$$[Confirm] Successfully sampled a record: Port:%d Gun:%d\r\n",pDev->port,pDev->gunCurrent);
                                                printf("$$[Confirm] Pre Total Volume:%lld\r\n",pDev->preData[pDev->gunCurrent].totalVolume);
                                                printf("$$[Confirm] Pre Total Amount:%lld\r\n",pDev->preData[pDev->gunCurrent].totalAmount);                                                       
                                                printf("$$[Confirm] New Total Volume:%lld\r\n",pDev->dataStore[pDev->gunCurrent].totalVolume);
                                                printf("$$[Confirm] New Total Amount:%lld\r\n",pDev->dataStore[pDev->gunCurrent].totalAmount);
                                                printf("$$[Confirm] This Volume:%lld\r\n",diffV);
                                                printf("$$[Confirm] This Amount:%lld\r\n",diffA);
                                                printf("$$[Confirm] TimeStamp:%d\r\n",pDev->dataStore[pDev->gunCurrent].timeStamp);

                                                pDev->preData[pDev->gunCurrent].totalVolume  = pDev->dataStore[pDev->gunCurrent].totalVolume;
                                                pDev->preData[pDev->gunCurrent].totalAmount  =pDev->dataStore[pDev->gunCurrent].totalAmount;
                                                            pDev->preData[pDev->gunCurrent].timeStamp = pDev->dataStore[pDev->gunCurrent].timeStamp;
                                                
                                                
                                                storeTotalHistoryRecord(pDev->port, pDev->gunCurrent, pDev->gunBaseNumber,
                                                                     pDev->dataStore[pDev->gunCurrent].totalVolume,pDev->dataStore[pDev->gunCurrent].totalAmount,pDev->serialID, pDev->taxUniqueID);
                                   
                                            
                                            
 /*                                               
                                                memset(&record,0xff,sizeof(TaxOnceTransactionRecord));
                                                record.head.mark= DIRTY;                          // make it dirty after write to flash, read function will make it done
                                                record.content.gunNo =  (pDev->gunCurrent << 8) + pDev->port;    // This gun #
                                                record.content.timeStamp= getLocalTick();
                                                memcpy(record.content.factorySerialNo,pDev->serialID,10);   // FactoryID
                                                getLocalFormatedTime(record.content.timeStamp,&ltm);
                                                memcpy(record.content.date,ltm.date,2);    // Date
                                                memcpy(record.content.hour,ltm.hour,2);    // Hour
                                                memcpy(record.content.minute,ltm.minute,2);    // Minute
                                                sprintf(tt,"%06d",diffV);                   // This volume
                                                memcpy(record.content.volume,tt,6);
                                                sprintf(tt,"%06d",diffA);                 // This amount
                                                memcpy(record.content.amount,tt,6);
                                                memcpy(record.content.price,ptaxOnceResp->price,4);
                                                
                                                
                                                sprintf(ss,"%12lld",pDev->dataStore[pDev->gunCurrent].totalVolume);                                
                                                memcpy(record.content.Totalvolume,ss,12);//±¾´Î×ÜÀà¼ÓÓÍÁ¿´æÈë
                                                
                                                sprintf(ss,"%12lld",pDev->dataStore[pDev->gunCurrent].totalAmount);     
                                                memcpy(record.content.Totalamount,ss,12);//±¾´Î×ÜÀà½ð¶î´æÈë
 */                                               
                                               // storeTransactionRecord(&record);
                                               
                                       #if Concentrator_OS == 0   
                                                
                                                TaxTotalQueryResp totalresp;                                             
                                                sprintf(totalresp.volume,"%12lld",pDev->dataStore[pDev->gunCurrent].totalVolume); //±¾´Î×ÜÀà¼ÓÓÍÁ¿´æÈë                                                                                                                              
                                                sprintf(totalresp.amount,"%12lld",pDev->dataStore[pDev->gunCurrent].totalAmount); //±¾´Î×ÜÀà½ð¶î´æÈë                                                
                                                TaxTransactionDataRecord(&record, pDev, &totalresp, diffV,  diffA,  atol(ptaxOnceResp->price));                                                                                             
                                                
                                       #else
                                                TaxTransactionDataRecord(&record, pDev, 0, diffV,  diffA,  atol(ptaxOnceResp->price));
                                                
                                                
                                       #endif
                                                
                                                 storeTransactionRecord(&record,TaxData);
                                            
                                                 pDev->statistic[pDev->gunCurrent].successCount++;
                                                                             
                                       
//                                                u8 ret;
//                                                ret=MeterDataCmpTaxData(pDev->port,pDev->gunCurrent,diffA,diffV,atol(ptaxOnceResp->price),&record);
//                                                
//                                                if(ret<=1)								
//                                                {								
//                                                    pDev->statistic[pDev->gunCurrent].successCount++;
//                                                }
//                                                else
//                                                {
//                                                    pDev->statistic[pDev->gunCurrent].successCount+=ret;
//                                     
//                                                }
                                                
                                                return E_SUCCESS_TRANSACTION;
                                            }
                                    }
                                break;

                                default:
                                    {
                                        while(1)
                                        {
                                            printf("!!!!TaxControlDevSamplingOnConfirm:  Confirm sub state wrong\r\n");
                                        }   
                                     }
                                break;
                            }
                         }
                     else
                         {
                            size = sizeof(TaxOnceQueryCmd);
                            error=taxControlCmdCreate(TAX_CONTROL_CMD_QUERY_ONCE,pDev->gunCurrent,pCmdBuf,&size,TAX_RESPONSE_RETURN_PLAINTEXT);
                            error=uartWriteBuffer(pDev->port, pCmdBuf,size);
                            error=uartSendBufferOut(pDev->port);
                            return E_CHECK_CODE;            
                         }
                }
            else
                {
                    size = sizeof(TaxOnceQueryCmd);
                    error=taxControlCmdCreate(TAX_CONTROL_CMD_QUERY_ONCE,pDev->gunCurrent,pCmdBuf,&size,TAX_RESPONSE_RETURN_PLAINTEXT);
                    error=uartWriteBuffer(pDev->port, pCmdBuf,size);
                    error=uartSendBufferOut(pDev->port);
                    return E_MESSAGE_TYPE;							
    		   }
        }
    else
        {
            if(pDev->stateCount < TAX_CONTROL_CMD_WAIT_TIMEOUT)
                {
                    return E_WAITING;
                }
            else
                {
                    return E_DEVICE_BUSY;
                }
        }
}

/*******************************************************************************************************************************
Function Name: taxControlDevSamplingOnBusy
Description:
      Processing if tax control device is busy(maybe gun is working, or it is doing some inernal work). Experiment shows we the data from device may occur 
"Gun conflict issue", so the data is not reliable, we need discard it and query again. This funciton need handle 2 types of query, once is "ONCE", the other is
"TOTAL", they can be distinguished by "curCmd" member of 
Parameter:
	pDev:[I], points to device infomation TaxControlDevInfo structure. 
Return:
	E_SUCCESS: Received a complete packet from device and discard it.
	E_CHECK_CODE: Check code error.
	E_MESSAGE_TYPE: Got unexpected data message
	E_WAITING: Still need wait for device's response.
	E_DEVICE_BUSY: Device is busy, can't repsonce within TIMEOUT	
*******************************************************************************************************************************/
static errorCode taxControlDevSamplingOnBusy(TaxControlDevInfo *pDev)
{
    errorCode error=E_SUCCESS;
    TaxTotalQueryResp *pTotalResp;
    TaxOnceQueryResp *pOnceResp;
    unsigned char pCmdBuf[64];
    unsigned char pRespBuf[128];
    unsigned char availBytes=0;
    unsigned char cc=0x00;
    unsigned char i;
    unsigned short expRespSize,nextCmdSize; // 2 options: sizeof(TaxTotalQueryResp) or size of(TaxOnceQueryResp)
    unsigned char  respCC,respCmd;

    expRespSize  = (pDev->curCmd == TAX_CONTROL_CMD_QUERY_TOTAL)? sizeof(TaxTotalQueryResp):sizeof(TaxOnceQueryResp);
    nextCmdSize = (pDev->curCmd == TAX_CONTROL_CMD_QUERY_TOTAL)? sizeof(TaxTotalQueryCmd):sizeof(TaxOnceQueryCmd);
        
    availBytes = uartGetAvailBufferedDataNum(pDev->port);
    if(availBytes >= expRespSize)	// Got expected # of bytes, need parse them
        {
            taxControlResponseRetriever(pDev->port,pRespBuf,availBytes);
            if(pDev->curCmd == TAX_CONTROL_CMD_QUERY_TOTAL)
                {
                    pTotalResp = (TaxTotalQueryResp *)pRespBuf;
                    respCC = pTotalResp->cc;
                    respCmd = pTotalResp->header.cmd;
                }
            else
                {
                    pOnceResp =  (TaxOnceQueryResp *)pRespBuf;
                    respCC = pOnceResp->cc;
                    respCmd = pOnceResp->header.cmd;
                }
            
            if(respCmd == pDev->curCmd)
                {
                    for(i=2;i<expRespSize-1;i++)
                        cc = cc ^ pRespBuf[i];
                    if(cc == respCC)        // We got a result but maybe wrong,discard this one, change to query state to read again.
                        { 

                            unsigned char t[13];
                            long long diffV=0;
                            long long diffA=0;
                                            
                            t[12]=0;
                            memcpy(t,pTotalResp->volume,12);
                            pDev->dataStore[pDev->gunCurrent].totalVolume =atoll(t);

                            t[12]=0;
                            memcpy(t,pTotalResp->amount,12);
                            pDev->dataStore[pDev->gunCurrent].totalAmount=atoll(t);
                                
                            if(pDev->preData[pDev->gunCurrent].totalVolume == 0xffffffffffffffffLL)
                                {
                                    pDev->preData[pDev->gunCurrent].totalVolume  = pDev->dataStore[pDev->gunCurrent].totalVolume;
                                    pDev->preData[pDev->gunCurrent].totalAmount  =pDev->dataStore[pDev->gunCurrent].totalAmount;
                                }
                            diffV = pDev->dataStore[pDev->gunCurrent].totalVolume - pDev->preData[pDev->gunCurrent].totalVolume;
                            diffA = pDev->dataStore[pDev->gunCurrent].totalAmount - pDev->preData[pDev->gunCurrent].totalAmount;                            

                        
                            error=taxControlCmdCreate(pDev->curCmd,pDev->gunCurrent,pCmdBuf,&nextCmdSize,TAX_RESPONSE_RETURN_PLAINTEXT);
                            error=uartWriteBuffer(pDev->port, pCmdBuf,nextCmdSize);
                            error=uartSendBufferOut(pDev->port);
                            return E_SUCCESS;
                        }
                    else
                        {
                            if(availBytes > 0)
                                uartEmptyRcvBuffer(pDev->port);
                            
                            error=taxControlCmdCreate(pDev->curCmd,pDev->gunCurrent,pCmdBuf,&nextCmdSize,TAX_RESPONSE_RETURN_PLAINTEXT);
                            error=uartWriteBuffer(pDev->port, pCmdBuf,nextCmdSize);
                            error=uartSendBufferOut(pDev->port);
                           
                            return E_CHECK_CODE;                            
                        }
                }
            else
                {
                    if(availBytes > 0)
                        uartEmptyRcvBuffer(pDev->port);                    
                      
                     error=taxControlCmdCreate(pDev->curCmd,pDev->gunCurrent,pCmdBuf,&nextCmdSize,TAX_RESPONSE_RETURN_PLAINTEXT);
                     error=uartWriteBuffer(pDev->port, pCmdBuf,nextCmdSize);
                     error=uartSendBufferOut(pDev->port);
                    
                    return E_MESSAGE_TYPE;
                }
        }
    else
        {
            if(pDev->stateCount<= TAX_CONTROL_CMD_WAIT_TIMEOUT)
                {
                    return E_WAITING;
                }
            else
                {
                    if(availBytes > 0)
                        uartEmptyRcvBuffer(pDev->port);
                      
                     error=taxControlCmdCreate(pDev->curCmd,pDev->gunCurrent,pCmdBuf,&nextCmdSize,TAX_RESPONSE_RETURN_PLAINTEXT);
                     error=uartWriteBuffer(pDev->port, pCmdBuf,nextCmdSize);
                     error=uartSendBufferOut(pDev->port);
                    
                    return E_DEVICE_BUSY;
                }
        }

    
    return E_SUCCESS;
}
/*******************************************************************************************************************************
Function Name: taxControlDeviceRecoverStateToIdle
Description:
    This function intends to recover the tax control internal state to IDLE state to handle "the case of abnormal abort".
Attention:
    TAX_CONTROL_SAMPLING_STATE_INIT can't be changed to IDLE state by this API. INIT state must be naturally switched to IDLE
state.

PARA:
    taxDev:[I], Specify the tax control device
Return:
    E_INPUT_PARA:   Invalid taxDev input parameter
    E_SUCCESS:       Successfully completed.
*******************************************************************************************************************************/
errorCode taxControlDeviceRecoverStateToIdle(unsigned char taxDev)
{
    TaxControlDevInfo *pTaxControlDevInfo;

    if(taxDev >= TAX_CONTROL_DEVICE_MAX_NUM)
        return E_INPUT_PARA;

    
    pTaxControlDevInfo = (TaxControlDevInfo *)&taxControlDevManager.taxControlDevInfo[taxDev];

    if((pTaxControlDevInfo->sampleState == TAX_CONTROL_SAMPLING_STATE_INIT)  ||
       (pTaxControlDevInfo->sampleState == TAX_CONTROL_SAMPLING_STATE_IDLE))
    {
        return E_SUCCESS;
    }
    else
    {
        pTaxControlDevInfo->sampleState = TAX_CONTROL_SAMPLING_STATE_IDLE;
        if(pTaxControlDevInfo->processState != TAX_CONTROL_PROCESS_SUB_STATE_INIT)
        {
            pTaxControlDevInfo->processState = TAX_CONTROL_PROCESS_SUB_STATE_NORMAL;
        }   
        return E_SUCCESS;
    }
    
}
/*******************************************************************************************************************************
Function Name:taxControlSamplingGun()
Description:
     This is muti_running_loops API that means it should be called many times then can return the final result since it instlled internal state mchine which one call
execute one state.   This API intends to samplling the spcified gun onwed by the specified dev.
Para:
    dev:[I], Specify the dev index
    gunNo:[I], specificy the gunNo
RETURN:
    E_SUCCESS: Completed the gun polinng but not het real transaction
    E_SUCCESS_TRANSACTION: Completed the gun polling and get real transaction
    E_FAIL:  Fails to sample this device due to other issues.
    Other E_XXXXX:  The sampling is in progress
********************************************************************************************************************************/
errorCode   taxControlSamplingGun(unsigned char dev,unsigned char gunNo)
{
    errorCode res=E_SUCCESS;
    static unsigned char count=0;

    TaxControlDevInfo *pTaxControlDevInfo =  (TaxControlDevInfo *)&taxControlDevManager.taxControlDevInfo[dev];

    printf("taxControlSamplingGun: [State:%d] [SubState:%d] [Dev:%d] [GunNo:%d]\r\n",pTaxControlDevInfo->sampleState,pTaxControlDevInfo->processState,dev,gunNo);

    if(pTaxControlDevInfo->bind == TAX_CONTROL_DEVICE_BOUND)
        {
            pTaxControlDevInfo->stateCount++;       // !!!!This is the must to control the timeout and state change

            switch  (pTaxControlDevInfo->sampleState)
                {
                    case TAX_CONTROL_SAMPLING_STATE_INIT:
                    {                        
                        unsigned char buf[128];

                        if(ClockStateGet() == CLOCK_GTC)
                        {
                            unsigned short availBytes;
                            availBytes = uartGetAvailBufferedDataNum(pTaxControlDevInfo->port);
                            if(availBytes > 0)
                            {
                                if(availBytes > 128)
                                    uartRead(pTaxControlDevInfo->port,buf, 128);
                                else
                                    uartRead(pTaxControlDevInfo->port,buf, availBytes);
                            }
                            pTaxControlDevInfo->sampleState =TAX_CONTROL_SAMPLING_STATE_IDLE;
                        }
                        
                        res = E_IN_PROGRESS;                        
                    }
                    break;
                
                    case  TAX_CONTROL_SAMPLING_STATE_IDLE:
                        {
                            pTaxControlDevInfo->stateCount = 0;     // Clear StateCount
                            pTaxControlDevInfo->sampleState= TAX_CONTROL_SAMPLING_STATE_PROCESSING; // Change the state to next state.
                            pTaxControlDevInfo->curCmd = TAX_CONTROL_CMD_QUERY_TOTAL;   // Initialize the intial query command
                            pTaxControlDevInfo->gunCurrent = gunNo;                              
                            taxControlDevSamplingOnIdle(pTaxControlDevInfo);
                            
                            res = E_IN_PROGRESS;
                        }
                    break;

                    case TAX_CONTROL_SAMPLING_STATE_PROCESSING:
                            {
                                res = taxControlDevSamplingOnProcessing(pTaxControlDevInfo);
                                switch(res)
                                {
                                    case    E_GUN_INITIALIZE:
                                        {
                                            pTaxControlDevInfo->stateCount = 0;
                                            pTaxControlDevInfo->sampleState = TAX_CONTROL_SAMPLING_STATE_IDLE;
                                        }
                                    case    E_WAITING:  // Nothing need to do since normal waiting
                                        //return E_IN_PROGRESS;
                                    break;
                                    
                                    case    E_AMBIGUITY:
                                        {
                                            pTaxControlDevInfo->curCmd = TAX_CONTROL_CMD_QUERY_ONCE;
                                            pTaxControlDevInfo->stateCount = 0;
                                            pTaxControlDevInfo->sampleState = TAX_CONTROL_SAMPLING_STATE_CONFIRM;
                                        }
                                    break;
                                    
                                    case    E_SUCCESS:
                                        {
                                            pTaxControlDevInfo->sampleState= TAX_CONTROL_SAMPLING_STATE_IDLE;
                                            return E_SUCCESS;
                                        }
                                    case    E_SUCCESS_TRANSACTION:
                                        {
                                            pTaxControlDevInfo->sampleState= TAX_CONTROL_SAMPLING_STATE_IDLE;
                                            return E_SUCCESS_TRANSACTION;
                                        }
                                    break;
                                    
                                    case    E_CHECK_CODE:
                                    case    E_MESSAGE_TYPE:
                                        {
                                            pTaxControlDevInfo->curCmd = TAX_CONTROL_CMD_QUERY_TOTAL;
                                            pTaxControlDevInfo->sampleState = TAX_CONTROL_SAMPLING_STATE_IDLE;
                                        }                                                                
                                    break;
                                    
                                    case    E_DEVICE_BUSY:
                                        {
                                            pTaxControlDevInfo->stateCount= 0;
                                            pTaxControlDevInfo->curCmd = TAX_CONTROL_CMD_QUERY_TOTAL;
                                            pTaxControlDevInfo->sampleState= TAX_CONTROL_SAMPLING_STATE_BUSY;
                                        }
                                    break;

                                    case    E_FAIL:     // something wrong with the data, need restart flow
                                        {
                                            pTaxControlDevInfo->curCmd = TAX_CONTROL_CMD_QUERY_TOTAL;
                                            pTaxControlDevInfo->sampleState= TAX_CONTROL_SAMPLING_STATE_IDLE;                                                                    
                                        }
                                    break;
                                    case    E_WAIT_CONFIRM:
                                        {
                                             pTaxControlDevInfo->stateCount= 0;   
                                        }
                                    break;
                                    default:
                                     {
                                            while(1)
                                             {
                                                    printf("!!!!taxControlDevSamplingOnProcessing: Port:%d Gun:%d Return unexpected result :%d @%x\r\n",pTaxControlDevInfo->port,pTaxControlDevInfo->gunCurrent,res,getLocalTick());
                                             }   
                                            
                                     }   
                                    break;    
                                }                          
                            }
                                    
                            break;

                              case TAX_CONTROL_SAMPLING_STATE_BUSY:
                                 {
                                    res = taxControlDevSamplingOnBusy(pTaxControlDevInfo);
                                    
                                    switch(res)
                                         {
                                                case E_WAITING: // Normal waiting, needs nothing to do.
                                                break;
                                                
                                                case E_SUCCESS: // This state doesn't need change command, reset count, return to the original state based on the command,
                                                    {
                                                        pTaxControlDevInfo->stateCount = 0;
                                                        pTaxControlDevInfo->sampleState = (pTaxControlDevInfo->curCmd == TAX_CONTROL_CMD_QUERY_TOTAL)?TAX_CONTROL_SAMPLING_STATE_PROCESSING:TAX_CONTROL_SAMPLING_STATE_CONFIRM;
                                                        res = E_IN_PROGRESS;
                                                    }
                                                break;

                                                // Below error case just need reset count to 0, stay this state until successfully.
                                                case E_MESSAGE_TYPE:
                                                case E_CHECK_CODE:
                                                    {
                                                        pTaxControlDevInfo->stateCount = 0;
                                                    }
                                                break;    
                                                case E_DEVICE_BUSY:
                                                    {
                                                        pTaxControlDevInfo->stateCount = 0;
                                                    }   
                                                break;
                                                default:
                                                    {
                                                        while(1)
                                                            {
                                                                printf("taxControlDevSamplingOnBusy: Port:%d Gun:%d Return unexpected result:%d @%x\r\n",pTaxControlDevInfo->port,pTaxControlDevInfo->gunCurrent,res,getLocalTick());
                                                            }    
                                                    }
                                                break;    
                                           }
                                 }
                              break;

                              case TAX_CONTROL_SAMPLING_STATE_CONFIRM:
                                  {
                                        res = taxControlDevSamplingOnConfirm(pTaxControlDevInfo);   // based on the 2nd time processing, processing "price" together w/ previous "total"
                                        switch(res)
                                            {
                                                case E_WAITING:
                                                    {
                                                        ;//printf("taxControlDevSamplingOnConfirm: Port %d Gun:%d waiting for response@%x\r\n",pTaxControlDevInfo->port,pTaxControlDevInfo->gunCurrent,getLocalTick());
                                                    }       
                                                case E_SUCCESS:
                                                    {
                                                        pTaxControlDevInfo->sampleState = TAX_CONTROL_SAMPLING_STATE_IDLE;
                                                        return E_SUCCESS;
                                                    }
                                                case E_SUCCESS_TRANSACTION:    
                        					         {
                                                        pTaxControlDevInfo->sampleState = TAX_CONTROL_SAMPLING_STATE_IDLE;
                                                        res = E_SUCCESS_TRANSACTION;
                                                     }
                                                break;

                                                case E_MESSAGE_TYPE:
                                                case E_CHECK_CODE:
                                                    {
                                                        pTaxControlDevInfo->stateCount = 0;
                                                        printf("taxControlDevSamplingOnConfirm:Port:%d Gun:%d Meet checkcode/message type error:%d @%x\r\n",pTaxControlDevInfo->port,pTaxControlDevInfo->gunCurrent,res,getLocalTick());
                                                    }
                                                break;

                                                case E_DEVICE_BUSY:
                                                    {
                                                        pTaxControlDevInfo->stateCount = 0;
                                                        pTaxControlDevInfo->sampleState = TAX_CONTROL_SAMPLING_STATE_BUSY;
                                                        printf("taxControlDevSamplingOnConfirm: Port:%d Gun:%d To enter BUSY state @%x\r\n",pTaxControlDevInfo->port,pTaxControlDevInfo->gunCurrent,getLocalTick());
                                                    }
                                                break;

                                                case E_WAIT_CONFIRM:
                                                    {
                                                        pTaxControlDevInfo->stateCount = 0;
                                                        pTaxControlDevInfo->sampleState = TAX_CONTROL_SAMPLING_STATE_CONFIRM;       
                                                    }
                                                break;
                                                default:
                                                    {
                                                        while(1)
                                                        {
                                                            printf("taxControlDevSamplingOnConfirm: Port:%d Gun:%d Return unexpected result:%d @%x\r\n",pTaxControlDevInfo->port,pTaxControlDevInfo->gunCurrent,res,getLocalTick());
                                                        }                                                  
                                                    }
                                                break;    
                                            }
                                  }
                              break;

                              default:
                              break;
                             }
        }
    else
        {
            printf("taxControlSamplingGun: This device is unbind, return E_INPROGRESS\r\n");
            return E_IN_PROGRESS;
        }
    return res;
}
/*******************************************************************************************************************************
Function Name: taxControlSamplingDev()
Description:
     This is muti_running_loops API that means it should be called many times then can return the final result since it instlled internal state mchine which one call
execute one state.   This API intends to samplling all guns onwed by the specified dev.     

Para: 
    dev: [IN], speicified tax control dev index
    sucNum:[IN/OUT],  how many guns samplled real trasaction data
    gunNo:[IN/OU]. The last gunNo that successfully samplled transaction data

RETURN:
    E_SUCCESS: Completed the dev polinng by going through all guns owned by this device
    E_IN_PROGRESS:   Sampling is in progress, yet to call further.
********************************************************************************************************************************/
errorCode   taxControlSamplingDev(unsigned char dev, unsigned char *sucNum, unsigned char *gunNo )
{
    errorCode   res=E_SUCCESS;
    static  unsigned char   gunSampledCount[2]={0};
    static  unsigned char   sucCount[2]={0};
    static  unsigned char   sucGunNo[2] = {0}; 
    TaxControlDevInfo *pTaxControlDevInfo = (TaxControlDevInfo *)&taxControlDevManager.taxControlDevInfo[dev];

    if(gunSampledCount[dev] < pTaxControlDevInfo->gunTotal)
    {
        res = taxControlSamplingGun(dev,gunSampledCount[dev]);
        if(res == E_SUCCESS_TRANSACTION)
        { 
            
            sucCount[dev]++;                 // Got a real transaction, so success count increment
            sucGunNo[dev] = gunSampledCount[dev];
            gunSampledCount[dev]++;     // swith to another gun
            
            taxControlDeviceRecoverStateToIdle(dev); 
        }
        if(res == E_SUCCESS || res == E_FAIL)
        {
            gunSampledCount[dev]++;      // complete a gun's polling w/o a real transaction, swith another gun
            taxControlDeviceRecoverStateToIdle(dev);
        }
        //printf("taxControlSamplingGun: return %d\r\n",res);
        return E_IN_PROGRESS;       // Not completed, so return E_IN_PROGRESS to continue
    }
    else    // Completed polling all the guns owned by this deivce. return E_SUCCESS
    {
        *sucNum = sucCount[dev];
        *gunNo = sucGunNo[dev];
        gunSampledCount[dev] =0;
        sucCount[dev] = 0;
        return E_SUCCESS;
    }
}

