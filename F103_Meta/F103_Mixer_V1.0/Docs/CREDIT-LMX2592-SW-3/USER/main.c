#include "delay.h"
#include "sys.h"
#include "led.h"
#include "lcd_init.h"
#include "lcd.h"
#include "pic.h"
#include "key.h"
#include "usart.h"	
#include "exti.h"

#include "lmx2592.h"


#include "lcd_osd.h"
#include "stmflash.h"

uint32_t R[65];
extern menu_para_setting menu_para;
int main(void)
{
	u8 key_test = 0;

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	delay_init(); 
	LED_Init();   
	KEY_Init(); 
	EXTIX_Init();  
	LCD_Init();   
	uart_init(115200);  
	
	LMX2592_GPIO_INIT();
	delay_ms(100);
	STMFLASH_Read(Frequence_flash_ADDR+22,(u16 *)(&menu_para.power_on_mode),1);
	if(menu_para.power_on_mode == 0xFF)
	{
		menu_default_para_init();
	}
	else
	{
		flash_read();
		menu_para_init();
	}

	#if 1
	LCD_Fill(0,0,LCD_W,LCD_H,GRED);
	Display_GB2312_String(25,0,24, "科迪特",BLUE,GRED);
	Display_GB2312_String(25,0,24, "科迪特",BLUE,GRED);
	LCD_ShowString(25,24,"LMX2592",BRED,GRED,24,0);
	LCD_ShowString(38,48,"TEST",BRED,GRED,24,0);
	LCD_ShowString(0,72,"MUXOUT: PA7",BLUE,GRED,12,0);
	LCD_ShowString(65,72,"SCK: PA6",BLUE,GRED,12,0);
	LCD_ShowString(0,84,"SDI: PA3",BLUE,GRED,12,0);
	LCD_ShowString(60,84,"CSB:PA2",BLUE,GRED,12,0);
	LCD_ShowString(0,96,"RAMPDIR:PA1",BLUE,GRED,12,0);
	LCD_ShowString(0,132,"Press any key to ",RED,GRED,12,0);
	LCD_ShowString(75,144,"continue!",RED,GRED,12,0);

	delay_ms(100);
	while(key_test==0)
	{
		key_test=KEY_Scan(0);	
	}
	key_test=0;
	
	LCD_Fill(0,0,LCD_W,LCD_H,LBBLUE);
	LCD_ShowString(0,0,"Model: LMX2592",RED,LBBLUE,16,0);
	LCD_ShowString(0,16,"Fun: PLL",RED,LBBLUE,16,0);
	//LCD_ShowString(0,32,"CLK: 300MSPS",RED,LBBLUE,16,0);
	LCD_ShowString(0,48,"OUT: 10MHz-Min",RED,LBBLUE,16,0);	
	LCD_ShowString(0,64,"OUT: 15000MHz-Max",RED,LBBLUE,16,0);
	//LCD_ShowString(0,80,"AMP: 0 - 4095",RED,LBBLUE,16,0);
	LCD_ShowString(0,96,"Power: +5V@1A",RED,LBBLUE,16,0);
	
	LCD_ShowString(25,144,"PLL - TEST",LBBLUE,WHITE,16,0);
	LCD_ShowString(0,114,"Press any key to",BLUE,LBBLUE,12,0);
	LCD_ShowString(75,126,"continue!",BLUE,LBBLUE,12,0);
	
	delay_ms(100);
	while(key_test==0)
	{
		key_test=KEY_Scan(0);
	}
	key_test=0;
	
	#endif
	menu_home();
	while(1)
	{		
		key_Function();
		cw_sw_output_lock();
		LED1=!LED1;
	}
}

