/***************************************************************************************
File name: transactionHelper.c
Date: 2016-10-8
Author: Xuzibo/Wangwei/George
Description:
     This file contain all the interface to help remove the ambiguity of gun # and make 
decision whether to query price or not.
***************************************************************************************/
#include "stm32f10x.h"
#include "math.h"
#include "transactionHelper.h"
/**************************************************************************************
Function Name: taxControlPriceJudegmenet
Description:
	Make decision whether need to query price or not base on the volume and amount of the transaction.
Parameter:
      volume:[I], the volume
      amount:[I], the amount
return:
	>0: actual price
	-1: can't calculate price since existing ambiguty, need query.
More:
    This algo was first implemented by Xuzibo w/ C#, Wamgwei ported to this cpu. This implementation just
change the para and funciton name, not touch the core algo.
***************************************************************************************/
s32 taxControlPriceJudegmenet(float volume,float amount)
{
    s32 highPrice,lowPrice,midPrice;
	
    //  金额 / (油量+0.005) < 单价 <＝ 金额 / (油量-0.005) 
    //  midPrice = Convert.ToInt32(Math.Round(amount / (sum + 0.0), 2) * 100);
    //  highPrice = Convert.ToInt32(Math.Floor((amount * 100) / (sum - 0.5)));
    //  lowPrice = Convert.ToInt32(Math.Ceiling((amount * 100) / (sum + 0.5)));
    //  if (lowPrice == (amount * 100) / (sum + 0.5)) //正好能够除尽
    //  	lowPrice++;
    //  if (highPrice < lowPrice)  //单价0.01，加油6.45，金额7分，得lowPrice > highPrice
    //	lowPrice = highPrice = midPrice;
    //  if (midPrice < lowPrice || midPrice > highPrice)
    //	throw new Exception("单价计算错误：金额=" + amount + ", 油量=" + sum);

    midPrice =(s32)((amount/volume)*100+0.5);
    highPrice=floor((amount*100)/(volume-0.5));
    lowPrice=ceil((amount*100)/(volume+0.5));
    
    if(lowPrice == (amount*100)/(volume+0.5))
        lowPrice++;
    
    if (highPrice < lowPrice)  //单价0.01，加油6.45，金额7分，得lowPrice > highPrice
        {
		lowPrice = midPrice;
		highPrice = midPrice;
        }
	
    if(lowPrice!=highPrice)
        {
            return -1;
        }
    else
        {
            return lowPrice;
        }	
}

