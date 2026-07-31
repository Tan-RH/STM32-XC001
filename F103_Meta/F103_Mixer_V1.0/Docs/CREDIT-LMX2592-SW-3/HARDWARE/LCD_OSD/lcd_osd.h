#ifndef __LCD_OSD_H
#define __LCD_OSD_H

#include "sys.h"

#define FREQ_UNIT_HZ    "Hz"
#define FREQ_UNIT_KHZ   "KHz"
#define FREQ_UNIT_MHZ   "MHz"
#define FREQ_UNIT_GHZ   "GHz"

#define POWER_UNIT_DBM  "Dbm"
#define POWER_UNIT_STEP  "Step"


//频率最大能设置到多少位
#define FREQ_LEN    8

#define TIME_LEN    3

//SW菜单默认
#define SW_FREQUENCE_DEFAULT    1000000
#define SW_POWER_MODE_DEFAULT     63

//CW菜单默认
#define CW_FREQUENCE_MAX 15000000    //4.4G
#define CW_FREQUENCE_MIN 10000     //35Mhz
#define CW_FREQUENCE_STEP_MAX   10000000  
#define CW_FREQUENCE_STEM_MIN   1 

#define SW_FREQUENCE_MAX_SET    15000000
#define SW_FREQUENCE_MIN_SET    10000
#define SW_FREQUENCE_STEP_MAX   10000000
#define SW_FREQUENCE_STEP_MIN   1

#define SW_FREQUENCE_STEP_STEP_MAX  10000000
#define SW_FREQUENCE_STEP_STEP_MIN  1

#define SW_FREQUENCE_TIME_MAX   1000
#define SW_FREQUENCE_TIME_MIN   1
//#define SW_FREQUENCE_TIME_STEP_MAX  1000

#define POWER_MODE_MAX      63
#define POWER_MODE_MIN      1

typedef struct 
{
    u8 current; //当前页面的索引号
    u8 up;      //按向上 按钮后跳转到页面的索引号
    u8 down;    //按向下 按钮后跳转到页面的索引号
    u8 left;    //按左 按钮后跳转到页面的索引号
    u8 right;   //按右 按钮后跳转到页面的索引号
    u8 enter;   
    void (*current_operation)(void);
}menu_table;


//CW菜单显示的值
typedef struct 
{
    u32 frequence;
    u8 power_mode;
}cw_para_setting;

//SW菜单显示的值
typedef struct 
{
    u32 frequence_max;
    u32 frequence_min;
    u32 frequence_step;
    u32 frequence_time;
}sw_para_setting;

typedef struct 
{
    cw_para_setting cw_para;
    sw_para_setting sw_para;

    //调节菜单时，用到的中间变量
    double cw_frequence_step;
    double sw_frequence_max_step;
    double sw_frequence_min_step;
    double sw_frequence_step_step;
    double sw_frequence_time_step;
    u8 flag_displacement;

    //开机启动模式
    u8 power_on_mode;
}menu_para_setting;

void menu_home(void);
void cw_mode_menu(void);
void sw_mode_menu(void);
void save_menu(void);

void cw_freq_setting(void);
void power_setting(void);


void sw_freqmax_setting(void);
void sw_freqmin_setting(void);
void sw_freqstep_setting(void);
void sw_freqtime_setting(void);


void cw_freq_enter(void);
void cw_freq_up(void);
void cw_freq_down(void);
void cw_freq_left(void);
void cw_freq_right(void);

void power_setting_enter(void);
void power_setting_up(void);
void power_setting_down(void);

void sw_freqmax_enter(void);
void sw_freqmax_up(void);
void sw_freqmax_down(void);
void sw_freqmax_left(void);
void sw_freqmax_right(void);

void sw_freqmin_enter(void);
void sw_freqmin_up(void);
void sw_freqmin_down(void);
void sw_freqmin_left(void);
void sw_freqmin_right(void);

void sw_freqstep_enter(void);
void sw_freqstep_up(void);
void sw_freqstep_down(void);
void sw_freqstep_left(void);
void sw_freqstep_right(void);

void sw_freqtime_enter(void);
void sw_freqtime_up(void);
void sw_freqtime_down(void);
void sw_freqtime_left(void);
void sw_freqtime_right(void);

void save_menu_enter(void);

void LCD_ShowInt_Displacement(u16 x,u16 y,u32 num,u8 len, u16 fc, u16 bc,u8 sizey, u8 displacement);
void LCD_ShowInt_Displacement_SECOND(u16 x,u16 y,u32 num,u8 len,u16 fc,u16 bc,u8 sizey,u8 displacement);
void LCD_ShowFloat_Displacement(u16 x,u16 y,double num,u8 len,u16 fc,u16 bc,u8 sizey,u8 displacement);
void menu_default_para_init(void);
void menu_para_init(void);
void flash_read(void);

void cw_sw_output_lock(void);
void set_cw_freq(double freq,u8 power_mode);
void set_sw_model(uint32_t starthz, uint32_t stophz, uint32_t stephz,u8 time);
#endif
