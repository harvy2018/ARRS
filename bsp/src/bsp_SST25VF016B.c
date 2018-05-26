/*
*********************************************************************************************************
*	                                  
*	ģ������ : ����FLASH  SST25VF016B����ģ��
*	�ļ����� : bsp_SST25VF016B.c
*	��    �� : V1.0
*	˵    �� : ʵ��SST25VF016B�Ķ�д������д��������AAIָ�����д��Ч�ʡ�
*			   ʹ��STM32��Ӳ��SPI�ӿڣ���IOģ��ʱ��
*	�޸ļ�¼ :
*		�汾��  ����       ����    ˵��
*		v1.0    2011-08-21 armfly  ST�̼���������V3.5.0�汾��
*
*	Copyright (C), 2010-2011, ���������� www.armfly.com
*
*********************************************************************************************************
*/

/*
	Ӧ��˵����CPU�ϵ������ȵ���һ�� sf_Init()����
*/

#include "stm32f10x.h"
#include "string.h"
#include "bsp_SST25VF016B.h"

/* 
	������������Flash��SPI��GPIO�˿� :
		PA5 = SCK
		PA6 = MISO
		PA7 = MOSI
		PB2 = CS
*/
#define SPI_FLASH          SPI1
#define SF_CLK             RCC_APB2Periph_SPI1
#define SF_GPIO            GPIOA
#define SF_GPIO_CLK        RCC_APB2Periph_GPIOA
#define SF_PIN_SCK         GPIO_Pin_5
#define SF_PIN_MISO        GPIO_Pin_6
#define SF_PIN_MOSI        GPIO_Pin_7

#define SF_CS             GPIO_Pin_4
#define SF_CS_GPIO        GPIOA
#define SF_CS_GPIO_CLK    RCC_APB2Periph_GPIOA

/* Ƭѡ�����õ�ѡ��  */
#define SF_CS_LOW()       GPIO_ResetBits(SF_CS_GPIO, SF_CS)

/* Ƭѡ�����ø߲�ѡ�� */
#define SF_CS_HIGH()      GPIO_SetBits(SF_CS_GPIO, SF_CS)



#define CMD_WBYTE     0x02  	/* �ֽڱ��ָ�� */
#define CMD_AAI       0xAD  	/* AAI �������ָ�� */
#define CMD_DISWR	  0x04		/* ��ֹд, �˳�AAI״̬ */
#define CMD_EWRSR	  0x50		/* ����д״̬�Ĵ��������� */
#define CMD_WRSR      0x01  	/* д״̬�Ĵ������� */
#define CMD_WREN      0x06		/* дʹ������ */
#define CMD_READ      0x03  	/* ������������ */
#define CMD_RDSR      0x05		/* ��״̬�Ĵ������� */
#define CMD_RDID      0x9F		/* ������ID���� */
#define CMD_SE        0x20		/* ������������ */
#define CMD_BE        0xC7		/* ������������ */

#define CMD_BE        0xC7		/* ������������ */

#define DUMMY_BYTE    0xA5		/* ���������Ϊ����ֵ�����ڶ����� */

#define WIP_FLAG      0x01		/* ״̬�Ĵ����е����ڱ�̱�־��WIP) */

static uint8_t sf_SendByte(uint8_t _ucValue);
static void sf_WriteEnable(void);
static void sf_WriteStatus(uint8_t _ucValue);
static void sf_WaitForWriteEnd(void);
static uint8_t sf_NeedErase(uint8_t * _ucpOldBuf, uint8_t *_ucpNewBuf, uint16_t _uiLen);
static uint8_t sf_CmpData(uint32_t _uiSrcAddr, uint8_t *_ucpTar, uint32_t _uiSize);
static uint8_t sf_AutoWritePage(uint8_t *_ucpSrc, uint32_t _uiWrAddr, uint16_t _usWrLen);

static uint8_t s_spiBuf[SF_PAGE_SIZE];	/* ����д�������ȶ�������page���޸Ļ�������������page��д */

/*
*********************************************************************************************************
*	�� �� ��: sf_InitHard
*	����˵��: ��ʼ������FlashӲ���ӿڣ�����STM32��SPIʱ�ӡ�GPIO)
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void sf_InitHard(void)
{
	SPI_InitTypeDef  SPI_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;
	
	/* ʹ�� SPI �� GPIO ʱ�� */
	RCC_APB2PeriphClockCmd(SF_CLK | SF_GPIO_CLK | SF_CS_GPIO_CLK, ENABLE);
	
	/* ����SPI����SCK��MISO �� MOSIΪ��������ģʽ */
	GPIO_InitStructure.GPIO_Pin = SF_PIN_SCK | SF_PIN_MISO | SF_PIN_MOSI;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(SF_GPIO, &GPIO_InitStructure);
	
	/* ����Ƭѡ����Ϊ�������ģʽ */
	GPIO_InitStructure.GPIO_Pin = SF_CS;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(SF_CS_GPIO, &GPIO_InitStructure);
	
	/* Ƭѡ�øߣ���ѡ�� */
	SF_CS_HIGH();
	
	/* ����SPIӲ������ */
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;	/* ���ݷ���2��ȫ˫�� */
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;		/* STM32��SPI����ģʽ ������ģʽ */
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;	/* ����λ���� �� 8λ */
	/* SPI_CPOL��SPI_CPHA���ʹ�þ���ʱ�Ӻ����ݲ��������λ��ϵ��
	   ��������: ���߿����Ǹߵ�ƽ,��2�����أ������ز�������)
	*/
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;			/* ʱ�������ز������� */
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;		/* ʱ�ӵĵ�2�����ز������� */
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;			/* Ƭѡ���Ʒ�ʽ���������� */
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;	/* ������Ԥ��Ƶϵ����4��Ƶ */
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;	/* ����λ������򣺸�λ�ȴ� */
	SPI_InitStructure.SPI_CRCPolynomial = 7;			/* CRC����ʽ�Ĵ�������λ��Ϊ7�������̲��� */
	SPI_Init(SPI_FLASH, &SPI_InitStructure);
	
	SPI_Cmd(SPI_FLASH, ENABLE);		/* ʹ��SPI  */
	
	SF_CS_LOW();				/* ������ʽ��ʹ�ܴ���FlashƬѡ */
	sf_SendByte(CMD_DISWR);		/* ���ͽ�ֹд�������,��ʹ������д���� */
	SF_CS_HIGH();				/* ������ʽ�����ܴ���FlashƬѡ */
	
	sf_WaitForWriteEnd();		/* �ȴ�����Flash�ڲ�������� */
	
	sf_WriteStatus(0);			/* �������BLOCK��д���� */
}

/*
*********************************************************************************************************
*	�� �� ��: sf_EraseSector
*	����˵��: ����ָ��������
*	��    �Σ�_uiSectorAddr : ������ַ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void sf_EraseSector(uint32_t _uiSectorAddr)
{
	sf_WriteEnable();								/* ����дʹ������ */
	
	/* ������������ */
	SF_CS_LOW();									/* ʹ��Ƭѡ */
	sf_SendByte(CMD_SE);							/* ���Ͳ������� */
	sf_SendByte((_uiSectorAddr & 0xFF0000) >> 16);	/* ����������ַ�ĸ�8bit */
	sf_SendByte((_uiSectorAddr & 0xFF00) >> 8);		/* ����������ַ�м�8bit */
	sf_SendByte(_uiSectorAddr & 0xFF);				/* ����������ַ��8bit */
	SF_CS_HIGH();									/* ����Ƭѡ */
	
	sf_WaitForWriteEnd();							/* �ȴ�����Flash�ڲ�д������� */
}

void sf_EraseBlock(uint8_t blocksize, uint32_t _uiSectorAddr)
{
	
	sf_WriteEnable();								/* ����дʹ������ */
	
	/* ������������ */
	SF_CS_LOW();									/* ʹ��Ƭѡ */
	
	if(blocksize==32)
	  sf_SendByte(0x52);	/* ����32K BLOCK�������� */
  else if(blocksize==64)
		sf_SendByte(0xD8);	/* ����64K BLOCK�������� */
	else
		sf_SendByte(0xD8);	
		
	sf_SendByte((_uiSectorAddr & 0xFF0000) >> 16);	/* ����������ַ�ĸ�8bit */
	sf_SendByte((_uiSectorAddr & 0xFF00) >> 8);		/* ����������ַ�м�8bit */
	sf_SendByte(_uiSectorAddr & 0xFF);				/* ����������ַ��8bit */
	SF_CS_HIGH();									/* ����Ƭѡ */
	
	sf_WaitForWriteEnd();							/* �ȴ�����Flash�ڲ�д������� */
}
/*
*********************************************************************************************************
*	�� �� ��: sf_EraseChip
*	����˵��: ��������оƬ
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/  
void sf_EraseChip(void)
{
	sf_WriteEnable();								/* ����дʹ������ */
	
	/* ������������ */
	SF_CS_LOW();									/* ʹ��Ƭѡ */
	sf_SendByte(CMD_BE);							/* ���Ͳ������� */
	SF_CS_HIGH();									/* ����Ƭѡ */
	
	sf_WaitForWriteEnd();							/* �ȴ�����Flash�ڲ�д������� */
}

/*
*********************************************************************************************************
*	�� �� ��: sf_PageWrite
*	����˵��: ��һ��page��д�������ֽڡ��ֽڸ������ܳ���ҳ���С��4K)
*	��    �Σ�	_pBuf : ����Դ��������
*				_uiWriteAddr ��Ŀ�������׵�ַ
*				_usSize �����ݸ��������ܳ���ҳ���С
*	�� �� ֵ: ��
*********************************************************************************************************
*/  
void sf_PageWrite(uint8_t * _pBuf, uint32_t _uiWriteAddr, uint16_t _usSize)
{
	uint32_t i;
	
	/* AAIָ��Ҫ��������ݸ�����ż�� */
	if ((_usSize < 2) && (_usSize % 2))
	{
		return ;
	}

	sf_WriteEnable();								/* ����дʹ������ */
	
	/* ������������ */
	SF_CS_LOW();									/* ʹ��Ƭѡ */
	sf_SendByte(CMD_AAI);							/* ����AAI����(��ַ�Զ����ӱ��) */
	sf_SendByte((_uiWriteAddr & 0xFF0000) >> 16);	/* ����������ַ�ĸ�8bit */
	sf_SendByte((_uiWriteAddr & 0xFF00) >> 8);		/* ����������ַ�м�8bit */
	sf_SendByte(_uiWriteAddr & 0xFF);				/* ����������ַ��8bit */	
	sf_SendByte(*_pBuf++);							/* ���͵�1������ */
	sf_SendByte(*_pBuf++);							/* ���͵�2������ */
	SF_CS_HIGH();									/* ����Ƭѡ */
	
	sf_WaitForWriteEnd();							/* �ȴ�����Flash�ڲ�д������� */

	_usSize -= 2;									/* ����ʣ���ֽ��� */
	
	for (i = 0; i < _usSize / 2; i++)
	{
		SF_CS_LOW();								/* ʹ��Ƭѡ */
		sf_SendByte(CMD_AAI);						/* ����AAI����(��ַ�Զ����ӱ��) */
		sf_SendByte(*_pBuf++);					/* �������� */
		sf_SendByte(*_pBuf++);					/* �������� */
		SF_CS_HIGH();								/* ����Ƭѡ */
		sf_WaitForWriteEnd();						/* �ȴ�����Flash�ڲ�д������� */
	}
	
	/* ����д����״̬ */
	SF_CS_LOW();
	sf_SendByte(CMD_DISWR);							
	SF_CS_HIGH();
	
	sf_WaitForWriteEnd();							/* �ȴ�����Flash�ڲ�д������� */
}

void sf_WordWrite(uint8_t data, uint32_t _uiWriteAddr)
{
	uint32_t i;
		
	sf_WriteEnable();								/* ����дʹ������ */
	
	/* ������������ */
	SF_CS_LOW();									/* ʹ��Ƭѡ */
	sf_SendByte(CMD_WBYTE);							/* ����AAI����(��ַ�Զ����ӱ��) */
	sf_SendByte((_uiWriteAddr & 0xFF0000) >> 16);	/* ����������ַ�ĸ�8bit */
	sf_SendByte((_uiWriteAddr & 0xFF00) >> 8);		/* ����������ַ�м�8bit */
	sf_SendByte(_uiWriteAddr & 0xFF);				/* ����������ַ��8bit */	
	sf_SendByte(data);							    /* ���͵�1������ */
	
	SF_CS_HIGH();									/* ����Ƭѡ */
	
	sf_WaitForWriteEnd();							/* �ȴ�����Flash�ڲ�д������� */

}
/*
*********************************************************************************************************
*	�� �� ��: sf_ReadBuffer
*	����˵��: ������ȡ�����ֽڡ��ֽڸ������ܳ���оƬ������
*	��    �Σ�	_pBuf : ����Դ��������
*				_uiReadAddr ���׵�ַ
*				_usSize �����ݸ���, ���Դ���PAGE_SIZE,���ǲ��ܳ���оƬ������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void sf_ReadBuffer(uint8_t * _pBuf, uint32_t _uiReadAddr, uint32_t _uiSize)
{
	/* �����ȡ�����ݳ���Ϊ0���߳�������Flash��ַ�ռ䣬��ֱ�ӷ��� */
	if ((_uiSize == 0) ||(_uiReadAddr + _uiSize) > SF_TOTAL_SIZE)
	{
		return;
	}
		
	/* ������������ */
	SF_CS_LOW();									/* ʹ��Ƭѡ */
	sf_SendByte(CMD_READ);							/* ���Ͷ����� */
	sf_SendByte((_uiReadAddr & 0xFF0000) >> 16);	/* ����������ַ�ĸ�8bit */
	sf_SendByte((_uiReadAddr & 0xFF00) >> 8);		/* ����������ַ�м�8bit */
	sf_SendByte(_uiReadAddr & 0xFF);				/* ����������ַ��8bit */	
	while (_uiSize--)
	{
		*_pBuf++ = sf_SendByte(DUMMY_BYTE);			/* ��һ���ֽڲ��洢��pBuf�������ָ���Լ�1 */
	}
	SF_CS_HIGH();									/* ����Ƭѡ */
}

/*
*********************************************************************************************************
*	�� �� ��: sf_CmpData
*	����˵��: �Ƚ�Flash������.
*	��    �Σ�	_ucpTar : ���ݻ�����
*				_uiSrcAddr ��Flash��ַ
*				_uiSize �����ݸ���, ���Դ���PAGE_SIZE,���ǲ��ܳ���оƬ������
*	�� �� ֵ: 0 = ���, 1 = ����
*********************************************************************************************************
*/
static uint8_t sf_CmpData(uint32_t _uiSrcAddr, uint8_t *_ucpTar, uint32_t _uiSize)
{
	uint8_t ucValue;

	/* �����ȡ�����ݳ���Ϊ0���߳�������Flash��ַ�ռ䣬��ֱ�ӷ��� */
	if ((_uiSrcAddr + _uiSize) > SF_TOTAL_SIZE)
	{
		return 1;
	}

	if (_uiSize == 0) 	
	{
		return 0;
	}
		

	SF_CS_LOW();									/* ʹ��Ƭѡ */
	sf_SendByte(CMD_READ);							/* ���Ͷ����� */
	sf_SendByte((_uiSrcAddr & 0xFF0000) >> 16);		/* ����������ַ�ĸ�8bit */
	sf_SendByte((_uiSrcAddr & 0xFF00) >> 8);		/* ����������ַ�м�8bit */
	sf_SendByte(_uiSrcAddr & 0xFF);					/* ����������ַ��8bit */	
	while (_uiSize--)
	{
		/* ��һ���ֽ� */
		ucValue = sf_SendByte(DUMMY_BYTE);		
		if (*_ucpTar++ != ucValue)
		{
			SF_CS_HIGH();
			return 1;
		}
	}
	SF_CS_HIGH();	
	return 0;
}

/*
*********************************************************************************************************
*	�� �� ��: sf_NeedErase
*	����˵��: �ж�дPAGEǰ�Ƿ���Ҫ�Ȳ�����
*	��    �Σ� _ucpOldBuf �� ������
*			   _ucpNewBuf �� ������
*			   _uiLen �����ݸ��������ܳ���ҳ���С
*	�� �� ֵ: 0 : ����Ҫ������ 1 ����Ҫ����
*********************************************************************************************************
*/ 
static uint8_t sf_NeedErase(uint8_t * _ucpOldBuf, uint8_t *_ucpNewBuf, uint16_t _usLen)
{
	uint16_t i;
	uint8_t ucOld;

	/*
	�㷨��1����old ��, new ����
	      old    new
		  1101   0101
	~     1111
		= 0010   0101

	�㷨��2��: old �󷴵Ľ���� new λ��
		  0010   old
	&	  0101   new
		 =0000

	�㷨��3��: ���Ϊ0,���ʾ�������. �����ʾ��Ҫ����
	*/

	for (i = 0; i < _usLen; i++)
	{
		ucOld = *_ucpOldBuf++;
		ucOld = ~ucOld;

		/* ע������д��: if (ucOld & (*_ucpNewBuf++) != 0) */
		if ((ucOld & (*_ucpNewBuf++)) != 0)
		{
			return 1;
		}
	}
	return 0;
}

/*
*********************************************************************************************************
*	�� �� ��: sf_AutoWritePage
*	����˵��: д1��PAGE��У��,�������ȷ������д���Ρ��������Զ���ɲ���������
*	��    �Σ�	_pBuf : ����Դ��������
*				_uiWriteAddr ��Ŀ�������׵�ַ
*				_usSize �����ݸ��������ܳ���ҳ���С
*	�� �� ֵ: 0 : ���� 1 �� �ɹ�
*********************************************************************************************************
*/ 
//static uint8_t sf_AutoWritePage(uint8_t *_ucpSrc, uint32_t _uiWrAddr, uint16_t _usWrLen)
//{
//	uint16_t i;
//	uint16_t j;					/* ������ʱ */
//	uint32_t uiFirstAddr;		/* ������ַ */
//	uint8_t ucNeedErase;		/* 1��ʾ��Ҫ���� */
//	//uint8_t ucaFlashBuf[SF_PAGE_SIZE];
//	uint8_t cRet;

//	/* ����Ϊ0ʱ����������,ֱ����Ϊ�ɹ� */
//	if (_usWrLen == 0)
//	{
//		return 1;
//	}	
//	
//	/* ���ƫ�Ƶ�ַ����оƬ�������˳� */
//	if (_uiWrAddr >= SF_TOTAL_SIZE)
//	{
//		return 0;
//	}

//	/* ������ݳ��ȴ����������������˳� */
//	if (_usWrLen > SF_PAGE_SIZE)
//	{
//		return 0;
//	}

//	/* ���FLASH�е�����û�б仯,��дFLASH */
//	sf_ReadBuffer(s_spiBuf, _uiWrAddr, _usWrLen);
//	if (memcmp(s_spiBuf, _ucpSrc, _usWrLen) == 0)
//	{
//		return 1;
//	}

//	/* �ж��Ƿ���Ҫ�Ȳ������� */
//	/* ����������޸�Ϊ�����ݣ�����λ���� 1->0 ���� 0->0, ���������,���Flash���� */
//	ucNeedErase = 0;
//	if (sf_NeedErase(s_spiBuf, _ucpSrc, _usWrLen))
//	{
//		ucNeedErase = 1;
//	}

//	uiFirstAddr = _uiWrAddr & (~(SF_PAGE_SIZE - 1));

//	if (_usWrLen == SF_PAGE_SIZE)		/* ������������д */
//	{
//		for	(i = 0; i < SF_PAGE_SIZE; i++)
//		{
//			s_spiBuf[i] = _ucpSrc[i];
//		}
//	}
//	else						/* ��д�������� */
//	{
//		/* �Ƚ��������������ݶ��� */
//		sf_ReadBuffer(s_spiBuf, uiFirstAddr, SF_PAGE_SIZE);

//		/* ���������ݸ��� */
//		i = _uiWrAddr & (SF_PAGE_SIZE - 1);
//		memcpy(&s_spiBuf[i], _ucpSrc, _usWrLen);
//	}

//	/* д��֮�����У�飬�������ȷ����д�����3�� */
//	cRet = 0;
//	for (i = 0; i < 3; i++)
//	{

//		/* ����������޸�Ϊ�����ݣ�����λ���� 1->0 ���� 0->0, ���������,���Flash���� */
//		if (ucNeedErase == 1)
//		{			
//			sf_EraseSector(uiFirstAddr);		/* ����1������ */
//		}

//		/* ���һ��PAGE */
//		sf_PageWrite(s_spiBuf, uiFirstAddr, SF_PAGE_SIZE);
//	
//		if (sf_CmpData(_uiWrAddr, _ucpSrc, _usWrLen) == 0)
//		{
//			cRet = 1;
//			break;
//		}
//		else
//		{
//			if (sf_CmpData(_uiWrAddr, _ucpSrc, _usWrLen) == 0)
//			{
//				cRet = 1;
//				break;
//			}

//			/* ʧ�ܺ��ӳ�һ��ʱ�������� */
//			for (j = 0; j < 10000; j++);
//		}
//	}

//	return cRet;
//}

/*
*********************************************************************************************************
*	�� �� ��: sf_AutoWritePage
*	����˵��: д1��PAGE��У��,�������ȷ������д���Ρ��������Զ���ɲ���������
*	��    �Σ�	_pBuf : ����Դ��������
*				_uiWriteAddr ��Ŀ�������׵�ַ
*				_usSize �����ݸ��������ܳ���ҳ���С
*	�� �� ֵ: 0 : ���� 1 �� �ɹ�
*********************************************************************************************************
*/ 
static uint8_t sf_AutoWritePage(uint8_t *_ucpSrc, uint32_t _uiWrAddr, uint16_t _usWrLen)
{
	uint16_t i;
	uint16_t j;					/* ������ʱ */
	uint32_t uiFirstAddr;		/* ������ַ */
	uint8_t ucNeedErase;		/* 1��ʾ��Ҫ���� */
	uint8_t s_spiBuf[SF_PAGE_SIZE];
	uint8_t cRet;
	
	u8 *SrcBufAddr=_ucpSrc;
    u32 WrAddr=_uiWrAddr;
	
	/* ����Ϊ0ʱ����������,ֱ����Ϊ�ɹ� */
	if (_usWrLen == 0)
	{
		return 1;
	}	
	
	/* ���ƫ�Ƶ�ַ����оƬ�������˳� */
	if (_uiWrAddr >= SF_TOTAL_SIZE)
	{
		return 0;
	}

	/* ������ݳ��ȴ����������������˳� */
	if (_usWrLen > SF_PAGE_SIZE)
	{
		return 0;
	}
	
	/* �ȶ�����������������Ϊ���� */
    uiFirstAddr = _uiWrAddr & (~(SF_PAGE_SIZE - 1));
	sf_ReadBuffer(s_spiBuf,  uiFirstAddr,  SF_PAGE_SIZE);
	
	/* ���������������������ı��û��� */
	i = _uiWrAddr & (SF_PAGE_SIZE - 1);
	memcpy(&s_spiBuf[i], _ucpSrc, _usWrLen);
	
	
	/* д��֮�����У�飬�������ȷ����д�����3�� */
	cRet = 0;		
		
	SrcBufAddr=_ucpSrc;
	WrAddr=_uiWrAddr;	

	/* ���ֽڱ��д������ */
	for(j=0;j<_usWrLen;j++)
	{
	  sf_WordWrite(*SrcBufAddr,WrAddr);
	  SrcBufAddr++;
	  WrAddr++;
	
	}
		
	if (sf_CmpData(_uiWrAddr, _ucpSrc, _usWrLen) == 0)
	{
		cRet = 1;
		return cRet;
	}
	else
	{
		if (sf_CmpData(_uiWrAddr, _ucpSrc, _usWrLen) == 0)
		{
			cRet = 1;
			return cRet;
		}
		
		for (i = 0; i < 3; i++)
		{
			sf_EraseSector(uiFirstAddr);
			/* ʧ�ܺ��ӳ�һ��ʱ�������� */
			bsp_DelayMS(5);											
			/* ���һ��PAGE */
			sf_PageWrite(s_spiBuf, uiFirstAddr, SF_PAGE_SIZE);
			bsp_DelayMS(5);
			
			if (sf_CmpData(uiFirstAddr, s_spiBuf, SF_PAGE_SIZE) == 0)
			{
				cRet = 1;
				return cRet;
			}				
		
		}			
		
	}
	
	return cRet;
}

/*
*********************************************************************************************************
*	�� �� ��: sf_WriteBuffer
*	����˵��: д1��������У��,�������ȷ������д���Ρ��������Զ���ɲ���������
*	��    �Σ�	_pBuf : ����Դ��������
*				_uiWrAddr ��Ŀ�������׵�ַ
*				_usSize �����ݸ��������ܳ���ҳ���С
*	�� �� ֵ: 1 : �ɹ��� 0 �� ʧ��
*********************************************************************************************************
*/ 
uint8_t sf_WriteBuffer(uint8_t* _pBuf, uint32_t _uiWriteAddr, uint16_t _usWriteSize)
{
	uint8_t NumOfPage = 0, NumOfSingle = 0, Addr = 0, count = 0, temp = 0;
	
	Addr = _uiWriteAddr % SF_PAGE_SIZE;
	count = SF_PAGE_SIZE - Addr;
	NumOfPage =  _usWriteSize / SF_PAGE_SIZE;
	NumOfSingle = _usWriteSize % SF_PAGE_SIZE;
	
	if (Addr == 0) /* ��ʼ��ַ��ҳ���׵�ַ  */
	{
		if (NumOfPage == 0) /* ���ݳ���С��ҳ���С */
		{
			if (sf_AutoWritePage(_pBuf, _uiWriteAddr, _usWriteSize) == 0)
			{
				return 0;
			}
		}
		else 	/* ���ݳ��ȴ��ڵ���ҳ���С */
		{
			while (NumOfPage--)
			{
				if (sf_AutoWritePage(_pBuf, _uiWriteAddr, SF_PAGE_SIZE) == 0)
				{
					return 0;
				}				
				_uiWriteAddr +=  SF_PAGE_SIZE;
				_pBuf += SF_PAGE_SIZE;
			}
			if (sf_AutoWritePage(_pBuf, _uiWriteAddr, NumOfSingle) == 0)
			{
				return 0;
			}				
		}
	}
	else  /* ��ʼ��ַ����ҳ���׵�ַ  */
	{
		if (NumOfPage == 0) /* ���ݳ���С��ҳ���С */
		{
			if (NumOfSingle > count) /* (_usWriteSize + _uiWriteAddr) > SPI_FLASH_PAGESIZE */
			{
				temp = NumOfSingle - count;
				
				if (sf_AutoWritePage(_pBuf, _uiWriteAddr, count) == 0)
				{
					return 0;
				}
								
				_uiWriteAddr +=  count;
				_pBuf += count;
			
				if (sf_AutoWritePage(_pBuf, _uiWriteAddr, temp) == 0)
				{
					return 0;
				}				
			}
			else
			{
				if (sf_AutoWritePage(_pBuf, _uiWriteAddr, _usWriteSize) == 0)
				{
					return 0;
				}
			}
		}
		else	/* ���ݳ��ȴ��ڵ���ҳ���С */
		{
			_usWriteSize -= count;
			NumOfPage =  _usWriteSize / SF_PAGE_SIZE;
			NumOfSingle = _usWriteSize % SF_PAGE_SIZE;
			
			if (sf_AutoWritePage(_pBuf, _uiWriteAddr, count) == 0)
			{
				return 0;
			}			
				
			_uiWriteAddr +=  count;
			_pBuf += count;
			
			while (NumOfPage--)
			{
				if (sf_AutoWritePage(_pBuf, _uiWriteAddr, SF_PAGE_SIZE) == 0)
				{
					return 0;
				}				
				_uiWriteAddr +=  SF_PAGE_SIZE;
				_pBuf += SF_PAGE_SIZE;
			}
			
			if (NumOfSingle != 0)
			{
				if (sf_AutoWritePage(_pBuf, _uiWriteAddr, NumOfSingle) == 0)
				{
					return 0;
				}				
			}
		}
	}
	return 1;	/* �ɹ� */
}


/*
*********************************************************************************************************
*	�� �� ��: sf_ReadID
*	����˵��: ��ȡ����ID
*	��    �Σ���
*	�� �� ֵ: 32bit������ID (���8bit��0����ЧIDλ��Ϊ24bit��
*********************************************************************************************************
*/  
uint32_t sf_ReadID(void)
{
	uint32_t uiID; 
	uint8_t id1, id2, id3;
	
	sf_WriteEnable();								/* ����дʹ������ */
	
	/* ������������ */
	SF_CS_LOW();									/* ʹ��Ƭѡ */
	sf_SendByte(CMD_RDID);							/* ���Ͷ�ID���� */
	id1 = sf_SendByte(DUMMY_BYTE);					/* ��ID�ĵ�1���ֽ� */
	id2 = sf_SendByte(DUMMY_BYTE);					/* ��ID�ĵ�2���ֽ� */
	id3 = sf_SendByte(DUMMY_BYTE);					/* ��ID�ĵ�3���ֽ� */
	SF_CS_HIGH();									/* ����Ƭѡ */

	uiID = ((uint32_t)id1 << 16) | ((uint32_t)id2 << 8) | id3;
	
	return uiID;
}		

/*
*********************************************************************************************************
*	�� �� ��: sf_SendByte
*	����˵��: ����������һ���ֽڣ�ͬʱ��MISO���߲����������ص�����
*	��    �Σ�_ucByte : ���͵��ֽ�ֵ
*	�� �� ֵ: ��MISO���߲����������ص�����
*********************************************************************************************************
*/
static uint8_t sf_SendByte(uint8_t _ucValue)
{
	/* �ȴ��ϸ�����δ������� */
	while (SPI_I2S_GetFlagStatus(SPI_FLASH, SPI_I2S_FLAG_TXE) == RESET);
	
	/* ͨ��SPIӲ������1���ֽ� */
	SPI_I2S_SendData(SPI_FLASH, _ucValue);
	
	/* �ȴ�����һ���ֽ�������� */
	while (SPI_I2S_GetFlagStatus(SPI_FLASH, SPI_I2S_FLAG_RXNE) == RESET);
	
	/* ���ش�SPI���߶��������� */
	return SPI_I2S_ReceiveData(SPI_FLASH);
}

/*
*********************************************************************************************************
*	�� �� ��: sf_WriteEnable
*	����˵��: ����������дʹ������
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void sf_WriteEnable(void)
{
	SF_CS_LOW();									/* ʹ��Ƭѡ */
	sf_SendByte(CMD_WREN);							/* �������� */
	SF_CS_HIGH();									/* ����Ƭѡ */  
}

/*
*********************************************************************************************************
*	�� �� ��: sf_WriteStatus
*	����˵��: д״̬�Ĵ���
*	��    �Σ�_ucValue : ״̬�Ĵ�����ֵ
*	�� �� ֵ: ��
*********************************************************************************************************
*/  
static void sf_WriteStatus(uint8_t _ucValue)
{
	/* ��1������ʹ��д״̬�Ĵ��� */
	SF_CS_LOW();									/* ʹ��Ƭѡ */
	sf_SendByte(CMD_EWRSR);							/* ������� ����д״̬�Ĵ��� */
	SF_CS_HIGH();									/* ����Ƭѡ */ 
	
	/* ��2������д״̬�Ĵ��� */
	SF_CS_LOW();									/* ʹ��Ƭѡ */
	sf_SendByte(CMD_WRSR);							/* ������� д״̬�Ĵ��� */
	sf_SendByte(_ucValue);							/* �������ݣ�״̬�Ĵ�����ֵ */
	SF_CS_HIGH();									/* ����Ƭѡ */ 	
}	

/*
*********************************************************************************************************
*	�� �� ��: sf_WaitForWriteEnd
*	����˵��: ����ѭ����ѯ�ķ�ʽ�ȴ������ڲ�д�������
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/  
static void sf_WaitForWriteEnd(void)
{
	SF_CS_LOW();									/* ʹ��Ƭѡ */
	sf_SendByte(CMD_RDSR);							/* ������� ��״̬�Ĵ��� */
	while((sf_SendByte(DUMMY_BYTE) & WIP_FLAG) == SET);	/* �ж�״̬�Ĵ�����æ��־λ */
	SF_CS_HIGH();									/* ����Ƭѡ */ 	
}
