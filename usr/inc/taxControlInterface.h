#ifndef	_TAX_CONTROL_INTERFACE_
#define   _TAX_CONTROL_INTERFACE_


#include "uartPort.h"


// Tax Control Device related MARCO
#define	TAX_CONTROL_PREAMABLE	0xBB

#define	TAX_RESPONSE_RETURN_DISPLAY  	0
#define	TAX_RESPONSE_RETURN_PLAINTEXT	1


#define	TAX_TOTAL_ACCOUNT_MODE_ACTUAL	0
#define	TAX_TOTAL_ACCOUNT_MODE_DEBUG	1
#define	TAX_TOTAL_ACCOUNT_MODE_CHECK	2

#define   TAX_GUN_MAX_VOL_PER_SECOND        2
#define   TAX_GUN_DATA_INITIALIZE_TIME      3

// define taxControlCommand currently applied

#define	TAX_CONTROL_CMD_QUERY_INFO		0x83
#define	TAX_CONTROL_CMD_QUERY_ONCE		0x86
#define	TAX_CONTROL_CMD_QUERY_DAY		0x87
#define	TAX_CONTROL_CMD_QUERY_MONTH 	0x88
#define	TAX_CONTROL_CMD_QUERY_TOTAL 	0x89
#define   TAX_CONTROL_CMD_QUERY_NONE		0xFF

#define	TAX_CONTROL_CMD_WAIT_TIMEOUT	3
#define   TAX_CONTROL_CMD_STATE_TIMEOUT	10

#define    TAX_CONTROL_MACHINE_GUNNO_MAX			16
#define	 TAX_CONTROL_DEVICE_GUNNO_MAX			8
//#define	 GUNNO_MAX_PER_TAX_CONTROL_DEVICE     4

#define    TAX_CONTROL_GUN_MAX_SPEED        167             // unit:0.01L per second

// Tax control device history record related MARCO

#define   TAX_CONTROL_DEVICE_DXYS_DEBUG_TAX_ID                  "00000000000000000001"  // For factory only initialized device and test purpose

#define	TAX_CONTROL_DEVICE_MAX_NUM		2
#define	METERPORT_MAX_NUM		2

typedef enum  _GunWorkingState
{
    GUN_STATE_PUT=0,    // 挂枪
    GUN_STATE_PICK,      // 摘枪 
    GUN_STATE_MAX
}GunWorkingState;

typedef enum  _MeterState
{
    METER_STATE_INVALID=0,    
    METER_STATE_VALID,     
    METER_STATE_MAX
}MeterState;

typedef enum _TaxControlDevBindState
{
	TAX_CONTROL_DEVICE_INIT = 0,
	TAX_CONTROL_DEVICE_BOUND,
	TAX_CONTROL_DEVICE_UNBOUND,
	TAX_CONTROL_DEVICE_BIND_MAX
}TaxControlDevBindState;

typedef enum _TaxControlDevBindingRequest
{
    TAX_CONTROL_DEV_BINDING_REQ_TOTAL=0,
    TAX_CONTROL_DEV_BINDING_REQ_PORT0,
    TAX_CONTROL_DEV_BINDING_REQ_PORT1,
    TAX_CONTROL_DEV_BINDING_REQ_MAX
}TaxControlDevBindingRequest;

typedef enum _TaxControlDevInitLevel
{
    TAX_CONTROL_DEVICE_INIT_FACTORY,
    TAX_CONTROL_DEVICE_INIT_OFFICIAL,
    TAX_CONTROL_DEVICE_INIT_MAX
}TaxControlDevInitLevel;

typedef enum _TaxControlDevBrand
{
	TAX_CONTROL_DEVICE_KTK,
	TAX_CONTROL_DEVICE_YTSF,
	TAX_CONTROL_DEVICE_MAX
}TaxControlDevBrand;


typedef enum _TaxControlDevMSDSample_Flag
{	
	TAX_CONTROL_DEVICE_MSDSF_DONE,
	TAX_CONTROL_DEVICE_MSDSF_UNDONE,
	TAX_CONTROL_DEVICE_MSDSF_ERR,
	TAX_CONTROL_DEVICE_MSDSF_MAX
}TaxControlDevMSDSample_Flag;


#pragma pack(1)
// define the tax control device initialization/command/response structure
typedef	struct _TaxFormatedInitialInfo
{
    unsigned char	factorySerialNo[10];	// Factory serial number
    unsigned char	gunCode[2];			// gun Location Code, seems to be logical gun number for gasoline station management, but not applied
    unsigned char	taxId[20];			// Tax ID
    unsigned char	oilGrade[4];			// oil grade
    unsigned char	year[4];
    unsigned char	month[2];
    unsigned char	date[2];
    unsigned char	hour[2];
    unsigned char	minute[2];	
}TaxFormatedInitialInfo;

// Header structure for query cmd and response 
typedef struct _TaxControlHeader
{
    unsigned char preamable;		// preamable code: 0xBB
    unsigned char length;			// datagram length: 1(cmd) + 1(frameNo)+sizeof(para)+1(checkcode)
    unsigned char frameNo;		// FrameNo: Single frame: 0xFF; Multiple Frame: if(bit 8=1) { the 1st frame, bit7~0, total frameNo} else{bit7~0, the remaining frame include the current}
    unsigned char cmd;
}TaxControlHeader;


// Structure for querying initialization infomation  command and response
typedef struct _TaxInitInfoQueryCmd
{
    TaxControlHeader  header;
    unsigned char gunNo;				// based on investigation, it can be '0','1',...'255', we can always use '0'
    unsigned char	cc;	
}TaxInitInfoQueryCmd;


// TODO: Add WXZL lab's machine's support
typedef struct _TaxOfficialInitInfo   // Official Initialized tax contrl's information
{
    unsigned char factorySerialNo[10];    // UnInitialized taxcontrol device only has this value, no others.
    unsigned char gunCode[2];
    unsigned char taxCode[19];
    unsigned char cc1;
    TaxControlHeader  header2;
    unsigned char taxCode20;
    unsigned char oilGrade[4];
    unsigned char year[4];
    unsigned char month[2];
    unsigned char day[2];
    unsigned char hour[2];
    unsigned char minute[2];
    unsigned char cc2;          // The official cc for 2nd frame(applied for Beijing Lab and gasoline station)
}TaxOfficialInitInfo;

typedef struct _TaxOfficialInitInfoQueryResp
{
    TaxControlHeader  header;
    unsigned char status;
    TaxOfficialInitInfo taxOrgInitInfo;
}TaxOfficialInitInfoQueryResp;


typedef struct _TaxFactoryInitInfoQueryResp
{
    TaxControlHeader  header;
    unsigned char status;
    unsigned char factorySerialNo[10];    // UnInitialized taxcontrol device only has this value, no others.
    unsigned char cc;
}TaxFactoryInitInfoQueryResp;

// Structure for querying current gaoline transaction command and response
typedef struct _TaxOnceQueryCmd
{
    TaxControlHeader  header;
    unsigned char gunNo;
    unsigned char rtnMethod;
    unsigned char	cc;
}TaxOnceQueryCmd;

typedef struct _TaxOnceQueryResp
{
    TaxControlHeader  header;
    unsigned char status;
    unsigned char date[2];
    unsigned char hour[2];
    unsigned char minute[2];
    unsigned char volume[6];
    unsigned char amount[6];
    unsigned char price[4];
    unsigned char cc;
}TaxOnceQueryResp;

// Structure for querying daily gaoline transaction command and response
typedef struct _TaxDailyQueryCmd
{
    TaxControlHeader  header;
    unsigned char gunNo;
    unsigned char	date[2];
    unsigned char rtnMethod;
    unsigned char	cc;
}TaxDailyQueryCmd;

typedef struct _TaxDailyQueryResp
{
    TaxControlHeader  header;
    unsigned char status;
    unsigned char year[4];
    unsigned char month[2];
    unsigned char date[2];
    unsigned char volume[8];
    unsigned char amount[8];
    unsigned char price[4];
    unsigned char cc;
}TaxDailyQueryResp;

// Structure for querying monthly gaoline transaction command and response
typedef struct _TaxMonthlyQueryCmd
{
    TaxControlHeader  header;
    unsigned char gunNo;
    unsigned char year[4];
    unsigned char month[2];
    unsigned char rtnMethod;
    unsigned char cc;	
}TaxMonthlyQueryCmd;

typedef struct _TaxMonthlyQueryResp
{
    TaxControlHeader  header;
    unsigned char status;
    unsigned char year[4];
    unsigned char month[2];
    unsigned char volume[10];
    unsigned char amount[10];
    unsigned char cc;	
}TaxMonthlyQueryResp;

// Structure for querying total gaoline transaction command and response
typedef struct _TaxTotalQueryCmd
{
    TaxControlHeader  header;
    unsigned char gunNo;
    unsigned char amountMode;
    unsigned char rtnMethod;
    unsigned char	cc;	
}TaxTotalQueryCmd;

typedef struct _TaxTotalQueryResp
{
    TaxControlHeader  header;
    unsigned char status;
    unsigned char volume[12];
    unsigned char amount[12];
    unsigned char cc;		
}TaxTotalQueryResp;


typedef struct _TaxDevInitializationInfo
{
    unsigned char magic[4];
    unsigned char factorySerialNo[10];
    unsigned char gunCode[2];
    unsigned char taxCode[20];
    unsigned char oilGrade[4];
    unsigned char year[4];
    unsigned char month[2];
    unsigned char day[2];
    unsigned char hour[2];
    unsigned char minute[2];
    unsigned char cc;
}TaxDevInitializationInfo;

typedef struct _TaxDevInitializationInfoMgr
{
    TaxDevInitializationInfo	dev[TAX_CONTROL_DEVICE_MAX_NUM];
}TaxDevInitializationInfoMgr;

// Below structure used for communicaiton and storage, so, MUST NOT applying "pack" to avoid wrong size of structure.
#pragma pack()

typedef struct _TaxTotalData
{
    long long totalVolume;		// 64bit presents total volume, actually 40bit is enough(original is 12 ASCII characters)
    long long totalAmount;		// 64bit presents total amount, actually 40bit is enough(original is 12 ASCII characters)
    unsigned int  timeStamp;            // Record the timepStamp of this sample
}__attribute__((aligned(8))) TaxTotalData;


typedef struct _TaxMonthData
{
    unsigned char year[4];
    unsigned char month[2];
    unsigned char volume[10];
    unsigned char amount[10];	
}TaxMonthData;

typedef struct _TaxMonthDataManager//wawe
{
    TaxMonthData  PreMonthData;
    u8 TestMonthData[50];
    u8 GetDataLen;
    u8 MSDSameCnt;
    u8 MSDNoDataCnt;//FOR TESTR
    u8 MSDccErrCnt;   //FOR TEST
}
TaxMonthDataManager;

typedef enum _TaxControlBindingState
{
    TAX_CONTROL_BINDING_STATE_IDLE = 0,               // Intial state, send "query total" to tax control device
    TAX_CONTROL_BINDING_STATE_RETRIEVE,             // Try to retrieve tax control device's initialization from flash for fast binding
    TAX_CONTROL_BINDING_STATE_NEW,                     // new device, query device's initial information
    TAX_CONTROL_BINDING_STATE_COMPLETE,                 // 
    TAX_CONTROL_BINDING_STATE_MAX
}TaxControlBindingState;

typedef enum _TaxControlSamplingState
{
    TAX_CONTROL_SAMPLING_STATE_INIT = 0,                                // Initial state, clock is not synced yet 
    TAX_CONTROL_SAMPLING_STATE_IDLE,					//  Start a new round of query
    TAX_CONTROL_SAMPLING_STATE_PROCESSING,			//  Process the "total record info" received from tax control device
    TAX_CONTROL_SAMPLING_STATE_CONFIRM,				// Confirm price
    TAX_CONTROL_SAMPLING_STATE_BUSY,                               // Device is busy when querying, stay in this state until get info from device
    TAX_CONTROL_SAMPLING_STATE_TAXMONTH_PROCESSING,//月累计查询处理
    TAX_CONTROL_SAMPLING_STATE_FREE,//
    TAX_CONTROL_SAMPLING_STATE_MeterPortDown,
    TAX_CONTROL_SAMPLING_STATE_MAX
}TaxControlSamplingState;

typedef enum _TaxControlConfirmSubState
{
    TAX_CONTROL_CONFIRM_SUB_STATE_INIT,
    TAX_CONTROL_CONFIRM_SUB_STATE_CONFIRM,
    TAX_CONTROL_CONFIRM_SUB_STATE_CONFIRM_TWICE,//wawe
    TAX_CONTORL_CONFIRM_SUB_STATE_MAX
}TaxControlConfirmSubState;

typedef enum _TaxControlProcessSubState
{
    TAX_CONTROL_PROCESS_SUB_STATE_INIT,
    TAX_CONTROL_PROCESS_SUB_STATE_NORMAL,
    TAX_CONTROL_PROCESS_SUB_STATE_PEND,
    TAX_CONTROL_PROCESS_SUB_STATE_MAX
}TaxControlProcessSubState;

typedef struct _TaxDevGunSamplingStatistic
{
    unsigned int totalTryCount;          // Total # of start query started from Idle state
    unsigned int successCount;          // # of successfully sampled transaction.
}TaxDevGunSamplingStatistic;

typedef struct _TaxControlDevInfo
{
    TaxControlDevBindState bind;
    TaxControlDevBrand brand;
    TaxControlDevInitLevel initLevel;
    UartPortNum   port;	
    unsigned char serialID[10];
    unsigned char taxUniqueID[20];
    unsigned char gunTotal;
    unsigned char gunBaseNumber;    // Assign the gun base #
    // Notice: Above info is ready immediately after binding 
    //            Below Info use for scheduling gasoline trasaction sampling and caculating
    unsigned char gunCurrent;		// Current working gun number
    unsigned char curCmd;			// Current working command
    unsigned char stateCount;		// Count how long the Current state takes
  
    unsigned char prePrice[4]; 		        // Previous price
    TaxControlConfirmSubState confirmState;
    
    TaxControlSamplingState sampleState;	// Current working command state

    TaxControlProcessSubState processState;
    TaxTotalData preData[TAX_CONTROL_DEVICE_GUNNO_MAX];
    TaxTotalData dataStore[TAX_CONTROL_DEVICE_GUNNO_MAX];  // gun's historic data(volume,amount)

    TaxControlProcessSubState gunSubState[TAX_CONTROL_DEVICE_GUNNO_MAX];
    char gunRepeatCount;
    TaxDevGunSamplingStatistic  statistic[TAX_CONTROL_DEVICE_GUNNO_MAX];
  //  TaxMonthDataManager  MSDdata[TAX_CONTROL_DEVICE_GUNNO_MAX];//wawe gun's monthtax data    
}TaxControlDevInfo;

typedef struct _TaxControlDevManager
{
    TaxControlDevInfo  taxControlDevInfo[TAX_CONTROL_DEVICE_MAX_NUM];	// Managing maxium to 3 tax_control_device
}TaxControlDevManager;


//月累采集标识
typedef struct _TaxControlDevMSDFlagManager
{
    TaxControlDevMSDSample_Flag   MSDFlag[TAX_CONTROL_DEVICE_MAX_NUM];
    u8 Month;    
}TaxControlDevMSDFlagManager;


typedef struct _GunStatusManager
{
    GunWorkingState GunStatus[TAX_CONTROL_DEVICE_GUNNO_MAX];
    u8 AnyGunPickUp;//
    MeterState MeterValid;
    u32 GunPickCnt[TAX_CONTROL_DEVICE_GUNNO_MAX];    
}GunStatusManager;


// Public API proto definition
TaxControlDevManager * getTaxControlDevManager();
errorCode taxControlDevToPortBinding(TaxControlDevBindingRequest bReq);
errorCode   taxPortConnectingDetectAndRebindTask();
errorCode   taxPortConnectingDetectAndRebindTask_MeterMode(u8 PortCanBeBind);
errorCode   taxPortDetectAndRebindCheckTask(void);
//void taxControlSamplingTask();
//void TaxMonthSamplingTask();

errorCode   taxControlSamplingGun(unsigned char dev,unsigned char gunNo);
errorCode   taxControlSamplingDev(unsigned char dev, unsigned char *sucNum, unsigned char *gunNo );
errorCode   taxControlDeviceRecoverStateToIdle(unsigned char dev);

#endif
