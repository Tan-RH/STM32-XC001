/**************************************************************
 * FilePath     : lcd_osd.c
 * author       :  
 * Version      : V1.0
 * Date         : 2024-02-22
 * brief        : 
 * Description  : 
 **************************************************************/
#include "lcd_osd.h"
#include "lcd.h"
#include "lcd_init.h"
#include "stmflash.h"
#include "delay.h"

#include "lmx2592.h"


extern u8 func_index;
extern u8 sweep_enable;

menu_para_setting menu_para;
/**************************************************************
 *  函数功能: 
 *  入口参数: 
 *  返回数值: 
 *  功能说明: 多级菜单的实现，大体分为两种设计思路：
 *  1.通过双向链表的实现（结构树）
 *  2.通过数组查表实现（索引）
 *  其中 索引法居多，优点：可阅读性好，拓展也不错，查找的性能差不多最优，缺点：有点占用空间
 * 
 * 
 **************************************************************/

menu_table table[41]=
{
    /* 索引号，向上一个，向下一个，左，右，确认，当前状态下执行的操作 */
    /* current up down left right ok,opt */
    {0,0,1,0,0,1,(*menu_home)},
    

    {1,2,2,1,1,4,(*cw_mode_menu)},
    {2,1,1,2,2,6,(*sw_mode_menu)}, 
	{3,2,1,0,0,38,(*save_menu)},


    {4,1,5,4,4,10,(*cw_freq_setting)},
    {5,4,3,5,5,15,(*power_setting)},

    {6,2,7,6,6,18,(*sw_freqmax_setting)},
    {7,6,8,7,7,23,(*sw_freqmin_setting)},
    {8,7,9,8,8,28,(*sw_freqstep_setting)},
    {9,8,3,9,9,33,(*sw_freqtime_setting)},

    {10,11,12,13,14,4,(*cw_freq_enter)},
    {11,11,12,13,14,4,(*cw_freq_up)},
    {12,11,12,13,14,4,(*cw_freq_down)},
    {13,11,12,13,14,4,(*cw_freq_left)},
    {14,11,12,13,14,4,(*cw_freq_right)},

    {15,16,17,16,17,5,(*power_setting_enter)},
    {16,16,17,16,17,5,(*power_setting_up)},
    {17,16,17,16,17,5,(*power_setting_down)},

    {18,19,20,21,22,6,(*sw_freqmax_enter)},
    {19,19,20,21,22,6,(*sw_freqmax_up)},
    {20,19,20,21,22,6,(*sw_freqmax_down)},
    {21,19,20,21,22,6,(*sw_freqmax_left)},
    {22,19,20,21,22,6,(*sw_freqmax_right)},

    {23,24,25,26,27,7,(*sw_freqmin_enter)},
    {24,24,25,26,27,7,(*sw_freqmin_up)},
    {25,24,25,26,27,7,(*sw_freqmin_down)},
    {26,24,25,26,27,7,(*sw_freqmin_left)},
    {27,24,25,26,27,7,(*sw_freqmin_right)},

    {28,29,30,31,32,8,(*sw_freqstep_enter)},
    {29,29,30,31,32,8,(*sw_freqstep_up)},
    {30,29,30,31,32,8,(*sw_freqstep_down)},
    {31,29,30,31,32,8,(*sw_freqstep_left)},
    {32,29,30,31,32,8,(*sw_freqstep_right)},

    {33,34,35,36,37,9,(*sw_freqtime_enter)},
    {34,34,35,36,37,9,(*sw_freqtime_up)},
    {35,34,35,36,37,9,(*sw_freqtime_down)},
    {36,34,35,36,37,9,(*sw_freqtime_left)},
    {37,34,35,36,37,9,(*sw_freqtime_right)},

    {38,0,0,0,0,0,(*save_menu_enter)},
};

void menu_default_para_init(void)
{

    menu_para.cw_para.frequence = SW_FREQUENCE_DEFAULT;
    menu_para.cw_para.power_mode = SW_POWER_MODE_DEFAULT;

    menu_para.sw_para.frequence_max = 200000;
    menu_para.sw_para.frequence_min = 100000;
    menu_para.sw_para.frequence_step = 1000;
    menu_para.sw_para.frequence_time = 1;

    menu_para.cw_frequence_step = 1;
    menu_para.sw_frequence_max_step = 1;
    menu_para.sw_frequence_min_step = 1;
    
    menu_para.sw_frequence_step_step = 1;
    menu_para.flag_displacement = 1;

    menu_para.power_on_mode = 1;
}

void menu_para_init(void)
{
    menu_para.cw_frequence_step = 1;
    menu_para.sw_frequence_max_step = 1;
    menu_para.sw_frequence_min_step = 1;
    
    menu_para.sw_frequence_step_step = 1;
    menu_para.flag_displacement = 1;
}
/**************************************************************
 *  函数功能: 一级菜单界面
 *  入口参数: 
 *  返回数值: 
 *  功能说明: 
 **************************************************************/
void menu_home(void)
{
    LCD_Fill(0,0,LCD_W,LCD_H,GREEN);

	LCD_ShowString(0,0,"CW MODE ",BLUE,CYAN,12,0);
    // 线
    LCD_Fill(0,13,99,14,LIGHTBLUE);
    /* 第二行 */
    LCD_ShowString(0,15,"Freq:",RED,GREEN,12,0);
    LCD_ShowIntNum(30,15,menu_para.cw_para.frequence,FREQ_LEN,BLUE,GREEN,12);
    LCD_ShowString(80,15,FREQ_UNIT_KHZ,RED,GREEN,12,0);
    // 线
    LCD_Fill(0,28,99,29,LIGHTBLUE);

    /* 第三行 */
    LCD_ShowString(0,30,"Power:",RED,GREEN,12,0);
    LCD_ShowIntNum(60,30,menu_para.cw_para.power_mode,2,BLUE,GREEN,12);
    LCD_ShowString(75,30,POWER_UNIT_STEP,RED,GREEN,12,0);

    // 线
    LCD_Fill(0,43,99,44,LIGHTBLUE);

    /* 第四行 */
    LCD_ShowString(0,47,"SW MODE ",BLUE,CYAN,12,0);

    //线
    LCD_Fill(0,60,99,61,LIGHTBLUE);

    /* 第五行 扫频最大频率*/
    LCD_ShowString(0,62,"Max:",RED,GREEN,12,0);
    LCD_ShowIntNum(30,62,menu_para.sw_para.frequence_max,FREQ_LEN,BLUE,GREEN,12);
    LCD_ShowString(80,62,FREQ_UNIT_KHZ,RED,GREEN,12,0);

    //线
    LCD_Fill(0,75,99,76,LIGHTBLUE);

    /* 第六行扫频最小频率 */
	LCD_ShowString(0,77,"Min:",RED,GREEN,12,0);
    LCD_ShowIntNum(30,77,menu_para.sw_para.frequence_min,FREQ_LEN,BLUE,GREEN,12);
    LCD_ShowString(80,77,FREQ_UNIT_KHZ,RED,GREEN,12,0);

    //线
    LCD_Fill(0,90,99,91,LIGHTBLUE);
    /* 第七行 步进频率*/
	LCD_ShowString(0,92,"Step:",RED,GREEN,12,0);
    LCD_ShowIntNum(30,92,menu_para.sw_para.frequence_step,FREQ_LEN,BLUE,GREEN,12);
    LCD_ShowString(80,92,FREQ_UNIT_KHZ,RED,GREEN,12,0);
    //线
    LCD_Fill(0,105,99,106,LIGHTBLUE);

    /* 第八行 步进时间 */
	LCD_ShowString(0,107,"Time:",RED,GREEN,12,0);
    LCD_ShowIntNum(30,107,menu_para.sw_para.frequence_time,TIME_LEN,BLUE,GREEN,12);
    LCD_ShowString(80,107,"ms",RED,GREEN,12,0);

    //线
    LCD_Fill(0,120,99,121,LIGHTBLUE);

    //保存
    LCD_ShowString(70,150,"SAVE",BLUE,CYAN,12,0);

    LCD_ShowString(110,15,"UP",BLUE,MAGENTA,16,0);
    LCD_ShowString(110,50,"DN",BLUE,MAGENTA,16,0);
    LCD_ShowString(110,85,"LF",BLUE,MAGENTA,16,0);
    LCD_ShowString(110,120,"RT",BLUE,MAGENTA,16,0);
    LCD_ShowString(110,145,"OK",BLUE,MAGENTA,16,0);
    LCD_DrawLine(100,0,100,160,RED);

		cw_sw_output_lock();
    if(menu_para.power_on_mode == 1)
    {
        cw_mode_menu();
        func_index = 1;
    }
    else if(menu_para.power_on_mode == 2)
    {
        sw_mode_menu();
        func_index = 2;
    }
}

/**************************************************************
 *  函数功能: cw mode 点频模式菜单，二级菜单界面
 *  入口参数: 
 *  返回数值: 
 *  功能说明: 
 **************************************************************/
void cw_mode_menu(void)
{
    LCD_ShowString(0,0,"CW MODE ",RED,BLUE,12,0);

    LCD_ShowString(0,47,"SW MODE ",BLUE,CYAN,12,0);
    LCD_ShowString(70,150,"SAVE",BLUE,CYAN,12,0);

    LCD_ShowString(0,30,"Power:",RED,GREEN,12,0);
    LCD_ShowString(0,15,"Freq:",RED,GREEN,12,0); 
	
		set_cw_freq(menu_para.cw_para.frequence,menu_para.cw_para.power_mode);
}

void sw_mode_menu(void)
{
    sweep_enable = 1;
    LCD_ShowString(0,47,"SW MODE ",RED,BLUE,12,0);
    LCD_ShowString(0,0,"CW MODE ",BLUE,CYAN,12,0);
    LCD_ShowString(70,150,"SAVE",BLUE,CYAN,12,0);

    LCD_ShowString(0,107,"Time:",RED,GREEN,12,0);
    LCD_ShowString(0,62,"Max:",RED,GREEN,12,0);
    set_sw_model(menu_para.sw_para.frequence_min,menu_para.sw_para.frequence_max,menu_para.sw_para.frequence_step,menu_para.sw_para.frequence_time);
}

void save_menu(void)
{
    LCD_ShowString(70,150,"SAVE",RED,BLUE,12,0);
    //LCD_ShowString(0,47,"SW MODE ",BLUE,CYAN,12,0); 
    //LCD_ShowString(0,0,"CW MODE ",BLUE,CYAN,12,0);
    LCD_ShowString(0,30,"Power:",RED,GREEN,12,0);
    LCD_ShowString(0,107,"Time:",RED,GREEN,12,0);
}


/**************************************************************
 *  函数功能: 三级菜单
 *  入口参数: 
 *  返回数值: 
 *  功能说明: 
 **************************************************************/
void cw_freq_setting(void)
{
    menu_para.flag_displacement = 1;
    menu_para.cw_frequence_step = 1;
    LCD_ShowString(0,15,"Freq:",RED,BLUE,12,0);

    //LCD_ShowString(0,0,"CW MODE ",BLUE,CYAN,12,0); 
    LCD_ShowString(0,30,"Power:",RED,GREEN,12,0);

    LCD_ShowIntNum(30,15,menu_para.cw_para.frequence,FREQ_LEN,BLUE,GREEN,12);
}

void power_setting(void)
{
    menu_para.power_on_mode = 1;
    LCD_ShowIntNum(60,30,menu_para.cw_para.power_mode,2,BLUE,GREEN,12);

    LCD_ShowString(0,30,"Power:",RED,BLUE,12,0);
    LCD_ShowString(0,15,"Freq:",RED,GREEN,12,0); 
}

void sw_freqmax_setting(void)
{
    menu_para.sw_frequence_max_step = 1;
    menu_para.flag_displacement = 1;
    
    LCD_ShowString(0,62,"Max:",RED,BLUE,12,0);
    
    //LCD_ShowString(0,47,"SW MODE ",BLUE,CYAN,12,0);
    LCD_ShowString(0,77,"Min:",RED,GREEN,12,0);

    LCD_ShowIntNum(30,62,menu_para.sw_para.frequence_max,FREQ_LEN,BLUE,GREEN,12);
}

void sw_freqmin_setting(void)
{
    menu_para.sw_frequence_min_step = 1;
    menu_para.flag_displacement = 1;
    LCD_ShowString(0,77,"Min:",RED,BLUE,12,0);

    LCD_ShowString(0,62,"Max:",RED,GREEN,12,0);
    LCD_ShowString(0,92,"Step:",RED,GREEN,12,0);
    LCD_ShowIntNum(30,77,menu_para.sw_para.frequence_min,FREQ_LEN,BLUE,GREEN,12);
}

void sw_freqstep_setting(void)
{
    menu_para.sw_frequence_step_step = 1;
    menu_para.flag_displacement = 1;

    LCD_ShowString(0,92,"Step:",RED,BLUE,12,0);

    LCD_ShowString(0,77,"Min:",RED,GREEN,12,0);
    LCD_ShowString(0,107,"Time:",RED,GREEN,12,0);
    LCD_ShowIntNum(30,92,menu_para.sw_para.frequence_step,FREQ_LEN,BLUE,GREEN,12);
}

void sw_freqtime_setting(void)
{
    menu_para.sw_frequence_time_step = 1;
    menu_para.flag_displacement = 1;
    menu_para.power_on_mode = 2;
    LCD_ShowString(0,107,"Time:",RED,BLUE,12,0);

    LCD_ShowString(0,92,"Step:",RED,GREEN,12,0);
    LCD_ShowIntNum(30,107,menu_para.sw_para.frequence_time,TIME_LEN,BLUE,GREEN,12);
}

/**************************************************************
 *  函数功能: 
 *  入口参数: 
 *  返回数值: 
 *  功能说明: 
 **************************************************************/

void cw_freq_enter(void)
{
    LCD_ShowInt_Displacement(30,15,menu_para.cw_para.frequence,FREQ_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

void cw_freq_up(void)
{
    menu_para.cw_para.frequence += menu_para.cw_frequence_step;
    if(menu_para.cw_para.frequence > CW_FREQUENCE_MAX)
    {
        menu_para.cw_para.frequence = CW_FREQUENCE_MAX;
    }
    LCD_ShowInt_Displacement(30,15,menu_para.cw_para.frequence,FREQ_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
		set_cw_freq(menu_para.cw_para.frequence,menu_para.cw_para.power_mode);
}

void cw_freq_down(void)
{
    menu_para.cw_para.frequence -= menu_para.cw_frequence_step;
    if(menu_para.cw_para.frequence < CW_FREQUENCE_MIN)
    {
        menu_para.cw_para.frequence = CW_FREQUENCE_MIN;
    }
    LCD_ShowInt_Displacement(30,15,menu_para.cw_para.frequence,FREQ_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
		set_cw_freq(menu_para.cw_para.frequence,menu_para.cw_para.power_mode);
}

void cw_freq_left(void)
{
    menu_para.flag_displacement++;
    if(menu_para.flag_displacement > FREQ_LEN)
        menu_para.flag_displacement = FREQ_LEN;
    menu_para.cw_frequence_step *= 10; 
    if(menu_para.cw_frequence_step >= CW_FREQUENCE_STEP_MAX)
    menu_para.cw_frequence_step = CW_FREQUENCE_STEP_MAX;
    LCD_ShowInt_Displacement(30,15,menu_para.cw_para.frequence,FREQ_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

void cw_freq_right(void)
{
    menu_para.flag_displacement--;
    if(menu_para.flag_displacement < 0)
        menu_para.flag_displacement = 1;
    menu_para.cw_frequence_step/=10.0f;
    if(menu_para.cw_frequence_step < CW_FREQUENCE_STEM_MIN)
        menu_para.cw_frequence_step = 1;
    LCD_ShowInt_Displacement(30,15,menu_para.cw_para.frequence,FREQ_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

/**************************************************************
 *  函数功能: 
 *  入口参数: 
 *  返回数值: 
 *  功能说明: 
 **************************************************************/
void power_setting_enter(void)
{
    LCD_ShowIntNum(60,30,menu_para.cw_para.power_mode,2,RED,GREEN,12);
}

void power_setting_up(void)
{
    menu_para.cw_para.power_mode++;
    if(menu_para.cw_para.power_mode > POWER_MODE_MAX)
    menu_para.cw_para.power_mode = POWER_MODE_MIN;
    if(menu_para.cw_para.power_mode < POWER_MODE_MIN)
    menu_para.cw_para.power_mode = POWER_MODE_MAX;
    LCD_ShowIntNum(60,30,menu_para.cw_para.power_mode,2,RED,GREEN,12);
		set_cw_freq(menu_para.cw_para.frequence,menu_para.cw_para.power_mode);
}

void power_setting_down(void)
{
    menu_para.cw_para.power_mode--;
    if(menu_para.cw_para.power_mode > POWER_MODE_MAX)
    menu_para.cw_para.power_mode = POWER_MODE_MIN;
    if(menu_para.cw_para.power_mode < POWER_MODE_MIN)
    menu_para.cw_para.power_mode = POWER_MODE_MAX;
    LCD_ShowIntNum(60,30,menu_para.cw_para.power_mode,2,RED,GREEN,12);
		set_cw_freq(menu_para.cw_para.frequence,menu_para.cw_para.power_mode);
}

void sw_freqmax_enter(void)
{
    LCD_ShowInt_Displacement(30,62,menu_para.sw_para.frequence_max,FREQ_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

void sw_freqmax_up(void)
{
     menu_para.sw_para.frequence_max += menu_para.sw_frequence_max_step;
     if(menu_para.sw_para.frequence_max > SW_FREQUENCE_MAX_SET)
     {
        menu_para.sw_para.frequence_max = SW_FREQUENCE_MAX_SET;
     }
     LCD_ShowInt_Displacement(30,62,menu_para.sw_para.frequence_max,FREQ_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

void sw_freqmax_down(void)
{
    menu_para.sw_para.frequence_max -= menu_para.sw_frequence_max_step;
    if(menu_para.sw_para.frequence_max < SW_FREQUENCE_MIN_SET)
    {
        menu_para.sw_para.frequence_max = SW_FREQUENCE_MIN_SET;
    }
    LCD_ShowInt_Displacement(30,62,menu_para.sw_para.frequence_max,FREQ_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

void sw_freqmax_left(void)
{
    menu_para.flag_displacement++;
    if(menu_para.flag_displacement > FREQ_LEN)
        menu_para.flag_displacement = FREQ_LEN;
    menu_para.sw_frequence_max_step*=10;
    if(menu_para.sw_frequence_max_step > SW_FREQUENCE_STEP_MAX)
        menu_para.sw_frequence_max_step = SW_FREQUENCE_STEP_MAX;
    LCD_ShowInt_Displacement(30,62,menu_para.sw_para.frequence_max,FREQ_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

void sw_freqmax_right(void)
{
    menu_para.flag_displacement--;
    if(menu_para.flag_displacement < 0)
        menu_para.flag_displacement = 1;
    menu_para.sw_frequence_max_step/=10.0f;
    if(menu_para.sw_frequence_max_step < SW_FREQUENCE_STEP_MIN)
        menu_para.sw_frequence_max_step = SW_FREQUENCE_STEP_MIN;
    LCD_ShowInt_Displacement(30,62,menu_para.sw_para.frequence_max,FREQ_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

void sw_freqmin_enter(void)
{
    LCD_ShowInt_Displacement(30,77,menu_para.sw_para.frequence_min,FREQ_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

void sw_freqmin_up(void)
{
        menu_para.sw_para.frequence_min += menu_para.sw_frequence_min_step;
     if(menu_para.sw_para.frequence_min > SW_FREQUENCE_MAX_SET)
     {
        menu_para.sw_para.frequence_min = SW_FREQUENCE_MAX_SET;
     }
     LCD_ShowInt_Displacement(30,77,menu_para.sw_para.frequence_min,FREQ_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

void sw_freqmin_down(void)
{
    menu_para.sw_para.frequence_min -= menu_para.sw_frequence_min_step;
    if(menu_para.sw_para.frequence_min < SW_FREQUENCE_MIN_SET)
    {
        menu_para.sw_para.frequence_min = SW_FREQUENCE_MIN_SET;
    }
    LCD_ShowInt_Displacement(30,77,menu_para.sw_para.frequence_min,FREQ_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

void sw_freqmin_left(void)
{
    menu_para.flag_displacement++;
    if(menu_para.flag_displacement > FREQ_LEN)
        menu_para.flag_displacement = FREQ_LEN;
    menu_para.sw_frequence_min_step*=10;
    if(menu_para.sw_frequence_min_step > SW_FREQUENCE_STEP_MAX)
        menu_para.sw_frequence_min_step = SW_FREQUENCE_STEP_MAX;
    LCD_ShowInt_Displacement(30,77,menu_para.sw_para.frequence_min,FREQ_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

void sw_freqmin_right(void)
{
    menu_para.flag_displacement--;
    if(menu_para.flag_displacement < 0)
        menu_para.flag_displacement = 1;
    menu_para.sw_frequence_min_step/=10.0f;
    if(menu_para.sw_frequence_min_step < SW_FREQUENCE_STEP_MIN)
        menu_para.sw_frequence_min_step = SW_FREQUENCE_STEP_MIN;
    LCD_ShowInt_Displacement(30,77,menu_para.sw_para.frequence_min,FREQ_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

void sw_freqstep_enter(void)
{
     LCD_ShowInt_Displacement(30,92,menu_para.sw_para.frequence_step,FREQ_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

void sw_freqstep_up(void)
{
    menu_para.sw_para.frequence_step += menu_para.sw_frequence_step_step;
     if(menu_para.sw_para.frequence_step > SW_FREQUENCE_STEP_STEP_MAX)
     {
        menu_para.sw_para.frequence_step = SW_FREQUENCE_STEP_STEP_MAX;
     }
     LCD_ShowInt_Displacement(30,92,menu_para.sw_para.frequence_step,FREQ_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

void sw_freqstep_down(void)
{
    menu_para.sw_para.frequence_step -= menu_para.sw_frequence_step_step;
    if(menu_para.sw_para.frequence_step < SW_FREQUENCE_STEP_STEP_MIN)
    {
        menu_para.sw_para.frequence_step = SW_FREQUENCE_STEP_STEP_MIN;
    }
    LCD_ShowInt_Displacement(30,92,menu_para.sw_para.frequence_step,FREQ_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

void sw_freqstep_left(void)
{
    menu_para.flag_displacement++;
    if(menu_para.flag_displacement > FREQ_LEN)
        menu_para.flag_displacement = FREQ_LEN;
    menu_para.sw_frequence_step_step*=10;
    if(menu_para.sw_frequence_step_step > SW_FREQUENCE_STEP_MAX)
        menu_para.sw_frequence_step_step = SW_FREQUENCE_STEP_MAX;
    LCD_ShowInt_Displacement(30,92,menu_para.sw_para.frequence_step,FREQ_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

void sw_freqstep_right(void)
{
    menu_para.flag_displacement--;
    if(menu_para.flag_displacement < 0)
        menu_para.flag_displacement = 1;
    menu_para.sw_frequence_step_step/=10.0f;
    if(menu_para.sw_frequence_step_step < SW_FREQUENCE_STEP_MIN)
        menu_para.sw_frequence_step_step = SW_FREQUENCE_STEP_MIN;
    LCD_ShowInt_Displacement(30,92,menu_para.sw_para.frequence_step,FREQ_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

void sw_freqtime_enter(void)
{
    LCD_ShowInt_Displacement(30,107,menu_para.sw_para.frequence_time,TIME_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

void sw_freqtime_up(void)
{
    menu_para.sw_para.frequence_time += menu_para.sw_frequence_time_step;
     if(menu_para.sw_para.frequence_time > SW_FREQUENCE_TIME_MAX)
     {
        menu_para.sw_para.frequence_time = SW_FREQUENCE_TIME_MAX;
     }
     LCD_ShowInt_Displacement(30,107,menu_para.sw_para.frequence_time,TIME_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

void sw_freqtime_down(void)
{
    menu_para.sw_para.frequence_time -= menu_para.sw_frequence_time_step;
    if(menu_para.sw_para.frequence_time < SW_FREQUENCE_TIME_MIN)
    {
        menu_para.sw_para.frequence_time = SW_FREQUENCE_TIME_MIN;
    }
    LCD_ShowInt_Displacement(30,107,menu_para.sw_para.frequence_time,TIME_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

void sw_freqtime_left(void)
{
    menu_para.flag_displacement++;
    if(menu_para.flag_displacement > TIME_LEN)
        menu_para.flag_displacement = TIME_LEN;
    menu_para.sw_frequence_time_step*=10;
    if(menu_para.sw_frequence_time_step > SW_FREQUENCE_STEP_MAX)
        menu_para.sw_frequence_time_step = SW_FREQUENCE_STEP_MAX;
    LCD_ShowInt_Displacement(30,107,menu_para.sw_para.frequence_time,TIME_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

void sw_freqtime_right(void)
{
    menu_para.flag_displacement--;
    if(menu_para.flag_displacement < 0)
        menu_para.flag_displacement = 1;
    menu_para.sw_frequence_time_step/=10.0f;
    if(menu_para.sw_frequence_time_step < SW_FREQUENCE_STEP_MIN)
        menu_para.sw_frequence_time_step = SW_FREQUENCE_STEP_MIN;
    LCD_ShowInt_Displacement(30,107,menu_para.sw_para.frequence_time,TIME_LEN,BLUE,GREEN,12,menu_para.flag_displacement);
}

void save_menu_enter(void)
{
    u16 data1 = 0,data2 = 0;
    u32 data = 0;
    data = menu_para.cw_para.frequence;
    data1 = data >> 16;
    data2 = data &0x0000FFFF;
    STMFLASH_Write(Frequence_flash_ADDR,(u16 *)(&data1),1);
    STMFLASH_Write(Frequence_flash_ADDR+2,(u16 *)(&data2),1);

    STMFLASH_Write(Frequence_flash_ADDR+4,(u16 *)(&menu_para.cw_para.power_mode),1);

    data = menu_para.sw_para.frequence_max;
    data1 = data >> 16;
    data2 = data &0x0000FFFF;
    STMFLASH_Write(Frequence_flash_ADDR+6,(u16 *)(&data1),1);
    STMFLASH_Write(Frequence_flash_ADDR+8,(u16 *)(&data2),1);

    data = menu_para.sw_para.frequence_min;
    data1 = data >> 16;
    data2 = data &0x0000FFFF;
    STMFLASH_Write(Frequence_flash_ADDR+10,(u16 *)(&data1),1);
    STMFLASH_Write(Frequence_flash_ADDR+12,(u16 *)(&data2),1);

    data = menu_para.sw_para.frequence_step;
    data1 = data >> 16;
    data2 = data &0x0000FFFF;
    STMFLASH_Write(Frequence_flash_ADDR+14,(u16 *)(&data1),1);
    STMFLASH_Write(Frequence_flash_ADDR+16,(u16 *)(&data2),1);

    data = menu_para.sw_para.frequence_time;
    data1 = data >> 16;
    data2 = data &0x0000FFFF;
    STMFLASH_Write(Frequence_flash_ADDR+18,(u16 *)(&data1),1);
    STMFLASH_Write(Frequence_flash_ADDR+20,(u16 *)(&data2),1);

    STMFLASH_Write(Frequence_flash_ADDR+22,(u16 *)(&menu_para.power_on_mode),1);

    menu_home();
}

void flash_read(void)
{
    u16 data1 = 0,data2 = 0;
    u32 data = 0;

    STMFLASH_Read(Frequence_flash_ADDR,(u16 *)(&data1),1);
    STMFLASH_Read(Frequence_flash_ADDR + 2,(u16 *)(&data2),1);
    data = (u32) data1 << 16;
    data = data|data2;
    menu_para.cw_para.frequence = data;
        
    STMFLASH_Read(Frequence_flash_ADDR + 4,(u16 *)(&menu_para.cw_para.power_mode),1);
    
    STMFLASH_Read(Frequence_flash_ADDR + 6,(u16 *)(&data1),1);
    STMFLASH_Read(Frequence_flash_ADDR + 8,(u16 *)(&data2),1);
    data = (u32) data1 << 16;
    data = data|data2;
    menu_para.sw_para.frequence_max = data;

    STMFLASH_Read(Frequence_flash_ADDR + 10,(u16 *)(&data1),1);
    STMFLASH_Read(Frequence_flash_ADDR + 12,(u16 *)(&data2),1);
    data = (u32) data1 << 16;
    data = data|data2;
    menu_para.sw_para.frequence_min = data;

    STMFLASH_Read(Frequence_flash_ADDR + 14,(u16 *)(&data1),1);
    STMFLASH_Read(Frequence_flash_ADDR + 16,(u16 *)(&data2),1);
    data = (u32) data1 << 16;
    data = data|data2;
    menu_para.sw_para.frequence_step = data;

    STMFLASH_Read(Frequence_flash_ADDR + 18,(u16 *)(&data1),1);
    STMFLASH_Read(Frequence_flash_ADDR + 20,(u16 *)(&data2),1);
    data = (u32) data1 << 16;
    data = data|data2;
    menu_para.sw_para.frequence_time = data;

    STMFLASH_Read(Frequence_flash_ADDR + 22,(u16 *)(&menu_para.power_on_mode),1);
}
/**************************************************************
 *  函数功能: 数字移位功能,按左右键时，移动位数
 *  入口参数: 显示数字的x\y坐标，num要显示的数字，len数字的长度，fc字的颜色，bc字的背景色，sizey字号，displacement移位标志位
 *  返回数值: 
 *  功能说明: 
 **************************************************************/
void LCD_ShowInt_Displacement(u16 x,u16 y,u32 num,u8 len, u16 fc, u16 bc,u8 sizey, u8 displacement)
{
    u8 t,sizex;
    u32 i = 1;
    sizex = sizey/2;
    for(t=0;t<len;t++)
    {
        if(displacement == t+1)
        {
            LCD_ShowIntNum(x+(len-t-1)*sizex,y,(num/i%10),1,RED,bc,sizey);
        }
        else
        {
            LCD_ShowIntNum(x+(len-t-1)*sizex,y,(num/i%10),1,fc,bc,sizey);
        }
        i*=10;
    }
}
/**************************************************************
 *  函数功能: 另外一个移位函数，数字移位功能,按左右键时，移动位数
 *  入口参数: 显示数字的x\y坐标，num要显示的数字，len数字的长度，fc字的颜色，bc字的背景色，sizey字号，displacement移位标志位
 *  返回数值: 
 *  功能说明: 
 **************************************************************/

void LCD_ShowInt_Displacement_SECOND(u16 x,u16 y,u32 num,u8 len,u16 fc,u16 bc,u8 sizey,u8 displacement)
{	
	u8 t,sizex;
	u32 i=1;//i为取各个数位时需要除的10  例：qianwan=data1/10000000%10;

	sizex=sizey/2;
	for(t=0;t<len;t++)    //刷各个位
	{
	  if(displacement==t+1)      //选中的位，改变颜色
	  {
		    LCD_ShowIntNum(x+(len-t)*sizex,y,(num/i%10),1,RED,bc,sizey); 
	  }
	  else
	      LCD_ShowIntNum(x+(len-t)*sizex,y,(num/i%10),1,fc,bc,sizey); 
	  i*=10;
	}
}
/**************************************************************
 *  函数功能: 小数的移位函数
 *  入口参数: 
 *  返回数值: 
 *  功能说明: 
 **************************************************************/
void LCD_ShowFloat_Displacement(u16 x,u16 y,double num,u8 len,u16 fc,u16 bc,u8 sizey,u8 displacement)
{	
	u8 t,sizex;
	u32 i=1;//i为取各个数位时需要除的10  例：qianwan=data1/10000000%10;
	u32 num1;   
	num1=(u32)(num*1000);         //取整数
	
	sizex=sizey/2;
	LCD_ShowChar(x+(len-2)*sizex,y,'.',fc,bc,sizey,0);  //小数点位置 3位小数为 len-2
	//len+=1;
	for(t=0;t<len;t++)    //刷各个位
	{
	  if(t<3)//刷小数点后的
	  {
		  if(displacement==t+1)      //选中的位，改变颜色
		  {
			 LCD_ShowIntNum(x+(len-t+1)*sizex,y,(num1/i%10),1,RED,bc,sizey);

		  }
		  else
			  LCD_ShowIntNum(x+(len-t+1)*sizex,y,(num1/i%10),1,fc,bc,sizey);
	  }
	  else if(t>=3) //刷小数点前的
	  {
		  if(displacement==t+1)      //选中的位，改变颜色
		  {
			 
				  LCD_ShowIntNum(x+(len-t)*sizex,y,(num1/i%10),1,RED,bc,sizey);
	
		  }
		  else
			  LCD_ShowIntNum(x+(len-t)*sizex,y,(num1/i%10),1,fc,bc,sizey);  
	  }
	  i*=10;
	}
}

//读取IO口来确认是否有输出。注意不同的板卡修改IO口
void cw_sw_output_lock(void)
{
	if(GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_7)==1)
		LCD_ShowString(0,150,"LOCK  ",RED,CYAN,12,0);
	else
		LCD_ShowString(0,150,"UNLOCK",BLUE,CYAN,12,0);
}

//设置CW频率，把对应模块的设置频率函数放进来。
void set_cw_freq(double freq,u8 power_mode)
{
	Frequencyfixed(freq,1,power_mode);
}

//设置SW频率动作,需要把模块驱动中写点频的程序放进来

void set_sw_model(uint32_t starthz, uint32_t stophz, uint32_t stephz,u8 time)
{
	uint32_t i = 0;
	while(sweep_enable)
	{
	if (starthz < stophz) //从低到高
	{
		for (i = 0; i < ((stophz - starthz) / stephz); i++)
		{
				//设置SW频率动作,需要把模块驱动中写点频的程序放进来
			Frequencyfixed(starthz + i * stephz,1,menu_para.cw_para.power_mode); //单位MHz
	
			delay_ms(time);			
		}
	}
	}
}