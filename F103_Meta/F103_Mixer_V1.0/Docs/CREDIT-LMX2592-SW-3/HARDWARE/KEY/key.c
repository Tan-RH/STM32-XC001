#include "stm32f10x.h"
#include "key.h"
#include "sys.h" 
#include "delay.h"
#include "led.h"
#include "lcd.h"

#include "lcd_osd.h"	    

u8 func_index = 0;
u8 last_index = 127;
extern menu_table table[30];

void (*current_opration_index)();
void KEY_Init(void) 
{ 
 	GPIO_InitTypeDef GPIO_InitStructure;
 
 	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC,ENABLE);

	GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; 
 	GPIO_Init(GPIOC, &GPIO_InitStructure);
}

u8 KEY_Scan(u8 mode)
{	 
	static u8 key_up=1;
	
	if(mode)key_up=1;  
	
	if(key_up&&(KEY1==0||KEY2==0||KEY3==0||KEY4==0||KEY5==0))
	{
		delay_ms(10);
		key_up=0;
		if(KEY1==0)return KEY1_PRES;
		else if(KEY2==0)return KEY2_PRES;
		else if(KEY3==0)return KEY3_PRES;
		else if(KEY4==0)return KEY4_PRES;
		else if(KEY5==0)return KEY5_PRES;	
	}
	
	else if(KEY1==1 && KEY2==1 && KEY3==1 && KEY4==1 && KEY5==1)  key_up=1; 
	
 	return 0;
}

void key_Function(void)
{ 
	u8 key_value=0;
	key_value=KEY_Scan(0);
	
	if(key_value)
	{	
		printf("func_index = %d\n",func_index);
		switch(key_value) 
		{
			case KEY1_PRES:		//OK
				func_index = table[func_index].enter;
				break;
			case KEY2_PRES:     //RIGHT
				func_index = table[func_index].right;
				break;			 
			case KEY3_PRES:     //LEFT
				func_index = table[func_index].left;
				break;
			case KEY4_PRES:		//DOWN	
				func_index = table[func_index].down;
				break;
			case KEY5_PRES:     //UP
				func_index = table[func_index].up;
				break;			 
			default:
				break;
		}

		//if(func_index != last_index)
		{
			current_opration_index = table[func_index].current_operation;
			(*current_opration_index)();
			last_index = func_index;
		}
		LED2=!LED2;
	}
}
