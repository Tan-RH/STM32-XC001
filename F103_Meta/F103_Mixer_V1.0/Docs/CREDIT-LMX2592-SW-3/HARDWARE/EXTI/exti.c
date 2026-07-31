#include "exti.h"
#include "led.h"
#include "key.h"
#include "delay.h"
#include "usart.h"
#include "beep.h"
//#include "menu.h"
#include "stm32f10x_exti.h"
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK战舰STM32开发板
//外部中断 驱动代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//修改日期:2012/9/3
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2009-2019
//All rights reserved									  
//////////////////////////////////////////////////////////////////////////////////   
//外部中断0服务程序
u8 exit_flag = 0;
u8 sweep_enable = 0;
void EXTIX_Init(void)
{
		EXTI_InitTypeDef EXTI_InitStructure;
		NVIC_InitTypeDef NVIC_InitStructure;
		GPIO_InitTypeDef  GPIO_InitStructure;
		
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);	 //使能A端口时钟
		GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_9;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; //设置成上拉输入
		GPIO_Init(GPIOC, &GPIO_InitStructure);//初始化GPIOE2,3,4
	  

	  GPIO_EXTILineConfig(GPIO_PortSourceGPIOC,GPIO_PinSource0);
  	EXTI_InitStructure.EXTI_Line=EXTI_Line0;	//KEY1
  	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;	
  	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
  	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  	EXTI_Init(&EXTI_InitStructure);	 	//根据EXTI_InitStruct中指定的参数初始化外设EXTI寄存器
		 
		GPIO_EXTILineConfig(GPIO_PortSourceGPIOC,GPIO_PinSource1);
  	EXTI_InitStructure.EXTI_Line=EXTI_Line1;	//KEY1
  	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;	
  	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
  	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  	EXTI_Init(&EXTI_InitStructure);	 	//根据EXTI_InitStruct中指定的参数初始化外设EXTI寄存器
		
		GPIO_EXTILineConfig(GPIO_PortSourceGPIOC,GPIO_PinSource2);
  	EXTI_InitStructure.EXTI_Line=EXTI_Line2;	//KEY1
  	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;	
  	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
  	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  	EXTI_Init(&EXTI_InitStructure);	 	//根据EXTI_InitStruct中指定的参数初始化外设EXTI寄存器
		
		GPIO_EXTILineConfig(GPIO_PortSourceGPIOC,GPIO_PinSource3);
  	EXTI_InitStructure.EXTI_Line=EXTI_Line3;	//KEY1
  	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;	
  	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
  	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  	EXTI_Init(&EXTI_InitStructure);	 	//根据EXTI_InitStruct中指定的参数初始化外设EXTI寄存器
		
		GPIO_EXTILineConfig(GPIO_PortSourceGPIOC,GPIO_PinSource9);
  	EXTI_InitStructure.EXTI_Line=EXTI_Line9;	//KEY1
  	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;	
  	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
  	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  	EXTI_Init(&EXTI_InitStructure);	 	//根据EXTI_InitStruct中指定的参数初始化外设EXTI寄存器
	
		NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;			//使能按键KEY2所在的外部中断通道
  	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x02;	//抢占优先级2， 
  	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;					//子优先级2
  	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;								//使能外部中断通道
  	NVIC_Init(&NVIC_InitStructure);
		
		NVIC_InitStructure.NVIC_IRQChannel = EXTI1_IRQn;			//使能按键KEY2所在的外部中断通道
  	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x02;	//抢占优先级2， 
  	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;					//子优先级2
  	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;								//使能外部中断通道
  	NVIC_Init(&NVIC_InitStructure);
		
		NVIC_InitStructure.NVIC_IRQChannel = EXTI2_IRQn;			//使能按键KEY2所在的外部中断通道
  	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x02;	//抢占优先级2， 
  	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;					//子优先级2
  	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;								//使能外部中断通道
  	NVIC_Init(&NVIC_InitStructure);
		
		NVIC_InitStructure.NVIC_IRQChannel = EXTI3_IRQn;			//使能按键KEY2所在的外部中断通道
  	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x02;	//抢占优先级2， 
  	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;					//子优先级2
  	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;								//使能外部中断通道
  	NVIC_Init(&NVIC_InitStructure);
		
    NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;			//使能按键KEY2所在的外部中断通道
  	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;	//抢占优先级2， 
  	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;					//子优先级2
  	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;								//使能外部中断通道
  	NVIC_Init(&NVIC_InitStructure);
		
	  
}
 
void EXTI0_IRQHandler(void)//TRIG
{ 
		sweep_enable = 0;
		EXTI_ClearITPendingBit(EXTI_Line0);  //清除LINE2上的中断标志位  
}

void EXTI1_IRQHandler(void)//TRIG
{ 
		EXTI_ClearITPendingBit(EXTI_Line1);  //清除LINE2上的中断标志位  
}

void EXTI2_IRQHandler(void)//TRIG
{ 
		EXTI_ClearITPendingBit(EXTI_Line2);  //清除LINE2上的中断标志位  
}

void EXTI3_IRQHandler(void)//TRIG
{ 
		//exit_flag = 1;
		sweep_enable = 0;
		EXTI_ClearITPendingBit(EXTI_Line3);  //清除LINE2上的中断标志位  
}

void EXTI9_5_IRQHandler(void)
{
		//exit_flag = 1;
		sweep_enable = 0;
		EXTI_ClearITPendingBit(EXTI_Line9);  //清除LINE2上的中断标志位  
}
