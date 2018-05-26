/***************************************************************************************
File name: uartPort.c
Date: 2016-8-24
Author: Wangwei/GuokuYan
Description:
     This file contain all the interface for uart related operation
***************************************************************************************/
#include "stm32f10x.h"		/* 如果要用ST的固件库，必须包含这个文件 */
#include "SEGGER_RTT.h"
#include "bsp_usart.h"
#include "bsp_timer.h"
#include "errorCode.h"
#include "uartPort.h"
#include "meterMessager.h"
#include "board.h"

#define dbgPrintf   SEGGER_RTT_printf

UartRBuffer	uartRBuffer[UART_TOTAL_PORT_NUM];
UartSBuffer	uartSBuffer[UART_TOTAL_PORT_NUM];
UartPortState	uartPortState[UART_TAX_PORT_NUM];
UartPortState	uartPortCurrentState[UART_TAX_PORT_NUM];
/*********************************************************************************************************
Function Name: uartBufferInitialize
Description:
	Initialize uart ports and working buffer management structure.
Parameter:
    NULL.
Return:
    E_NO_MEMORY:  malloc buffer fail.
    E_SUCCESS: Initialize uart port successfully.
**********************************************************************************************************/	
errorCode uartBufferInitialize()
{
    int i=0;
	
    // initialize tax uart port[0,1] buffer
    for(i=0;i<UART_TAX_PORT_NUM;i++)
	{
		uartRBuffer[i].size = TAX_RCV_BUFFER_SIZE;
		uartRBuffer[i].rptr = 0;
		uartRBuffer[i].wptr = 0;
		uartRBuffer[i].overFlow = 0;
		uartRBuffer[i].data =(unsigned char *)malloc(TAX_RCV_BUFFER_SIZE);
		if(uartRBuffer[i].data == 0)
		{
			printf("uartTaxBufferInitialize:Maclloc read buffer %d fails\r\n",i);
			return E_NO_MEMORY;
		}
		
		uartSBuffer[i].size = TAX_SND_BUFFER_SIZE;
		uartSBuffer[i].rptr = 0;
		uartSBuffer[i].wptr = 0;
		uartSBuffer[i].data =(unsigned char *)malloc(TAX_SND_BUFFER_SIZE);
		if(uartSBuffer[i].data == 0)
		{
			printf("uartTaxBufferInitialize:Maclloc send buffer %d fails\r\n",i);
			return E_NO_MEMORY;
		}

		uartPortState[i].cState = CONNECTION_STATE_INIT;
		uartPortState[i].bState= PORT_UNBOUND;;
	}

	
//	 if(DEBUG_PORT_EN)// Initialize DEBUG uart port[2]
	 {
		uartRBuffer[UART_DEBUG_PORT_BASE].size = DEBUG_RCV_BUFFER_SIZE;
		uartRBuffer[UART_DEBUG_PORT_BASE].rptr = 0;
		uartRBuffer[UART_DEBUG_PORT_BASE].wptr = 0;
		uartRBuffer[UART_DEBUG_PORT_BASE].overFlow = 0;
		uartRBuffer[UART_DEBUG_PORT_BASE].data =(unsigned char *)malloc(DEBUG_RCV_BUFFER_SIZE);
		if(uartRBuffer[UART_DEBUG_PORT_BASE].data == 0)
		{
			printf("uartTaxBufferInitialize:Maclloc read buffer %d fails\r\n",UART_DEBUG_PORT_BASE);
			return E_NO_MEMORY;
		}
		uartSBuffer[UART_DEBUG_PORT_BASE].size = DEBUG_SND_BUFFER_SIZE;
		uartSBuffer[UART_DEBUG_PORT_BASE].rptr = 0;
		uartSBuffer[UART_DEBUG_PORT_BASE].wptr = 0;
		uartSBuffer[UART_DEBUG_PORT_BASE].data =(unsigned char *)malloc(DEBUG_SND_BUFFER_SIZE);
		if(uartSBuffer[UART_DEBUG_PORT_BASE].data == 0)
		{
			printf("uartTaxBufferInitialize:Maclloc send buffer %d fails\r\n",UART_DEBUG_PORT_BASE);
			return E_NO_MEMORY;
		}
            
     }
	 
   // if(getMeterMode())// Initialize meter uart port[3]
    {
		uartRBuffer[UART_METER_PORT_BASE].size = METER_RCV_BUFFER_SIZE;
		uartRBuffer[UART_METER_PORT_BASE].rptr = 0;
		uartRBuffer[UART_METER_PORT_BASE].wptr = 0;
		uartRBuffer[UART_METER_PORT_BASE].overFlow = 0;
		uartRBuffer[UART_METER_PORT_BASE].data =(unsigned char *)malloc(METER_RCV_BUFFER_SIZE);
		
//		printf("uartBuf[3]:%08X,rptr=%08X,wptr=%08X\r\n",uartRBuffer[UART_METER_PORT_BASE].data,
//										   &(uartRBuffer[UART_METER_PORT_BASE].rptr),
//										   &(uartRBuffer[UART_METER_PORT_BASE].wptr));
		
		if(uartRBuffer[UART_METER_PORT_BASE].data == 0)
		{
			printf("uartTaxBufferInitialize:Maclloc read buffer %d fails\r\n",UART_METER_PORT_BASE);
			return E_NO_MEMORY;
		}
		uartSBuffer[UART_METER_PORT_BASE].size = METER_SND_BUFFER_SIZE;
		uartSBuffer[UART_METER_PORT_BASE].rptr = 0;
		uartSBuffer[UART_METER_PORT_BASE].wptr = 0;
		uartSBuffer[UART_METER_PORT_BASE].data =(unsigned char *)malloc(METER_SND_BUFFER_SIZE);
		if(uartSBuffer[UART_METER_PORT_BASE].data == 0)
		{
			printf("uartTaxBufferInitialize:Maclloc send buffer %d fails\r\n",UART_METER_PORT_BASE);
			return E_NO_MEMORY;
		}
            
     }


    // Initialize upstreaming uart port(PLC/485/etc) 
    uartRBuffer[UART_UPSTREAM_NO].size = UPSTREAM_RCV_BUFFER_SIZE;
    uartRBuffer[UART_UPSTREAM_NO].rptr = 0;
    uartRBuffer[UART_UPSTREAM_NO].wptr = 0;
    uartRBuffer[UART_UPSTREAM_NO].overFlow = 0;
    uartRBuffer[UART_UPSTREAM_NO].data =(unsigned char *)malloc(UPSTREAM_RCV_BUFFER_SIZE);
    if(uartRBuffer[UART_UPSTREAM_NO].data == 0)
        {
            printf("uartTaxBufferInitialize:Maclloc read buffer 5 fails\r\n");
            return E_NO_MEMORY;
        }
    uartSBuffer[UART_UPSTREAM_NO].size = UPSTREAM_SND_BUFFER_SIZE;
    uartSBuffer[UART_UPSTREAM_NO].rptr = 0;
    uartSBuffer[UART_UPSTREAM_NO].wptr = 0;
    uartSBuffer[UART_UPSTREAM_NO].data =(unsigned char *)malloc(UPSTREAM_SND_BUFFER_SIZE);
    if(uartSBuffer[UART_UPSTREAM_NO].data == 0)
    	{
            printf("uartTaxBufferInitialize:Maclloc send buffer 5 fails\r\n");
            return E_NO_MEMORY;
       }
    
  //  printf("Uart Port Buffer/State Completes Initialization: UART5R:%x\r\n",&uartRBuffer[4]);
    return E_SUCCESS;
}
/************************************************************************************
Description:
	Move received data into uart buffer, this funciton intends to be called in uart ISR.
Para:
	port:[I], uart port No
	c:[I], received data
return:
	NULL
**************************************************************************************/	
void uartPutCharIntoRcvBuffer(unsigned char  port, unsigned char c)
{
	UartRBuffer *pUartBuf = &uartRBuffer[port];

	pUartBuf->data[pUartBuf->wptr]=c;
	pUartBuf->wptr++;
	if(pUartBuf->wptr==pUartBuf->size)		// wrap around
		{
			pUartBuf->wptr=0;
		}
}

void uartPutCharsIntoRcvBuffer(unsigned char  port, unsigned char *pBuf, short size)
{
    UartRBuffer *pUartBuf = &uartRBuffer[port];

    for(int i=0;i<size;i++)
       uartPutCharIntoRcvBuffer(port,pBuf[i]);
    
}
/************************************************************************************
Function name: uartGetAvailDataNum
para:
	port: [I]  usart port No
return:
	return the # of avaialable of data in usart buffer, !!! buffer's rptr doesn't change. you can fetch them later
by calling "readUartData(UartPortNum port, u8 *buf, u8 len)".
*************************************************************************************/
u16  uartGetAvailBufferedDataNum(UartPortNum port)
{
	u16 availNo=0;
	UartRBuffer *pUartBuf = &uartRBuffer[port];

	if(port >= UART_PORT_MAX)
		return availNo;

	if(pUartBuf->wptr >= pUartBuf->rptr){
		availNo = pUartBuf->wptr - pUartBuf->rptr;
	}
	else{
		availNo = pUartBuf->wptr + pUartBuf->size - pUartBuf->rptr;
	}

	return availNo;
}
/************************************************************************************
Function Name: uartPurgeRcvBuffer()
Description:
	Empty the uart receive buffer until there is no incoming data. Assuming minmum baudrate is 9600bps.
Parameter:
	None.
Return:
	None.
************************************************************************************/
void uartEmptyRcvBuffer(UartPortNum port)
{
	unsigned short count1,count2;
	count1 =  uartGetAvailBufferedDataNum(port);
		
	do{
		bsp_DelayMS(10);
		count2 =  uartGetAvailBufferedDataNum(port);
		if(count2 != count1)
 			count1 = count2;
		
	} while(count1 != count2);
	
	uartRBuffer[port].rptr=uartRBuffer[port].wptr;
	
	
	
}

void uartClearRcvBuffer(UartPortNum port)
{
	unsigned short count1,count2;
	count1 =  uartGetAvailBufferedDataNum(port);
		
	do{
		bsp_DelayMS(10);
		count2 =  uartGetAvailBufferedDataNum(port);
		if(count2 != count1)
 			count1 = count2;
		
	} while(count1 != count2);
	
	uartRBuffer[port].rptr=0;
	uartRBuffer[port].wptr=0;
		
	
}
/************************************************************************************
Function name: uartRead
para:
	port: [I]  usart port No
	buf:	 [I/O]  data buffer
	len:  [I]   expected byte number
return:
	E_SUCCESS: when fetch number of data
	E_EMPTY:  the buffer is empty
	E_INPUT_PARA: At lease one input para is wrong. 
	E_FAIL: when fail to fetch expected number of data
*************************************************************************************/
errorCode uartRead(UartPortNum port, unsigned char *buf, unsigned short len)
{
	unsigned short t;
	UartRBuffer *pUartBuf = &uartRBuffer[port];

       
	if((len >= pUartBuf->size) || (buf == 0) || (port >=UART_PORT_MAX))
            {  
                printf("uartRead: Port:%d input para error\r\n");
                return E_INPUT_PARA;
            }

	if(pUartBuf->wptr == pUartBuf->rptr)
		return E_EMPTY;

       
	if(pUartBuf->rptr + len <= pUartBuf->size)
		{
			memcpy(buf,&pUartBuf->data[pUartBuf->rptr],len);
		}
	else
		{
                    memcpy(buf,&pUartBuf->data[pUartBuf->rptr],pUartBuf->size-pUartBuf->rptr);
                    memcpy(buf+pUartBuf->size-pUartBuf->rptr,&pUartBuf->data[0],pUartBuf->rptr+ len - pUartBuf->size);
		}
    
    pUartBuf->rptr = pUartBuf->rptr + len;
    if(pUartBuf->rptr >= pUartBuf->size)
        pUartBuf->rptr = pUartBuf->rptr - pUartBuf->size;
	
    return E_SUCCESS;
}

/****************************************************************************************************
Function Name: uartWriteBuffer
Description:  Put command frame into uart send buffer.
Para:
	port:[I], UART port number;
	buf:[I],  source data
	len:[I], source data length
Return:
	E_SUCCESS: source data copyed to send buffer successfully.
	E_DEVICE_BUSY: send buffer is not empty, device is busy. normally "send is interrupt mode". 
	E_INPUT_PARA:  At lease one input para is wrong.
*****************************************************************************************************/
errorCode uartWriteBuffer(UartPortNum port,unsigned char *buf,unsigned short len)
{
	if((len >= uartSBuffer[port].size) || (buf == 0) || (port >=UART_PORT_MAX))
            {
                printf("uartWriteBuffer: Input parameter error\r\n");
                return E_INPUT_PARA;
            }
    
	if(uartSBuffer[port].rptr < uartSBuffer[port].wptr)
		{
			return E_DEVICE_BUSY;
		}
	else	
		{
			if((uartSBuffer[port].size -uartSBuffer[port].wptr) >= len)
				{
					memcpy(&uartSBuffer[port].data[uartSBuffer[port].wptr],buf,len);
					uartSBuffer[port].wptr = uartSBuffer[port].wptr + len;
					if(uartSBuffer[port].wptr == uartSBuffer[port].size)
						uartSBuffer[port].wptr = 0;
				}
			else
				{
					memcpy(&uartSBuffer[port].data[uartSBuffer[port].wptr],buf,uartSBuffer[port].size -uartSBuffer[port].wptr);
					memcpy(&uartSBuffer[port].data[0],&buf[uartSBuffer[port].size -uartSBuffer[port].wptr],len-(uartSBuffer[port].size -uartSBuffer[port].wptr));
					uartSBuffer[port].wptr = uartSBuffer[port].wptr + len - uartSBuffer[port].size;
				}
		}
	return E_SUCCESS;
}

/************************************************************************************************
Function Name: uartSendBufferOut
Description:
	This function use polling mode to send buffed data out of uart port, this function call lower layer BSP's
 "void USART_SendData(USART_TypeDef* USARTx, uint16_t Data)" API. This function is sync(blocking) call, it return after all
 buffered data out.
 Para:
 	port:[I], USART port
 Return:
 	E_SUCCESS
 *************************************************************************************************/
errorCode uartSendBufferOut(UartPortNum port)
{
    USART_TypeDef * USARTn;
    
    switch(port)
        {
            case	UART_TAX_PORT_0:
                USARTn = USART1;
            break;
			
            case	UART_TAX_PORT_1:
                USARTn = USART2;
            break;

            case UART_DEBUG_PORT:
                USARTn = USART3;
            break;

            case UART_METER_PORT:
                USARTn = UART4;
            break;    

            case  UART_UPSTREAM_PORT:
                USARTn = UART5;
            break;
                     
            default:
            break;
	}

	do
	{				 
		USART_SendData(USARTn,uartSBuffer[port].data[uartSBuffer[port].rptr]);
		while(USART_GetFlagStatus(USARTn, USART_FLAG_TC) == RESET);
	 
		uartSBuffer[port].rptr++;
		if(uartSBuffer[port].rptr == uartSBuffer[port].size )
			{
				uartSBuffer[port].rptr=0;
			}
		
	}while(uartSBuffer[port].rptr != uartSBuffer[port].wptr);
	
	return E_SUCCESS;
}

/************************************************************************************************
Function Name: uartConnectionDetect
Description:
	This funciton intends to detect RxD signal to decide the cable connected to an active taxControlDevice or not.
 Para:
 	pUartPortState:[I/O], UartPortState pointer.
 Return:
 	E_SUCCESS

 More:
 	GPIO PORTC[0,3] connect to UART3/2/1/0's RXD signal. 
 *************************************************************************************************/
errorCode uartConnectionDetectBlocked(UartPortState *pUartPortState)
{
    volatile u16 ioData;
    u8 count[UART_TAX_PORT_NUM]={0};
    u8 i,j;
    u8 N=5,X=2;
    for(i=0;i<N;i++)		// Repeat detecting N times, if the singal is high(connected), then the count number +1;
        {
            ioData = GPIO_ReadInputData(GPIOC);
            ioData&=0x0F;
            for(j=0;j<UART_TAX_PORT_NUM;j++)
                {
                    if(ioData&(1<<j))
                        {
                            count[j]++;
                        }
                 }
            bsp_DelayMS(70);	// delay to skip the maxium taxcontroldevice repsonse transmission
        }
    for(i=0;i<UART_TAX_PORT_NUM;i++)
        {
            pUartPortState[i].cState= CONNECTION_STATE_CONNECT;
            if(count[i]>=((N+X)/2))		// >50% probability, means connected
                {
                    pUartPortState[i].cState= CONNECTION_STATE_INIT;
                }
        }
    return E_SUCCESS;	
}


errorCode uartConnectionDetect(UartPortState *pUartPortState)
{ 
    volatile u16 ioData;

	u8 i,j;
	u8 N=5,X=2;

	static u8 state=0;
	static u8 count[UART_TAX_PORT_NUM]={0};
	  
	if(state<=N)
	{
		ioData = GPIO_ReadInputData(GPIOC);
		ioData&=0x0F;
							
		for(j=0;j<UART_TAX_PORT_NUM;j++)
		{
			if(ioData&(1<<j))
			{
				count[j]++;																			
			}				
		}
		
		state++;
				
	}
	
	if(state>N)
	{
		for(i=0;i<UART_TAX_PORT_NUM;i++)
		{
			pUartPortState[i].cState= CONNECTION_STATE_CONNECT;
			if(count[i]>=((N+X)/2))		// >50% probability, means connected
				pUartPortState[i].cState= CONNECTION_STATE_INIT;
										
		}
		state=0;
		memset(count,0,UART_TAX_PORT_NUM);
		
		return E_SUCCESS;	
		
	}
		   
	return E_WAITING;		
							
}


/************************************************************************************************
Function Name: uartGetConnectionState
Description:
	This funciton intends to get previous UART port connection state.
 Para:
 	NULL.
 Return:
 	UartPortState pointer.
 *************************************************************************************************/
UartPortState *uartGetPortState()
{
	return (UartPortState *)uartPortState;
}

UartPortState *uartGetPortCurrentState()
{
	return (UartPortState *)uartPortCurrentState;
}
/************************************************************************************************

************************************************************************************************/
errorCode uartSetPortState(UartPortState *pUartPortState)
{
	memcpy((char *)uartPortState,(char *)pUartPortState, sizeof(UartPortState));
	return E_SUCCESS;
}

errorCode uartPortBufferReset(UartPortNum port)
{
    uartRBuffer[port].rptr = 0;
    uartRBuffer[port].wptr = 0;
    uartSBuffer[port].rptr = 0;
    uartSBuffer[port].wptr = 0;

    return E_SUCCESS;
}

void Uart5_Pin_Disable(void)
{
    
    USART_Cmd(UART5, DISABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &GPIO_InitStructure);
}




