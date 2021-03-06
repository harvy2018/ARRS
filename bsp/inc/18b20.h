#ifndef __18b20_H__
#define __18b20_H__

typedef enum _TemperatureSampleState
{
    TEMP_SAMPLE_STATE_START = 0,
    TEMP_SAMPLE_STATE_RESULT,
    TEMP_SAMPLE_STATE_MAX
}TemperatureSampleState;

void  getBoardTemperature(unsigned char *tSymbol,unsigned char *tInt, unsigned short *tFraction);
void sampleBoardTempertureTask();
void  getBoardTemperature_F(float *t);
#endif 

