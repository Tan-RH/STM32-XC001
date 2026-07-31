#ifndef __LMX2592_H
#define	__LMX2592_H
#include "stm32f10x.h"

#define LMX2592_CLK 	PAout(6)
#define LMX2592_LE 		PAout(2)
#define LMX2592_DATA 	PAout(3)

#define LMX2592_CE 		PAout(1)

#define LMX2592_MUX 	PAin(7)

extern uint32_t Sweepmode;
extern uint8_t Sdatareceived;
extern uint8_t Newdata;
extern uint32_t R[65];

void Registerdata(double Freqout, double Space,u8 Power);			//定义计算寄存器参数函数输入
uint32_t Gcd(uint32_t a, uint32_t b);				//定义求解最大公约数函数
void RegDataInit_Lmx2592(void);							//定义Lmx2592寄存器初始化函数

void SendData(uint32_t a);					//定义写控制字程序
void LMX2592_GPIO_INIT(void);		//写控制字引脚初始化函数
void Frequencyfixed(double f1, double t1,u8 dBm);
void SendDataArray(void);
void Function_GPIO_Config(void);
void SWEEP_INIT(uint32_t starthz, uint32_t stophz, uint32_t stephz);

#endif 

