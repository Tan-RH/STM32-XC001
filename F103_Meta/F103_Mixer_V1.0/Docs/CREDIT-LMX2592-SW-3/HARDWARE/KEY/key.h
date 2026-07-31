#ifndef __KEY_H
#define __KEY_H	 
#include "sys.h"

#define KEY1  GPIO_ReadInputDataBit(GPIOC,GPIO_Pin_0)
#define KEY2  GPIO_ReadInputDataBit(GPIOC,GPIO_Pin_1)
#define KEY3  GPIO_ReadInputDataBit(GPIOC,GPIO_Pin_2)
#define KEY4  GPIO_ReadInputDataBit(GPIOC,GPIO_Pin_3)
#define KEY5  GPIO_ReadInputDataBit(GPIOC,GPIO_Pin_9)

#define KEY1_PRES 	1	
#define KEY2_PRES	  2	
#define KEY3_PRES	  3	
#define KEY4_PRES   4	
#define KEY5_PRES   5	

extern u32 Step,Start_FRQ;
extern  u16 Vpp_Step,Amp_value;


void KEY_Init(void);//IO初始化
u8 KEY_Scan(u8);  	//按键扫描函数		
void key_Function(void);

#endif
