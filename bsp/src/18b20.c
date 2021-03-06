/***********************************************************************************
File Name: 18b20.c
Description:
   This file contian all the components to read the temperature.    
***********************************************************************************/
#include "stm32f10x.h"
#include "bsp_timer.h"
#include "18b20.h"

#define GPIO_PORT_DS18B20   GPIOA
#define GPIO_PIN_DS18B20	GPIO_Pin_8
#define GPIO_CLK_DS18B20	RCC_APB2Periph_GPIOA

//float temperaure;
static unsigned int a1,a2,a3,a4,a5,a6,a7,a8,a9,a10; 
static unsigned int conf[10];
static unsigned char    id[9];
static int  a[9];
static int  temperaure;   
static char Tsymbol;
unsigned char   Tem_int;
unsigned short  Tem_dec;
static TemperatureSampleState  state = TEMP_SAMPLE_STATE_START;

void DQ_CLR()   {GPIO_PORT_DS18B20->BRR&=~(0x0100); GPIO_PORT_DS18B20->BRR|=GPIO_PIN_DS18B20;}   //GPIO_PORT_DS18B20->BRR = GPIO_PIN_DS18B20
void DQ_SET()   {GPIO_PORT_DS18B20->BSRR&=~(0x0100);GPIO_PORT_DS18B20->BSRR|=GPIO_PIN_DS18B20;}     //GPIO_PORT_DS18B20->BSRR = GPIO_PIN_DS18B20


void  DQ_OUT()	{ GPIO_PORT_DS18B20->CRH &=~(0x0f);	GPIO_PORT_DS18B20->CRH|=0x03;}  //
void  DQ_IN()	{ GPIO_PORT_DS18B20->CRH &=~(0x0f);	GPIO_PORT_DS18B20->CRH|=0x04;}

unsigned char GetDQ(void)
{
	unsigned short temp;
	DQ_IN();
	temp = GPIO_PORT_DS18B20->IDR;	 //IDR  
	if((temp&0x0100)==0x0100)	//PB8
	   return 1;
	else
	   return 0;

}

static unsigned char init_1820()
{
	unsigned char presence=0;

	DQ_OUT();
	DQ_SET(); 
	DQ_CLR();           //pull DQ line low
	TIM5_US_CALC(480);  // leave it low for 480μs  
	DQ_SET();    
	TIM5_US_CALC(60);   // wait for presence
	presence = !GetDQ();    // get presence signal
	DQ_OUT();
	DQ_SET();  

	TIM5_US_CALC(140);  // wait for end of timeslot

	return(presence);   // presence signal returned
}
//***********************************
//功能：写一BYTE
//说明：

void write_1820(unsigned char data) 
{    
     unsigned char m; 
    for(m=0;m<8;m++) 
    { 
       DQ_CLR(); 
       if(data &(1<<m))    
          DQ_SET(); 
       else 
         DQ_CLR(); 
       TIM5_US_CALC(60);  
	     
       DQ_SET(); 
    } 
       DQ_SET(); 
} 

//***********************************
//功能：读一BYTE
//说明：
unsigned char read_1820() 
{     
    unsigned char temp,k,n; 
    temp=0; 
    for(n=0;n<8;n++) 
    { 
      DQ_CLR() ;
      DQ_SET();
     // in_dat;
	  	//DQ_IN;    
      k=GetDQ();    //read data
      if(k) 
        temp|=(1<<n); 
      else 
        temp&=~(1<<n);
	   
	   
      TIM5_US_CALC(40);     
     // out_dat; 
	 	DQ_OUT();
   } 
   return (temp); 
} 

void Get_id()
{    
    unsigned char i;   
    // init_1820();        //resert
     write_1820(0x33); 
    
     for(i=0;i<8;i++)
     {
	     id[i]=read_1820();
     }
}

void sampleBoardTemperatureStart()
{
    init_1820();
    write_1820(0xcc);
    write_1820(0x44);     
}


void sampleBoardTempertureResult()                   
{   
    unsigned char TL,TH;
	
    temperaure=0; 

	   
    init_1820();        //resert
    write_1820(0xcc);   // overlook 64bit address
    write_1820(0xbe);   //read memory     
      
    
    TL=read_1820();    //read temp low
    TH=read_1820();    //read temp hign
       
    a3=read_1820();    //read th
    a4=read_1820();    //read tl
    a5=read_1820();    //read config
    a6=read_1820();    //read reserved
    a7=read_1820();    //.............
    a8=read_1820();    //..............
    a9=read_1820();    //read crc


	if(TH>7)	  //12位温度数据，TL的八位和TH前四位
    {
        Tsymbol=1;//温度为负  
        temperaure=((TH<<8)|TL); 
        temperaure=~temperaure+1;
        temperaure&=0x0FFF;
        temperaure*=625;
        temperaure+=50000;//温度修正-5°
    }
	else
    {
        Tsymbol=0;//温度为正
        temperaure=((TH<<8)|TL); 
        temperaure*=625;
        temperaure-=50000;//温度修正-5°
    }                    
                                      
     a[0]=temperaure/1000000;             //100 
     a[1]=temperaure%1000000/100000;      //10  
     a[2]=temperaure/10000%100%10;        //1 
     
     a[4]=temperaure%10000/1000;          //.1
     a[5]=temperaure%1000/100;            //0.01
     a[6]=temperaure%100/10;              //0.001
    
     a[3]=a[0]*100+a[1]*10+a[2];            //int part
     a[8]=a[4]*1000+a[5]*100+a[6]*10+a[7];  //decimal fraction 
		
	 Tem_int=a[3];
	 Tem_dec=a[8];
}


void sampleBoardTempertureTask()
{
    switch(state)
        {
            case TEMP_SAMPLE_STATE_START:
                sampleBoardTemperatureStart();
                state = TEMP_SAMPLE_STATE_RESULT;
            break;

            case TEMP_SAMPLE_STATE_RESULT:
                sampleBoardTempertureResult();
                state = TEMP_SAMPLE_STATE_START;
            break;    
        }
}

void  getBoardTemperature(unsigned char *tSymbol,unsigned char *tInt, unsigned short *tFraction)
{
    *tSymbol = Tsymbol;
    *tInt = Tem_int;
    *tFraction = Tem_dec;
}


void  getBoardTemperature_F(float *t)
{
    *t=(float)temperaure/10000;
}

