/*************************************************************************************************
File: ControlledTaxSampling.h
Author: George Yan
Date: 2017-6-16
Description:
    This header file intends to unify Gpx port and meter port to support controlled tax interface sampling. all the necessary
 MACRO, structure and APIs are defined here.
**************************************************************************************************/
#ifndef _CONTROLLED_TAX_SAMPLING_H_
#define _CONTROLLED_TAX_SAMPLING_H_

#include "errorCode.h"
#include "taxControlInterface.h"

#define MAXIUM_GPX_PER_PORT  8
#define MAXIUM_PORT_SET_NUM  2
#define UNMAPPED_TAXGUNNO  100  //未映射的税控枪号
// Enumerate all the combination of tax_control_device in a machine, yes, we can only support below combinations.
typedef enum  _Sample_Mode
{   
    GUN_SIGNAL_MODE=0,
    METER_MODE,
    FREE_MODE,
    MAX_MODE
}Sample_Mode;


typedef enum _TaxDevCombineType
{
    SINGLE_TAX_SINGLE_GUN = 0,
    SINGLE_TAX_TWO_GUN, 
    SINGLE_TAX_FOUR_GUN,
    SINGLE_TAX_SIX_GUN,
    DOUBLE_TAX_TWO_GUN,
    DOUBLE_TAX_FOUR_GUN,
    DOUBLE_TAX_SIX_GUN,
    DOUBLE_TAX_MAX_GUN
}TaxDevCombineType;

typedef enum _ControlPortState
{
    CONTROL_PORT_STATE_INTIALIZE = 0,    // In this state, do the necessary initializing, then switch to map state
    CONTROL_PORT_STATE_MAPING,              // Map Gpx to tax_control_device_number, then switch to IDLE state 
    CONTROL_PORT_STATE_POLLING,          // This is normal state, do checking whether we have pended task to do   
    CONTROL_PORT_STATE_WORKING,          // In this state, need to execute all the GPx pended task
    CONTROL_PORT_STATE_PERIOD_IDLE,
    CONTROL_PORT_STATE_PERIOD_SAMPLE,
    CONTROL_PORT_STATE_MAX
}ControlPortState;

typedef enum _GpxTaskState
{
    GPX_TASK_STATE_IDLE = 0,                    // This Gpx has no task
    GPX_TASK_STATE_PEND,                        // This Gpx has pended task yet to sample
    GPX_STATE_MAXIUM
}GpxTaskState;


typedef enum _GpxPPState
{
    GPX_PP_STATE_PUTDOWN = 0,               // This Gpx curerntly in PUTDOWN state 
    GPX_PP_STATE_PICKUP,                        // This Gpx currently in PULLUP state
    GPX_PP_STATE_MAX
}GpxPPState;

typedef enum _GpxMapState
{
    GPX_MAP_NO,                                         // This GPx hasn't mapped to tax_dev_gun
    GPX_MAP_YES,                                        // This Gpx has already mapped to tax_dev_gun
    GPX_MAP_MAX        
}GpxMapState;

#pragma pack(1)

typedef struct _TAX_SAMPLE_UNLOCK
{
    u8 unlock[MAXIUM_GPX_PER_PORT]; 
    
}TAX_SAMPLE_UNLOCK;

typedef struct _GpxStruct
{
    unsigned char taxDev;                   // Map to which tax dev
    unsigned char taxGun;                   // Map to which gun
    unsigned char meterGun; 
  
    GpxMapState  mapState;               // May state: mapped or not
    GpxTaskState taskState;               // Do we have pended task on this Gpx                
    GpxPPState    ppState;                  // Record GPx current putdown/pullup state
    unsigned int  ppCount;                // Statistic of how many pickup_purdown cycles from reset
    
}GpxStruct;

typedef struct _ControlPortStruct
{
    unsigned char valid;                     // 0: invalid;   1:valid   税控port绑定成功后此valid有效
    unsigned char gPxTotal;                  // How many Gpx belong to this port(Gpx Set or meter port)
    unsigned char gPxCurrent;                // Currently sampling Gpx 
    unsigned char gPxMappedNum;              // How many Gpx in this set has been mapped
    unsigned int  pendflag;                // Bitwise:How many Gpxs are in pended state
    unsigned int  getTsdFlag;   //上电获取总累数据标志
    GpxStruct gPx[MAXIUM_GPX_PER_PORT];     //8 Maintain each Port(Gpx set ot meter port)'s Gpx info             
    ControlPortState portState;                         // This port already mapped completely or not 
    GpxPPState gPxSetState;                               // Partner Tax dev  is ready or not 
}ControlPortStruct;

typedef struct _ControlPortSetMgr
{
    unsigned char   valid;                            // 0: invalid; 1:valid //delimiter确定后此valid有效
    TaxDevCombineType type;                           // The combination type
    unsigned char  delimiter;                         // GPx offset to delimit the 1st set and 2nd set if have 2 set 
    ControlPortStruct    port[MAXIUM_PORT_SET_NUM];   // Gpx group  <-> tax control dev  
}ControlPortSetMgr;

MeterState  setMeterState(u8 meterport,MeterState state);
MeterState  getMeterState(u8 meterport);
errorCode   initControlPortSetManager();
errorCode   ControlPortEventHandler(unsigned char *pMsg);
void    controlledSamplingTask();
ControlPortSetMgr * getControlPortSetMgr();
void    meterBoardAliveMonitor();
void  controlledSamplingSyncGpxState(unsigned char port,unsigned char status);
ControlPortSetMgr *getControlPortMgr(void);
Sample_Mode getMeterBoardMode(void);
void setMeterBoardMode(Sample_Mode state);
#endif
