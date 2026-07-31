#include "led.h"

void LED_Init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
 	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOC|RCC_APB2Periph_AFIO, ENABLE);	 //使能C端口时钟
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable , ENABLE);  //PB3 PB4配置成普通IO，默认为JTAG
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15 ;	 
 	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 //推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;//速度50MHz
 	GPIO_Init(GPIOA, &GPIO_InitStructure);	  //初始化GPIOA
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 ;	 
 	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 //推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;//速度50MHz
 	GPIO_Init(GPIOC, &GPIO_InitStructure);	  //初始化GPIOA
	
 	GPIO_SetBits(GPIOA,GPIO_Pin_15);
	GPIO_SetBits(GPIOC,GPIO_Pin_8);
}





