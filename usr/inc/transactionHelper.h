/***************************************************************************************
File name: transactionHelper.h
Date: 2016-10-8
Author: Xuzibo/Wangwei/George
Description:
     This file contain all the interface definition to help remove the ambiguity of gun # and make decision whether to query 
price or not.
***************************************************************************************/
#ifndef _TRANSACTION_HELPER
#define _TRANSACTION_HELPER

s32 taxControlPriceJudegmenet(float volume,float amount);
#endif
