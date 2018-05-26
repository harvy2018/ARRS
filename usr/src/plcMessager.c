/***************************************************************************************
File name: plcMessager.c
Date: 2016-9-14
Author: GuoxiZhang/Wangwei/GuokuYan
Description:
     This file contain all the interface and defaulr parameter to control the PLC modem,not inlucde the application
protocal to layer the sofware component.. 
***************************************************************************************/
#include "stm32f10x.h"
#include "SEGGER_RTT.h"
#include "uartPort.h"
#include "flashMgr.h"
#include "plcMessager.h"
#include "swapOrder.h"
#include "board.h"
#include "crc16.h"
#include "snr.h"

#define dbgPrintf	SEGGER_RTT_printf
u32 PLCResetCnt=0;
// Define the global parameters here
static UINT16 _PAN_Id = PANID;
static UINT16 _LBA_Address = 0;

static BYTE genIpv6Addr[16] = {0xFE,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xFE,0x00,0x00,0x00};

static int snrInt=0,snrDec=0;
static float snr=0;

static BYTE ToneMasks[13][14]  = {
{0x17, 0x24, 0x00, 0xFF, 0xFF, 0xFF, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 0 =  Cenelec A36
{0x17, 0x24, 0xFF, 0xFF, 0x00, 0xF8, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 1 = Cenelec A25
{0x3F, 0x10, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 2 = Cenelec B
{0x3F, 0x1A, 0xFF, 0xFF, 0xFF, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 3 = Cenelec BC
{0x3F, 0x20, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 4 = Cenelec BCD
{0x21, 0x24, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 5 = FCC Low Band
{0x45, 0x24, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 6 = FCC High Band
{0x21, 0x48, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00 }, // 7 = FCC Full Band.
{0x3B, 0xA4, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 8 = ????
{0x1F, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00 }, // 9 = ????
{0x21, 0x0C, 0xFF, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 10= FCC ARIB 12 
{0x21, 0x18, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 11= FCC ARIB 24
{0x21, 0x36, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00 }  // 12= FCC ARIB 54
};

static PLCModulationMode TX_Modulation = PLC_MODULATION_BPSK;//PLC_MODULATION_BPSK;//;PLC_MODULATION_ROBO

static PLCMessagerStateManager plcMsgStateMgr = {PLC_MSG_STATE_RESET,0,0,{{0},PLC_MSG_RCV_EMPTY,0}};

// Define the maxium loop waiting count for each state.
// Experiment reference delay:
static unsigned int plcChStateDuration[PLC_MSG_STATE_MAX]={     2,      //PLC_MSG_STATE_PWROFF
                                                                50,      //PLC_MSG_STATE_PWROFF_DELAY      
                                                                2,      //PLC_MSG_STATE_PWRON   
                                                                50,    //PLC_MSG_STATE_PWRON_DELAY 
                                                                5,      //PLC_MSG_STATE_RESET: <100ms
                                                                5,      //PLC_MSG_STATE_GETSYSTEMINFO <100ms
                                                                2,      // PLC_MSG_STATE_LOADSYSTEMCFG_PORT <100ms     
                                                                2,      // PLC_MSG_STATE_LOADSYSCFG_DEVICE <100ms
                                                                2,      // PLC_MSG_STATE_LOADSYSCFG_G3 <100ms
                                                                2,      // PLC_MSG_STATE_SHUTDOWN <100ms
                                                                20,      // PLC_MSG_STATE_POSTSHUTDOWN <1500ms
                                                                2,      // PLC_MSG_STATE_TX_INFO <100ms
                                                                2,      // PLC_MSG_STATE_RX_INFO <100ms
                                                                300,      // PLC_MSG_STATE_ATTACH <?
                                                                0xffffffff,      // PLC_MSG_STATE_NETWORK
                                                          };

FuncAppMessageHandler   pAppMessageHandler = NULL;

// Helper function to do swap and caculate CRC.
static void SwapBytes(DataTransferRequest * _pDTR)
{
	//
	// Swap the bytes... for the UINT16 fields.
	//
	_pDTR->_IPv6_Header._PayloadLength = SwapOrder_UInt16(_pDTR->_IPv6_Header._PayloadLength);
	_pDTR->_UDP_Message._DestinationPort = SwapOrder_UInt16(_pDTR->_UDP_Message._DestinationPort);
	_pDTR->_UDP_Message._SourcePort = SwapOrder_UInt16(_pDTR->_UDP_Message._SourcePort);
	_pDTR->_UDP_Message._Length = SwapOrder_UInt16(_pDTR->_UDP_Message._Length);
	_pDTR->_UDP_Message._CRC = SwapOrder_UInt16(_pDTR->_UDP_Message._CRC);
}


static UINT16 CalculateCRC(DataTransferRequest * dtr)
{
	UINT32 tempCRC = 0;
	UINT16 _CRC = 0;	
	
	UINT16 Length = dtr->_UDP_Message._Length;
	int size = 16;//Source add
	size += 16;//Destination add
	size += 4;//Length(4bytes);
	size += 4;//Three zeroed bytes + Next header byte
	size += 2;//Source Port(2 bytes)
	size += 2;//Dest port (2 bytes)
	size += 2;//Length(2 bytes)
	size += 2;//checksum (2 zeroed out bytes)
	size += Length - 8;//payload length( total - header(header = 8 bytes)
	
	if((size & 1) == 1) //Make the message end on an even boundary
		size++;
	
	BYTE *ba = (BYTE*) malloc(size);
	if(ba == NULL)
		return _CRC;
	
	int pos=0;
	memcpy(ba,dtr->_IPv6_Header._SourceAddress,16);
	pos += 16;
	
	memcpy(ba + pos, dtr->_IPv6_Header._DestinationAddress,16);
	pos += 16;
	
	ba[pos++] = 0;
	ba[pos++] = 0;	
	ba[pos++] = (Length >> 8) & 0xFF;
	ba[pos++] = Length & 0xFF;
	
	ba[pos++] = 0;
	ba[pos++] = 0;
	ba[pos++] = 0;
	ba[pos++] = 0x11;
	
	ba[pos++] = (dtr->_UDP_Message._SourcePort >> 8) & 0xFF;
	ba[pos++] = dtr->_UDP_Message._SourcePort & 0xFF;
	
	ba[pos++] = (dtr->_UDP_Message._DestinationPort >> 8) & 0xFF;
	ba[pos++] = dtr->_UDP_Message._DestinationPort & 0xFF;
	
	ba[pos++] = (Length >> 8) & 0xFF;
	ba[pos++] = Length & 0xFF;
	
	ba[pos++] = 0;
	ba[pos++] = 0;
	
	memcpy(ba + pos, dtr->_UDP_Message._Data, Length - 8);
	pos += Length - 8;
	
	if((Length & 1) == 1)
		ba[pos++] = 0;
	
	for (int index = 0; index < size; index += 2)
	{
		UINT16 temp = ba[index+1] + (ba[index] << 8);
		tempCRC += temp;
	}
	
	while ((tempCRC >> 16) > 0)
	{
		tempCRC = (tempCRC & 0xFFFF) + (tempCRC >> 16);
	}
	
	_CRC = ((UINT16)~tempCRC);
	dtr->_UDP_Message._CRC = _CRC;
	
	free(ba);
	
	return _CRC;
}
/**************************************************************************************
Function Name: MakeCRC_Header
Description:
   Make CRC header, define as "static" since limited the usage only in this file.
Parameter:
    ......
Return:
   NULL
***************************************************************************************/
void MakeCRC_Header(CRC_Header * crcHeader, void * header, int headerLength, void * body, int bodyLength)
{
	crcHeader->_HeaderCRC = CRC16_BlockChecksum( header, headerLength);
	if (bodyLength == 0)
	{
		crcHeader->_MessageCRC = 0;
	}
	else
	{
		crcHeader->_MessageCRC = CRC16_BlockChecksum(body, bodyLength);
	}
}

// Helper function
static void * FormDataTransferMessage(void * transferStruct, BYTE * msg, int size)
{
	DataTransfer * newMessage;
	DataTransfer * orgMessage;
	//
	// Size without data..only the basic message with the IPv6 and UPD (no data) headers..
	//
	int UDP_Size = size + sizeof (UDP);
	//
	// Total message size.
	//
	int totalMessageSize = sizeof(DataTransfer) + size; 
	//
	// allocate space for the new message;
	//
	BYTE * messagePointer = (BYTE *) malloc(totalMessageSize);
	memset(messagePointer, 0, totalMessageSize);
	
	newMessage = (DataTransfer *)messagePointer;
	orgMessage = (DataTransfer *)transferStruct;
	//
	// clear and copy over the IPv6 and UPD data to the new message...
	//
	newMessage->_Header._Id = orgMessage->_Header._Id;
	newMessage->_Header._Origin = orgMessage->_Header._Origin;
	newMessage->_Header._Length = (UINT16)(totalMessageSize-sizeof(Header));
	
	newMessage->_Spacer = orgMessage->_Spacer;
	
	newMessage->_IPv6_Header._Header[0] = 0x60;
	newMessage->_IPv6_Header._NextHeader = 17; // This is value for the UDP header..
	newMessage->_IPv6_Header._HopLimit = orgMessage->_IPv6_Header._HopLimit;
	newMessage->_IPv6_Header._PayloadLength = (UINT16)UDP_Size;
	memcpy(newMessage->_IPv6_Header._DestinationAddress, orgMessage->_IPv6_Header._DestinationAddress, 16);
	memcpy(newMessage->_IPv6_Header._SourceAddress, orgMessage->_IPv6_Header._SourceAddress, 16);
	
	newMessage->_UDP_Message._CRC = 0xABCD;  // just put something here for wireshark..
	newMessage->_UDP_Message._SourcePort = orgMessage->_UDP_Message._SourcePort;
	newMessage->_UDP_Message._DestinationPort = orgMessage->_UDP_Message._DestinationPort;
	newMessage->_UDP_Message._Length = (UINT16)UDP_Size;
	memcpy(&newMessage->_UDP_Message._Data[0], msg, size);
	
	return newMessage;
}
/**************************************************************************************
Function Name: InitLoadSystemConfigMsg
Description:
   Initialize "LoadSystemConfig" message, define as "static" since limited the usage only in this file.
Parameter:
    _LoadSystemConfigPort:[I/O], pointer of  message
Return:
   NULL
***************************************************************************************/
static void InitLoadSystemConfigMsg(LoadSystemConfig * _LoadSystemConfigPort)
{
	_LoadSystemConfigPort->_Header._Id = 0x0C;
	_LoadSystemConfigPort->_Header._Origin = 0x80;
	_LoadSystemConfigPort->_Port = NULL;
	_LoadSystemConfigPort->_SystemConfig = NULL;
	_LoadSystemConfigPort->_G3_Config = NULL;
}
/**************************************************************************************
Function Name: MakeLoadSystemConfigMsg
Description:
   Make "LoadSystemConfig" message. define as "static" since limited the usage only in this file.
Parameter:
    config:[I], pointer of  config structure
Return:
    Result message pointer. 
***************************************************************************************/
BYTE * MakeLoadSystemConfigMsg(LoadSystemConfig * config)
{
	UINT16 totalMessageLength = sizeof(Header) + sizeof(CRC_Header);
	int tlvLength = 0;
	BYTE * payLoad = NULL;
	BYTE * msg = NULL;
	int offset = 0;
	int headerLength = 0;
	
	if (config->_Port != NULL)
	{
		config->_Port->_Type = 0x0001;
		config->_Port->_Length = 1;
		
		tlvLength += sizeof(PortDesignation);
	}
	if (config->_SystemConfig != NULL)
	{
		config->_SystemConfig->_Type = 0x0003;
		config->_SystemConfig->_Length = 26;
		
		tlvLength += sizeof(SystemConfig);
	}
	if(config->_G3_Config != NULL)
	{
		config->_G3_Config->_Type = 0x0008;
		config->_G3_Config->_Length = 16;
		config->_G3_Config->_MAC_SegmentSize = 239; // obsolete but needed for older dsp s/w
		
		tlvLength += sizeof(G3Configuration);
	}
	totalMessageLength += (UINT16)tlvLength;
	headerLength = totalMessageLength - sizeof(Header);;
	//
	// Make sure the message length is even
	//
	if ((totalMessageLength & 1) != 0)
	{
		totalMessageLength++;
	}
	msg = (BYTE *) malloc(totalMessageLength);
	if(msg!=0)
	{
			memset(msg, 0, totalMessageLength);
			
			payLoad = (BYTE *) malloc(tlvLength);
			
			if (config->_Port != NULL)
			{
				memcpy(&payLoad[offset], config->_Port, sizeof(PortDesignation));
				offset += sizeof(PortDesignation);
			}
			if (config->_SystemConfig != NULL)
			{
				memcpy(&payLoad[offset], config->_SystemConfig, sizeof(SystemConfig));
				offset += sizeof(SystemConfig);
			}
			if(config->_G3_Config != NULL)
			{
				memcpy(&payLoad[offset], config->_G3_Config, sizeof(G3Configuration));
				offset += sizeof(G3Configuration);
			}
			
			config->_Header._Length = (UINT16)headerLength;
			
			MakeCRC_Header(&config->_CRC_Header, &config->_Header, sizeof(Header), payLoad, tlvLength);
			
			offset = 0;
			memcpy(&msg[offset], config, sizeof(Header) + sizeof(CRC_Header));
			offset += sizeof(Header) + sizeof(CRC_Header);
			memcpy(&msg[offset], payLoad, tlvLength);
			
			free(payLoad);	
	}
	return msg;
}
/**************************************************************************************
Function Name: CheckInit
Description:
   Helper function to check the correctness of systemInfo, define as "static" since limited the usage only in this file.
Parameter:
    sysInfo:[I], systemInfo structure
Return:
    E_SUCCESS: no error.
    E_FAIL: has error.
***************************************************************************************/
static errorCode CheckInit(SystemInfo sysInfo)
{
    if (sysInfo._DeviceMode != 0)
	{
		return E_FAIL;
	}
    if ((sysInfo._PortAssigments & 0x03) == 0)
	{
		return E_FAIL;
	}
    if ((sysInfo._PortAssigments & 0x0C) == 0)
	{
		return E_FAIL;
	}
    BYTE longAddr[8] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    if (memcmp(sysInfo._LongAddress, longAddr, 8) == 0)
 	{
 		return E_FAIL;
 	}
    return  E_SUCCESS;
}

/**************************************************************************************
Function Name: plcWrite
Description:
   Send out the buffered data out to dedicated uart Port(Finally to power line).
Parameter:
    bytes:[I], pointer of data buffer
    length:[I], expected # of bytes to send out
Return:
    E_SUCCESS: Send successfully 
***************************************************************************************/
errorCode    plcWrite(BYTE* bytes, UINT16 length)
{
    uartWriteBuffer(UART_UPSTREAM_NO, bytes,length);
    uartSendBufferOut(UART_UPSTREAM_NO);
    return  E_SUCCESS;
}

/**************************************************************************************
Function Name: GetLoadSystemConfigMsgSize
Description:
   Get the size of  "LoadSystemConfig" message. define as "static" since limited the usage only in this file.
Parameter:
    config:[I], pointer of  config structure
Return:
    Size of  message. 
***************************************************************************************/
static int GetLoadSystemConfigMsgSize(LoadSystemConfig config)
{
    int size = sizeof(Header) + sizeof(CRC_Header);
    if (config._Port != NULL)
        {
            size += sizeof(PortDesignation);
        }
    if (config._SystemConfig != NULL)
	{
            size += sizeof(SystemConfig);
	}
    if (config._G3_Config != NULL)
	{
            size += sizeof(G3Configuration);
	}
    size += (size & 1);	
    return size;
}
/**************************************************************************************
Function Name: MakeMessage
Description:
   Helper function to generate PLC control message.
Parameter:
    message:[I], input raw message
    size:[I], input raw message size 
Return:
    Formated message according to PLC modem SPEC.
***************************************************************************************/
static BYTE* MakeMessage(void * message, int size)
{
    int payloadLength = size - sizeof(Header);
    int headerLength = sizeof(Header) + sizeof(CRC_Header); 
    Header * header = (Header *)message;
    BYTE * pMessage = (BYTE *)message;
    BYTE * msg = NULL;
    int relSize = 0;
    if((size & 1) == 1)
	{
		relSize = size+1;
	}
    else
	{
		relSize = size;
	}   
    msg = (BYTE *) malloc(relSize);
    if(msg!=0)
	{
	     memset(msg, 0, relSize);
            CRC_Header * crcHeader = (CRC_Header *)(&pMessage[sizeof(Header)]);	
            header->_Length = (UINT16) payloadLength;	
            MakeCRC_Header(crcHeader, header, sizeof(Header), & pMessage[headerLength], size-headerLength);	
            memcpy(msg, message, size);
	}
	return msg;
}
/**************************************************************************************
Function Name: SendShutDownMessage
Description:
   "soft" shutdown PLC modem.
Parameter:
    NULL
Return:
   E_SUCCESS: Success
   E_FAIL: Fail
***************************************************************************************/
static errorCode    SendShutDownMessage()
{
	errorCode res = E_SUCCESS;

	ShutDownMessage _ShutDownMessage;
	memset(&_ShutDownMessage, 0, sizeof(ShutDownMessage));
	_ShutDownMessage._Header._Id = PLC_MSG_SHUTDOWN;
	_ShutDownMessage._Header._Origin = 0x80;
	_ShutDownMessage._ResetType = PLC_RESET_SOFT;

	int size = sizeof(ShutDownMessage);
	BYTE* msg = MakeMessage(&_ShutDownMessage, size);

	if (plcWrite(msg, ((size&1)==1)?size+1:size))
	{
		res = E_FAIL;
	}

	free(msg);
    return res;
}
/**************************************************************************************
Function Name: SendGetSystemInfoRequest
Description:
    Get PLC system infomation.
Parameter:
    NULL
Return:
    E_SUCCESS: Success
    E_FAIL: Fail
***************************************************************************************/
errorCode SendGetSystemInfoRequest()
{
	errorCode res = E_SUCCESS;
	GetSystemInfoRequest _GetSystemInfoRequest;
	memset(&_GetSystemInfoRequest, 0, sizeof(GetSystemInfoRequest));
	_GetSystemInfoRequest._Header._Id = PLC_MSG_GET_SYSTEM_INFO;
	_GetSystemInfoRequest._Header._Origin = 0x80;
	
	void* message = & _GetSystemInfoRequest;
	int size = sizeof(GetSystemInfoRequest);

	BYTE * msg = MakeMessage(message, size);
 	if(plcWrite(msg, ((size&1)==1)?size+1:size))
 	{
 		res  =E_FAIL;
 	}
	free(msg);
	return res;
}
/**************************************************************************************
Function Name: SendLoadSystemConfig_DeviceMode
Description:
    Configure PLC data and diagnoistic port.
    Since it should only be called by "initPlcModem()", so deifne this API as "static".
Parameter:
    dataPort: Port # for data
    diagPort: port # for diag
Return:
    E_SUCCESS: Success
    E_FAIL: Fail
***************************************************************************************/
static errorCode SendLoadSystemConfig_Port(int dataPort, int diagPort)
{
	errorCode res = E_SUCCESS;
	LoadSystemConfig _LoadSystemConfigPort;
	InitLoadSystemConfigMsg(&_LoadSystemConfigPort);
	
	if (_LoadSystemConfigPort._Port == NULL)
	{
		_LoadSystemConfigPort._Port = (PortDesignation*) malloc(sizeof(PortDesignation));
		memset(_LoadSystemConfigPort._Port, 0, sizeof(PortDesignation));
	}
	
	//set DataPort
	_LoadSystemConfigPort._Port->_Port &= 0x0C;
	if (dataPort != 0)
	{
		_LoadSystemConfigPort._Port->_Port |= 0x01;
	}
	
	
	//set DiagPort
	_LoadSystemConfigPort._Port->_Port &= 0x03;
	if (diagPort != 0)
	{
		_LoadSystemConfigPort._Port->_Port |= 0x04;
	}
	
	
	BYTE *msg = MakeLoadSystemConfigMsg(&_LoadSystemConfigPort);
	int size  = GetLoadSystemConfigMsgSize(_LoadSystemConfigPort);
	
	if(plcWrite(msg, ((size&1)==1)?size+1:size))
	{
		res  = E_FAIL;
 	}

	free(msg);
	if (_LoadSystemConfigPort._Port != NULL)
	{
		free(_LoadSystemConfigPort._Port);
	}
	return res;
}

/**************************************************************************************
Function Name: SendLoadSystemConfig_DeviceMode
Description:
    Configure PLC device mode.
    Since it should only be called by "initPlcModem()", so deifne this API as "static".
Parameter:
    deviceMode:[I] Device mode
Return:
    E_SUCCESS: Success
    E_FAIL: Fail
***************************************************************************************/
static errorCode SendLoadSystemConfig_DeviceMode(int deviceMode)
{
	errorCode res = E_SUCCESS;
	LoadSystemConfig _LoadSystemConfigDeviceMode;
	InitLoadSystemConfigMsg(&_LoadSystemConfigDeviceMode);
	
	if (_LoadSystemConfigDeviceMode._SystemConfig == NULL)
	{
		_LoadSystemConfigDeviceMode._SystemConfig = (SystemConfig*) malloc(sizeof(SystemConfig));
		memset(_LoadSystemConfigDeviceMode._SystemConfig, 0, sizeof(SystemConfig));
	}
	
	//set DeviceMode
	_LoadSystemConfigDeviceMode._SystemConfig->_DeviceMode = deviceMode;
	
	BYTE* msg = MakeLoadSystemConfigMsg(&_LoadSystemConfigDeviceMode);
	int size = GetLoadSystemConfigMsgSize(_LoadSystemConfigDeviceMode);
	
	if(plcWrite(msg, ((size&1)==1)?size+1:size))
	{
		res  = E_FAIL;
 	}

	if (_LoadSystemConfigDeviceMode._SystemConfig != NULL)
	{
		free(_LoadSystemConfigDeviceMode._SystemConfig);
	}
	free(msg);
	return res;
}

/**************************************************************************************
Function Name: SendLoadSystemConfig_G3Config
Description:
    Configure "G3" mode paramters.
    Since it should only be called by "initPlcModem()", so deifne this API as "static".
Parameter:
    longAddress: long address
Return:
    E_SUCCESS: Success
    E_FAIL: Fail
***************************************************************************************/
static errorCode SendLoadSystemConfig_G3Config(BYTE* longAddress)
{
	errorCode res = E_SUCCESS;
	LoadSystemConfig _LoadSystemConfigG3Config;
	InitLoadSystemConfigMsg(&_LoadSystemConfigG3Config);
	
	if (_LoadSystemConfigG3Config._G3_Config == NULL)
	{
		_LoadSystemConfigG3Config._G3_Config = (G3Configuration*) malloc(sizeof(G3Configuration));
		memset(_LoadSystemConfigG3Config._G3_Config, 0, sizeof(G3Configuration));
	}
	
	//set longAddress
	memcpy(_LoadSystemConfigG3Config._G3_Config->_LongAddress, longAddress, 8);
	
	BYTE * msg = MakeLoadSystemConfigMsg(&_LoadSystemConfigG3Config);
	int size = GetLoadSystemConfigMsgSize(_LoadSystemConfigG3Config);

	if(plcWrite(msg, ((size&1)==1)?size+1:size))
	{
		res  = E_FAIL;
 	}
	
	if (_LoadSystemConfigG3Config._G3_Config != NULL)
	{
		free(_LoadSystemConfigG3Config._G3_Config);
	}
	free(msg);
	return res;
}
/**************************************************************************************
Function Name: SendAttachRequest
Description:
    Send "attach" reuest to PLC  modem to try to join the network.
    Since it should only be called by "initPlcModem()", so deifne this API as "static".
Parameter:
    NULL
Return:
    E_SUCCESS: Success
    E_FAIL: Fail
***************************************************************************************/
static errorCode SendAttachRequest()
{
	errorCode res = E_SUCCESS;
	AttachRequest _AttachRequest;
	memset(&_AttachRequest, 0, sizeof(AttachRequest));
	_AttachRequest._Header._Id = PLC_MSG_ATTACH;
	_AttachRequest._Header._Origin = 0x80;
	 
	_AttachRequest._LBA_Address = _LBA_Address;
	_AttachRequest._PAN_Id = _PAN_Id;
	
	int size = sizeof(AttachRequest);
	BYTE* msg = MakeMessage(&_AttachRequest, size);

	if (plcWrite(msg, ((size&1)==1)?size+1:size))
	{
		res = E_FAIL;
	}
	free(msg);

	return res;
}

/**************************************************************************************
Function Name: SendDetachRequest
Description:
    Send "attach" reuest to PLC  modem to try to join the network.
    Since it should only be called by "initPlcModem()", so deifne this API as "static".
Parameter:
    longAddr: long address.
Return:
    E_SUCCESS: Success
    E_FAIL: Fail
***************************************************************************************/
static errorCode SendDetachRequest(BYTE * longAddr)
{
	errorCode res = E_SUCCESS;
	DetachRequest _DetachRequest;
	memset(&_DetachRequest, 0, sizeof(DetachRequest));
	_DetachRequest._Header._Id = PLC_MSG_DETACH;
	_DetachRequest._Header._Origin = 0x80;
	
	memcpy(_DetachRequest._ExtendedAddress, longAddr, 8);
	
	int size = sizeof(DetachRequest);
	BYTE* msg = MakeMessage(&_DetachRequest, size);
	res = plcWrite(msg, ((size&1)==1)?size+1:size);

	if(res)
	{
		printf("SendDetachRequest:Fail\r\n");
		res = E_FAIL;
	}
	free(msg);
	return res;
}

/**************************************************************************************
Function Name: SendSetTxInfo
Description:
    Setup Tx Modulation mode and ToneMask for PLC  modem.
    Since it should only be called by "initPlcModem()", so deifne this API as "static".
Parameter:
    NULL
Return:
    E_SUCCESS: Success
    E_FAIL: Fail
***************************************************************************************/
static errorCode SendSetTxInfo()
{
	errorCode res = E_SUCCESS;
  
	SetInfo_TX _SetInfo_TX;
	memset(&_SetInfo_TX, 0, sizeof(SetInfo_TX));

	_SetInfo_TX._Header._Id = PLC_MSG_SET_INFO;
	_SetInfo_TX._Header._Origin = 0x80;
	_SetInfo_TX._InfoType = 0x0002;
	
	_SetInfo_TX._InfoLength = 18;
	_SetInfo_TX._TX_Level = 32;
	memcpy(_SetInfo_TX._ToneMask, ToneMasks[PLC_MASK_CENELEC_A_36], sizeof(_SetInfo_TX._ToneMask));

	_SetInfo_TX._Flags &= 0xFE;

	_SetInfo_TX._Flags &= 0xF7;

	_SetInfo_TX._Modulation = TX_Modulation;

	_SetInfo_TX._Flags &= 0xBF;
	
	void* message = &_SetInfo_TX;
	int size = sizeof(SetInfo_TX);

	BYTE * msg = MakeMessage(message, size);
	
	if(plcWrite(msg, ((size&1)==1)?size+1:size))
	{
		res = E_FAIL;
	}
	free(msg);
	return res;
}

/**************************************************************************************
Function Name: SendSetRxInfo
Description:
    Setup Rx Modulation mode and ToneMask for PLC  modem.
    Since it should only be called by "initPlcModem()", so deifne this API as "static".
Parameter:
    NULL
Return:
    E_SUCCESS: Success
    E_FAIL: Fail
***************************************************************************************/
static errorCode SendSetRxInfo()
{
	errorCode res = E_SUCCESS;
	SetInfo_RX _SetInfo_RX;
	memset(&_SetInfo_RX, 0, sizeof(SetInfo_RX));
	
	_SetInfo_RX._Header._Id = PLC_MSG_SET_INFO;
	_SetInfo_RX._Header._Origin = 0x80;
	_SetInfo_RX._InfoType = 0x0003;	
	_SetInfo_RX._InfoLength = 18;

	_SetInfo_RX._Flags &= 0x07;
	
	_SetInfo_RX._Flags &= 0x0E;
	_SetInfo_RX._Flags |= 0x01;

	_SetInfo_RX._Flags &= 0x0D;
	_SetInfo_RX._Flags |= 0x02;
	
	_SetInfo_RX._GainValue=0;//
  
	_SetInfo_RX._Flags &= 0xBF;

	memcpy(_SetInfo_RX._ToneMask, ToneMasks[PLC_MASK_CENELEC_A_36], sizeof(_SetInfo_RX._ToneMask));

	void* message = &_SetInfo_RX;
	int size = sizeof(SetInfo_RX);
	
	BYTE * msg = MakeMessage(message, size);
	
	if(plcWrite(msg, ((size&1)==1)?size+1:size))
	{
		res = E_FAIL;
	}
	free(msg);

	return res;
}

/**************************************************************************************
Function Name: createPLCDataPacket
Description:
    Input applicaiton packet and generate PLC data transfer packet. the output use the static buffer, so, this funciton
is not thread safe!!!!
Parameter:
    srcBuf:[I], applicatin packet
    srcLen:[I] srcBuf data length
    dstBuf:[I/O] Pointer to the target message buffer
    dstLen:[I/O], Generated packet length.
Return:
    E_SUCCESS: Success
    E_INPUT_PARA: Input parameter is/are wrong.
    E_FAIL: Fail
***************************************************************************************/
errorCode  createPLCDataPacket( BYTE* srcBuf, UINT16 srcLen, BYTE* dstBuf, UINT16* dstLen)
{
    errorCode res = E_SUCCESS;
    u8  pBuf;
    
    if(NULL == srcBuf || dstBuf == NULL || NULL == dstLen)
        {
            res = E_INPUT_PARA;
        }
    else
        {
            DataTransferRequest _DataTransferRequest;
            memset(&_DataTransferRequest, 0, sizeof(DataTransferRequest));
            static BYTE _NSDU = 0;
            _DataTransferRequest._Header._Id = PLC_MSG_DATA_TRANSFER;
            _DataTransferRequest._Header._Length = 4 /* _CRC */ + sizeof(IPv6) + 8; /* min udp size */
            _DataTransferRequest._Header._Origin = 0x80;
            _DataTransferRequest._NSDU_Handle = _NSDU++;
            _DataTransferRequest._UDP_Message._Length = 8;				/* min udp size */
            _DataTransferRequest._IPv6_Header._PayloadLength = 8;	/* min udp size */
            _DataTransferRequest._IPv6_Header._HopLimit = 8;
            memcpy(_DataTransferRequest._IPv6_Header._SourceAddress, genIpv6Addr, 16);            
            
            DataTransferRequest* _pDataTransferRequest = (DataTransferRequest*)FormDataTransferMessage(&_DataTransferRequest, srcBuf, srcLen);
            CalculateCRC(_pDataTransferRequest);
            int realDataLen = _pDataTransferRequest->_Header._Length + 4;
            int padLen = realDataLen;
            if ((realDataLen&1)==1)
                {
                    padLen++;
                }
            if (*dstLen >= padLen)
                {
                    *dstLen = padLen;
                    SwapBytes(_pDataTransferRequest);
                    BYTE* msg = MakeMessage(_pDataTransferRequest, realDataLen);
                    memcpy(dstBuf, msg, *dstLen);
                    SwapBytes(_pDataTransferRequest);
                    free(msg);
                }
            else
                {
                    res = E_FAIL;
                }
            free(_pDataTransferRequest);
        }
    return res;
}

/**************************************************************************************
Funcion Name: plcMsgRetriever
Description:
    This function intends to retrieve a message(push into pDes->pMsg) from plc receive buffer.
Parameter:
    pDes: [I/O], pointer of receiver descriptor
Return:
   PLCMessageRcvState:  return the state based on the uart input buffer and parsing status.
   while return "PLC_MSG_RCV_COMPLETE", a correct mesage was put into pDes->pMsg buffer.
**************************************************************************************/
PLCMessageRcvState  plcMsgRetriever(PLCMessageRCVDescriptor *pDes)
{
    unsigned short availBytes = 0;  // The # bytes of available data in uart input buffer
    unsigned short hL=0;        // header size
    unsigned short pL=0;        // payLoad size
    unsigned short crcHeader = 0;   // For calculating header CRC
    
    hL= sizeof(Header)+sizeof(CRC_Header);
        
    availBytes = uartGetAvailBufferedDataNum(UART_UPSTREAM_NO);
    
    switch(pDes->rcvState)
    {
        case    PLC_MSG_RCV_EMPTY:
            {
                if(availBytes >=hL)
                    {
                        uartRead(UART_UPSTREAM_NO, (unsigned char *)&pDes->head, sizeof(Header));
                        uartRead(UART_UPSTREAM_NO, (unsigned char *)&pDes->ccHead,sizeof(CRC_Header));

                        crcHeader = CRC16_BlockChecksum(&pDes->head,4);

                        // Judge by checking known pattern of messager and header CRC
                        if(((pDes->head._Id >= PLC_MSG_DATA_TRANSFER ) &&(pDes->head._Id < PLC_MSG_MAX)) &&
                             ((pDes->head._Length < 1024) && (pDes->head._Length >=4)) && (pDes->ccHead._HeaderCRC == crcHeader))
                            {
                                short  rL = availBytes - hL;
                                short  rN = pDes->head._Length-4;
                                if((rN & 1) == 1) rN++;
                                if( rL >= rN)   // Payload arrived
                                    {                                       
                                        uartRead(UART_UPSTREAM_NO,pDes->pMsg,rN);

                                        // Passed the checking, pMsg contians the payload.
                                        pDes->rcvState = PLC_MSG_RCV_COMPLETE;
                                    }
                                else    // Payload is not totally arrived
                                    {
                                         pDes->rcvState = PLC_MSG_RCV_POSTHEAD;
                                         pDes->count = 0;                   // Avoid stuck in "POSTHEAD" state
                                    }
                            }
                        else    // Header seems arrived, but not correct
                            {
                                pDes->rcvState = PLC_MSG_RCV_PREHEAD;
                            }                        
                    }
            }
        break;

        // We already got a header+crcheader, but not correct, most probabbly garbage data involved as per experiment, or data corrupted.
        // So, we need read byte by byte and shift left byte by byte to look for the correct header first.
        
        case PLC_MSG_RCV_PREHEAD:
            {
                int remain=availBytes;
                unsigned char t;
                unsigned char *pH,*pC;
                int k=0;
				int used=0;
				
                while(remain > 0)
                    {
                        uartRead(UART_UPSTREAM_NO,&t,1);
						used++;
                        pH=(unsigned char *)&pDes->head;
                        pC=(unsigned char *)&pDes->ccHead;
                        
                        // Sequentially shift left one byte out.
                        for(k=0;k<sizeof(Header)-1;k++)        
                            *(pH+k) = *(pH+k+1);

                        *(pH+k) = *pC;

                        for(k=0;k<sizeof(CRC_Header)-1;k++)
                         *(pC+k) = *(pC+k+1); 

                        *(pC+k) = t;

                        crcHeader = CRC16_BlockChecksum(&pDes->head,4);
                        // Try to check header
                        if(((pDes->head._Id >= PLC_MSG_DATA_TRANSFER ) &&(pDes->head._Id < PLC_MSG_MAX)) &&
                             ((pDes->head._Length < 1024) && (pDes->head._Length >=4)) && (pDes->ccHead._HeaderCRC==crcHeader))
                            {
                                short  rL= availBytes - used;
                                short  rN = pDes->head._Length-4;
                                if((rN & 1) == 1) rN++;
                                
                                if( rL >= rN)
                                    {
                                        uartRead(UART_UPSTREAM_NO,pDes->pMsg,rN);

                                        // Passed the checking, pMsg contians the payload.
                                        pDes->rcvState = PLC_MSG_RCV_COMPLETE;
                                    }
                                else
                                    {
                                         pDes->rcvState = PLC_MSG_RCV_POSTHEAD;
                                         pDes->count = 0;                   // Avoid stuck in "POSTHEAD" state                                         
                                    }
                            }
                        else
                            {
                                pDes->rcvState = PLC_MSG_RCV_PREHEAD;
                            }
                        
                        remain --;      
                    }                
            }
        break;
        case PLC_MSG_RCV_POSTHEAD:
            {
                short rN = pDes->head._Length-4;
                pDes->count++;
                if(pDes->count < PLC_MESSAGE_PAYLOAD_LATE_COUNT)
                    {
                        if((rN & 1) == 1) rN++;
                        if(availBytes >= rN)
                            {
                                uartRead(UART_UPSTREAM_NO, pDes->pMsg, rN);
                                pDes->rcvState = PLC_MSG_RCV_COMPLETE;
                            }
                    }
                else
                    {
                        // We already got header, but wait here for as long as 10 times, payload still not arrived, have to drop it.
                        printf("!!!!plcMsgRetriever: Plc message broke, drop it\r\n");
                        pDes->count = 0;
                        pDes->rcvState = PLC_MSG_RCV_EMPTY;
                    }
            }
        break;

        case PLC_MSG_RCV_COMPLETE:
                // should not come here since this state the final state, handle it same as "default".
        default:
            {
                printf("plcMsgRetriever: Wrong PLC buffer state:%d\r\n",pDes->rcvState);
            }    
    }    
   return pDes->rcvState; 
}


void    plcMessagerTask()
{
    errorCode res = E_SUCCESS;
    PLCMessagerStateManager *pPlcMsgStateMgr = &plcMsgStateMgr;
    PLCMessageRCVDescriptor *pDes=(PLCMessageRCVDescriptor *)&pPlcMsgStateMgr->rcvDesriptor;
    static u16 reconnect_cnt=0;
    char SNRchar[20];
	
    switch  (pPlcMsgStateMgr->chState)
    {
            case PLC_MSG_STATE_PWROFF:
			{
				
				 reconnect_cnt++;
                 PLCResetCnt++;
//					if(reconnect_cnt>20)
//						NVIC_SystemReset();
				
				printf("plcMessagerTask:Power OFF \r\n");
				Uart5_Pin_Disable();//disable uart5 pin 
				PLC_PWR_OFF;
				pPlcMsgStateMgr->chState = PLC_MSG_STATE_PWROFF_DELAY;
				pPlcMsgStateMgr->rcvDesriptor.rcvState = PLC_MSG_RCV_EMPTY;
				pPlcMsgStateMgr->waitCount = 0; 
			}
            break;

            case PLC_MSG_STATE_PWROFF_DELAY:
			{
				pPlcMsgStateMgr->waitCount++;
				if(pPlcMsgStateMgr->waitCount >= plcChStateDuration[PLC_MSG_STATE_PWROFF_DELAY])
				{
					pPlcMsgStateMgr->chState = PLC_MSG_STATE_PWRON;
					pPlcMsgStateMgr->rcvDesriptor.rcvState = PLC_MSG_RCV_EMPTY;
					pPlcMsgStateMgr->waitCount = 0; 
				}
			}
            break;


            case PLC_MSG_STATE_PWRON:
			{
				printf("plcMessagerTask:Power ON \r\n");  
				
				PLC_PWR_ON;
				bsp_InitUart5();                    
				pPlcMsgStateMgr->chState = PLC_MSG_STATE_PWRON_DELAY;
				pPlcMsgStateMgr->rcvDesriptor.rcvState = PLC_MSG_RCV_EMPTY;
				pPlcMsgStateMgr->waitCount = 0; 
				
			}
            break;

            case PLC_MSG_STATE_PWRON_DELAY:
			{
				pPlcMsgStateMgr->waitCount++;
				if(pPlcMsgStateMgr->waitCount >= plcChStateDuration[PLC_MSG_STATE_PWRON_DELAY])
					{
						pPlcMsgStateMgr->chState = PLC_MSG_STATE_RESET;
						pPlcMsgStateMgr->rcvDesriptor.rcvState = PLC_MSG_RCV_EMPTY;
						pPlcMsgStateMgr->waitCount = 0;
					}    
			}
            break;
            
            case PLC_MSG_STATE_RESET:
			{
				pPlcMsgStateMgr->waitCount++;
				res = SendGetSystemInfoRequest();
				printf("plcMessagerTask:Request Info\r\n");
				if(res == E_SUCCESS)
					{
						pPlcMsgStateMgr->chState = PLC_MSG_STATE_GETSYSTEMINFO;
						pPlcMsgStateMgr->rcvDesriptor.rcvState = PLC_MSG_RCV_EMPTY;
						pPlcMsgStateMgr->waitCount = 0;
					
					}
				else
					{
						pPlcMsgStateMgr->chState = PLC_MSG_STATE_PWROFF;
						pPlcMsgStateMgr->rcvDesriptor.rcvState = PLC_MSG_RCV_EMPTY;
						pPlcMsgStateMgr->waitCount = 0;
				
					}
			}   
            break;

            case PLC_MSG_STATE_GETSYSTEMINFO:
			{
				pPlcMsgStateMgr->waitCount++;
				if((plcMsgRetriever(pDes) == PLC_MSG_RCV_COMPLETE) && (pDes->head._Id == PLC_MSG_GET_SYSTEM_INFO))
				{
					SystemInfo sysInfo;
					memset(&sysInfo,0,sizeof(SystemInfo));
					memcpy(&sysInfo,&pDes,sizeof(SystemInfo));
					printf("plcMessagerTask: SysInfo OK\r\n");
					
					pPlcMsgStateMgr->retryCount=0;
					
					res = SendLoadSystemConfig_Port(PLC_PORT_SCI_B, PLC_PORT_SCI_B);
					if(res == E_SUCCESS)
						{
							pDes->rcvState = PLC_MSG_RCV_EMPTY;
							pPlcMsgStateMgr->chState = PLC_MSG_STATE_LOADSYSTEMCFG_PORT;
							pPlcMsgStateMgr->waitCount = 0;
						}
					else
						{
							pDes->rcvState = PLC_MSG_RCV_EMPTY;
							pPlcMsgStateMgr->waitCount = 0;
							pPlcMsgStateMgr->chState = PLC_MSG_STATE_PWROFF;
						}
					
				}
				else
				{
					if(pPlcMsgStateMgr->waitCount >= plcChStateDuration[pPlcMsgStateMgr->chState])
					{
						pDes->rcvState = PLC_MSG_RCV_EMPTY;
					    pPlcMsgStateMgr->waitCount = 0;
						pPlcMsgStateMgr->retryCount++;
						pPlcMsgStateMgr->chState = PLC_MSG_STATE_RESET;

						if(pPlcMsgStateMgr->retryCount > 5)
						{
							pDes->rcvState = PLC_MSG_RCV_EMPTY;
							pPlcMsgStateMgr->waitCount = 0;
							pPlcMsgStateMgr->retryCount=0;
							pPlcMsgStateMgr->chState = PLC_MSG_STATE_PWROFF;
														  
						}						
						
					}
					
					
				}
			}
            break;
                
            case PLC_MSG_STATE_LOADSYSTEMCFG_PORT:
                    {
                        pPlcMsgStateMgr->waitCount++;
                        if(plcMsgRetriever(pDes) == PLC_MSG_RCV_COMPLETE)
                            {
                                StatusMessage statMsg;
                                memset(&statMsg,0,sizeof(StatusMessage));
                                memcpy(&statMsg,&pDes->head,sizeof(StatusMessage));
                                if(statMsg._Status == 0)
                                    {
                                        printf("plcMessagerTask: Port OK\r\n");                                    
                                        res= SendLoadSystemConfig_DeviceMode(0);
                                        if(res == E_SUCCESS)
                                            {
                                                pDes->rcvState = PLC_MSG_RCV_EMPTY;
                                                pPlcMsgStateMgr->waitCount = 0;
                                                pPlcMsgStateMgr->chState = PLC_MSG_STATE_LOADSYSCFG_DEVICE; 
                                            }
                                    }
                                else
                                    {
                                        pDes->rcvState = PLC_MSG_RCV_EMPTY;
                                        pPlcMsgStateMgr->waitCount = 0;
                                        pPlcMsgStateMgr->chState = PLC_MSG_STATE_PWROFF;     
                                    }
                            }
                        else
                            {
                                if(pPlcMsgStateMgr->waitCount >= plcChStateDuration[pPlcMsgStateMgr->chState])
                                    {
                                        pDes->rcvState = PLC_MSG_RCV_EMPTY;
                                        pPlcMsgStateMgr->waitCount = 0;
                                        pPlcMsgStateMgr->chState = PLC_MSG_STATE_PWROFF;
                                    }    
                            }
                    }
            break;
                
            case PLC_MSG_STATE_LOADSYSCFG_DEVICE:
                    {
                         pPlcMsgStateMgr->waitCount++;
    
                         if(plcMsgRetriever(pDes) == PLC_MSG_RCV_COMPLETE)
                            {
                                StatusMessage statMsg;
                                memset(&statMsg,0,sizeof(StatusMessage));
                                memcpy(&statMsg,&pDes->head,sizeof(StatusMessage));
                                if(statMsg._Status == 0)
                                    {
                                        printf("plcMessagerTask: Mode OK\r\n");
                                        res= SendLoadSystemConfig_G3Config(getBoardSerialID());
                                        if(res == E_SUCCESS)
                                            {
                                                pDes->rcvState = PLC_MSG_RCV_EMPTY;
                                                pPlcMsgStateMgr->waitCount = 0;
                                                pPlcMsgStateMgr->chState = PLC_MSG_STATE_LOADSYSCFG_G3; 
                                            }
                                    }
                                else
                                    {
                                        pDes->rcvState = PLC_MSG_RCV_EMPTY;
                                        pPlcMsgStateMgr->waitCount = 0;
                                        pPlcMsgStateMgr->chState = PLC_MSG_STATE_PWROFF;     
                                    }
                            }
                        else
                            {
                                if(pPlcMsgStateMgr->waitCount >= plcChStateDuration[pPlcMsgStateMgr->chState])
                                    {
                                        pDes->rcvState = PLC_MSG_RCV_EMPTY;
                                        pPlcMsgStateMgr->waitCount = 0;
                                        pPlcMsgStateMgr->chState = PLC_MSG_STATE_PWROFF;
                                    }    
                            }    
                    }
            break;
                
            case PLC_MSG_STATE_LOADSYSCFG_G3:
                    {
                         pPlcMsgStateMgr->waitCount++;

                         if(plcMsgRetriever(pDes) == PLC_MSG_RCV_COMPLETE)
                            {
                                StatusMessage statMsg;
                                memset(&statMsg,0,sizeof(StatusMessage));
                                memcpy(&statMsg,&pDes->head,sizeof(StatusMessage));
                                if(statMsg._Status == 0)
                                    {
                                        printf("plcMessagerTask: G3 OK\r\n");
                                        
                                        res= SendShutDownMessage();
                                        if(res == E_SUCCESS)
                                            {
                                                pDes->rcvState = PLC_MSG_RCV_EMPTY;
                                                pPlcMsgStateMgr->waitCount = 0;
                                                pPlcMsgStateMgr->chState = PLC_MSG_STATE_SHUTDOWN; 
                                            }
                                    }
                                else
                                    {
                                        pDes->rcvState = PLC_MSG_RCV_EMPTY;
                                        pPlcMsgStateMgr->waitCount = 0;
                                        pPlcMsgStateMgr->chState = PLC_MSG_STATE_PWROFF;     
                                    }
                            }
                        else
                            {
                                if(pPlcMsgStateMgr->waitCount >= plcChStateDuration[pPlcMsgStateMgr->chState])
                                    {
                                        pDes->rcvState = PLC_MSG_RCV_EMPTY;
                                        pPlcMsgStateMgr->waitCount = 0;
                                        pPlcMsgStateMgr->chState = PLC_MSG_STATE_PWROFF;
                                    }    
                            }    
                    }

            break;
            
            case PLC_MSG_STATE_SHUTDOWN:
                    {
                        pPlcMsgStateMgr->waitCount++;
                        if(plcMsgRetriever(pDes) == PLC_MSG_RCV_COMPLETE)
                            {
                                StatusMessage statMsg;
                                memset(&statMsg,0,sizeof(StatusMessage));
                                memcpy(&statMsg,&pDes->head,sizeof(StatusMessage));
                                if(statMsg._Status == 0)
                                    {
                                        printf("plcMessagerTask: ShutDown OK\r\n");
                                        pDes->rcvState = PLC_MSG_RCV_EMPTY;
                                        pPlcMsgStateMgr->waitCount = 0;
                                        pPlcMsgStateMgr->chState = PLC_MSG_STATE_POSTSHUTDOWN;                                             
                                    }
                                else
                                    {
                                        pDes->rcvState = PLC_MSG_RCV_EMPTY;
                                        pPlcMsgStateMgr->waitCount = 0;
                                        pPlcMsgStateMgr->chState = PLC_MSG_STATE_PWROFF;     
                                    }
                            }
                        else
                            {
                                if(pPlcMsgStateMgr->waitCount >= plcChStateDuration[pPlcMsgStateMgr->chState])
                                    {
                                        pDes->rcvState = PLC_MSG_RCV_EMPTY;
                                        pPlcMsgStateMgr->waitCount = 0;
                                        pPlcMsgStateMgr->chState = PLC_MSG_STATE_PWROFF;
                                    }    
                            }    
                    }               
            break;

            case PLC_MSG_STATE_POSTSHUTDOWN:
                    {
                        pPlcMsgStateMgr->waitCount++;
                        if(pPlcMsgStateMgr->waitCount >=15)
                            {
                                res = SendSetTxInfo();
                                if(res == E_SUCCESS)
                                    {
                                        pDes->rcvState = PLC_MSG_RCV_EMPTY;
                                        pPlcMsgStateMgr->waitCount = 0;
                                        pPlcMsgStateMgr->chState = PLC_MSG_STATE_TX_INFO;                                             
    
                                    }
                                else
                                    {
                                        pDes->rcvState = PLC_MSG_RCV_EMPTY;
                                        pPlcMsgStateMgr->waitCount = 0;
                                        pPlcMsgStateMgr->chState = PLC_MSG_STATE_PWROFF;     

                                    }
                            }
                    }
            break;
            
            case PLC_MSG_STATE_TX_INFO:
                    {
                        pPlcMsgStateMgr->waitCount++;
                         if(plcMsgRetriever(pDes) == PLC_MSG_RCV_COMPLETE)
                            {
                                StatusMessage statMsg;
                                memset(&statMsg,0,sizeof(StatusMessage));
                                memcpy(&statMsg,&pDes->head,sizeof(StatusMessage));
                                if(statMsg._Status == 0)
                                    {
                                        printf("plcMessagerTask: TX OK\r\n");
                                         res= SendSetRxInfo();
                                        if(res == E_SUCCESS)
                                            {
                                                pDes->rcvState = PLC_MSG_RCV_EMPTY;
                                                pPlcMsgStateMgr->waitCount = 0;
                                                pPlcMsgStateMgr->chState = PLC_MSG_STATE_RX_INFO; 
                                            }
                                    }
                                else
                                    {
                                        pDes->rcvState = PLC_MSG_RCV_EMPTY;
                                        pPlcMsgStateMgr->chState = PLC_MSG_STATE_PWROFF;     
                                        pPlcMsgStateMgr->waitCount = 0;
                                    }
                            }
                        else
                            {
                                if(pPlcMsgStateMgr->waitCount >= plcChStateDuration[pPlcMsgStateMgr->chState])
                                    {
                                        pDes->rcvState = PLC_MSG_RCV_EMPTY;
                                        pPlcMsgStateMgr->waitCount = 0;
                                        pPlcMsgStateMgr->chState = PLC_MSG_STATE_PWROFF;
                                    }    
                            }    
                    }
            break;
              
            case PLC_MSG_STATE_RX_INFO:
			{
				pPlcMsgStateMgr->waitCount++;
				 if(plcMsgRetriever(pDes) == PLC_MSG_RCV_COMPLETE)
				{
					StatusMessage statMsg;
					memset(&statMsg,0,sizeof(StatusMessage));
					memcpy(&statMsg,&pDes->head,sizeof(StatusMessage));
					if(statMsg._Status == 0)
						{                                        
							printf("plcMessagerTask: RX OK\r\n");
							res= SendAttachRequest();
							if(res == E_SUCCESS)
								{
									pDes->rcvState = PLC_MSG_RCV_EMPTY;
									pPlcMsgStateMgr->waitCount = 0;
									pPlcMsgStateMgr->chState = PLC_MSG_STATE_ATTACH; 
								}
						}
					else
						{
							pDes->rcvState = PLC_MSG_RCV_EMPTY;
							pPlcMsgStateMgr->waitCount = 0;
							pPlcMsgStateMgr->chState = PLC_MSG_STATE_PWROFF;     
						}
				}
				else
				{
					if(pPlcMsgStateMgr->waitCount >= plcChStateDuration[pPlcMsgStateMgr->chState])
					{
						pDes->rcvState = PLC_MSG_RCV_EMPTY;
						pPlcMsgStateMgr->waitCount = 0;
						pPlcMsgStateMgr->chState = PLC_MSG_STATE_PWROFF;
					}    
				}    
			}
            break;
                
            case PLC_MSG_STATE_ATTACH:
                {
                        pPlcMsgStateMgr->waitCount++;
                        if(plcMsgRetriever(pDes) == PLC_MSG_RCV_COMPLETE)
						{
							AttachConfirm attachConfirm;
							memset((void*)&attachConfirm, 0, sizeof(AttachConfirm));
							memcpy((void*)&attachConfirm,&pDes->head, sizeof(AttachConfirm));
							if (attachConfirm._Status == 0x0000)
							{
								_PAN_Id = attachConfirm._PAN_Id;
								_LBA_Address = attachConfirm._NetworkAddress;
								genIpv6Addr[8] = _PAN_Id / 256;
								genIpv6Addr[9] = _PAN_Id % 256;
								genIpv6Addr[14] = _LBA_Address / 256;
								genIpv6Addr[15] = _LBA_Address % 256;
								
								printf("plcMessagerTask:Attach OK\r\n");

								pDes->rcvState = PLC_MSG_RCV_EMPTY;
								pPlcMsgStateMgr->waitCount = 0;
								pPlcMsgStateMgr->chState = PLC_MSG_STATE_NETWORK;     
							   //pPlcMsgStateMgr->chState = PLC_MSG_STATE_PWROFF; 
							}
							else
							{
								printf("plcMessagerTask: Attach Fails\r\n");
								pDes->rcvState = PLC_MSG_RCV_EMPTY;
								pPlcMsgStateMgr->waitCount = 0;
								pPlcMsgStateMgr->chState = PLC_MSG_STATE_PWROFF; 
							}
							
						}
                        else
						{

							if(pPlcMsgStateMgr->waitCount >= plcChStateDuration[pPlcMsgStateMgr->chState])
							{
								printf("No attach repsone\r\n");
								pDes->rcvState = PLC_MSG_RCV_EMPTY;
								pPlcMsgStateMgr->waitCount = 0;
							    pPlcMsgStateMgr->chState = PLC_MSG_STATE_PWROFF;                               
							}    
					    }    
                    }
            break;

            // Normal network communicating state.  here retrieve the message, and call proper handler to process further.        
            case PLC_MSG_STATE_NETWORK:
                    {
                        reconnect_cnt=0;
						
						pPlcMsgStateMgr->waitCount++;

                        if(plcMsgRetriever(pDes) == PLC_MSG_RCV_COMPLETE)
                            {
                                if(pDes->head._Id == PLC_MSG_DATA_TRANSFER) // Here call applicaiton handler to process
                                    {
                                        if(pDes->head._Length == 8)     // Application transfer indication
                                        {
                                            ;   // Do nothing here right now    
                                        }
                                        else   // True Application got form the concentrator
                                        {
                                            DataTransferIndication*pData = (DataTransferIndication*)pDes;

                                            // Below formula from Wuzengxi/Wangwei
                                         
                                            snr=getSnr(pData->_LQI);
                                            //sprintf(SNRchar,"SNR:%.02fdB\r\n",snr);
                                            // printf(SNRchar);
                                            //PrintBuf(SNRchar);
                                            if(pAppMessageHandler !=NULL)
                                                pAppMessageHandler(pData->_UDP_Message._Data);
                                        }
                                    }
                                else
                                    {
                                        printf("plcMessagerTask: Got unknown message:%x\r\n",pDes->head._Id);
                                    } 
                                pDes->rcvState = PLC_MSG_RCV_EMPTY;
                            }
                    }   
            break;

            default:
                printf("plcConnectingTask: Wrong State:%d\r\n",pPlcMsgStateMgr->chState);
                break;
        }
}


PLCChannelState plcMessagerGetChannelState()
{
    return plcMsgStateMgr.chState;
}


errorCode   plcMessagerInstallAppProtocolHandler(FuncAppMessageHandler funcPtr)
{
    pAppMessageHandler = funcPtr;
    return E_SUCCESS;
}

errorCode plcMessagerReset()
{
    PLCMessagerStateManager *pPlcMsgStateMgr = &plcMsgStateMgr;

    pPlcMsgStateMgr->chState= PLC_MSG_STATE_PWROFF;
    pPlcMsgStateMgr->waitCount = 0;
    pPlcMsgStateMgr->rcvDesriptor.rcvState = PLC_MSG_RCV_EMPTY;

    _LBA_Address = 0;
    uartPortBufferReset(UART_UPSTREAM_PORT);
   // PLCResetCnt++;
    return E_SUCCESS;    
}

/*****************************************************************************************************
Function Name: plcGetModulationMode
Description:
    Get current PLC TX modulation mode.

Parameter:
    NULL
Return:
    Current TX modulation mode.
******************************************************************************************************/
PLCModulationMode plcGetModulationMode()
{
    return  TX_Modulation; 
}
/*****************************************************************************************************
Function Name: plcStoreModulationMode
Description:
    Save PLC TX modulation mode to flash dedicated section, please be noticed that the mode change doesn't take effect immediately.
next reset will retrieve this mode and take effect. Also, the opposite side need set the same mode accordingly.

Parameter:
    mode: new TX modulation mode
Return:
    E_SUCCESS: Store to flash successfully.
    E_INPUT_PAPA: Input mode is invalid..
******************************************************************************************************/
errorCode plcStoreModulationMode(PLCModulationMode mode)
{
    unsigned char pBuf[4];
  
    FlashSectionStruct *pFS = uf_RetrieveFlashSectionInfo(FLASH_SECTION_PLC_MODULATION);

    if(mode >= PLC_MODULATION_MAX)
        {
            printf("plcSetModulationMode: mode:%d is invalid\r\n",mode);
            return E_INPUT_PARA;
        }   

    uf_EraseSector(pFS->type, pFS->base); 
   
     // !!!!!! DON'T CHANGE BELOW SEQUENCE
     // 1st write contents
     uf_WriteBuffer(pFS->type, (unsigned char *)&mode, pFS->base+4, 4);
     // 2nd write magic
     uf_WriteBuffer(pFS->type, (unsigned char *)PLC_MODULATION_FLASH_MAGIC, pFS->base, 4);

    printf("plcSetModulationMode: Save mode:%d to flash successfullu\r\n",mode);
    return E_SUCCESS;
}

/*****************************************************************************************************
Function Name: plcSetModulationMode
Description:
    Retrieve PLC TX modulation mode from flash if configured before, otherwise adopts the default mode.

Parameter:
    mode: new TX modulation mode
Return:
    Always E_SUCCESS.
******************************************************************************************************/
errorCode plcRetrieveModulationMode()
{
    unsigned char pBuf[4];
    unsigned char mode;
    FlashSectionStruct *pFS = uf_RetrieveFlashSectionInfo(FLASH_SECTION_PLC_MODULATION);

    // Read out magic code from flash
    uf_ReadBuffer(pFS->type, pBuf, pFS->base, 4);
    if(!memcmp(pBuf,PLC_MODULATION_FLASH_MAGIC,4))      // Magic code is correct
    {
        uf_ReadBuffer(pFS->type, &mode, pFS->base+4, 1);
        if(mode < PLC_MODULATION_MAX)
            {
                TX_Modulation = mode;
                printf("plcRetrieveModulationMode: Successfully retrieves modulation mode:%d from flash\r\n",TX_Modulation);
                return E_SUCCESS;
            }
    }
  
    printf("plcRetrieveModulationMode: No valid modulation mode stored in flash,Adopt default mode:%d\r\n",TX_Modulation);
    
    return E_SUCCESS;
}
/************************************************************************************************************
Function Name: plcGetLatestSNR
Description:
    Get the latest PLC channel's SNR
Paramter:
    sInt:[I/O] pointer to integer of SNR
    sInt:[I/O] pointer to decimal of SNR
Return:
    Always return E_SUCCESS.
*************************************************************************************************************/
errorCode plcGetLatestSNR(float *pSNR)
{
    *pSNR = snr;
     
    return E_SUCCESS;
}

//errorCode plcGetLatestSNR(int *sInt,int *sDec)
//{
//    *sInt = snrInt;
//    *sDec = snrDec;
//    
//    return E_SUCCESS;
//}

