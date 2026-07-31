#include "lmx2592.h"
#include "delay.h"
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <math.h>

/********定义R[0]寄存器控制变量*********/
		uint16_t   POWERDOWN = 0;						//定义软件关断变量
		uint16_t	 RESET_SOFT = 0;				  //定义软件重启变量
		uint16_t   MUXOUT_SEL = 0;		  		//定义Muxout引脚功能变量
		uint16_t   FCAL_EN = 0;							//定义VCO频率校准使能变量
		uint16_t   ACAL_EN = 0;             //定义VCO幅度校准使能变量
		uint16_t   FCAL_LPFD_ADJ = 0;       //定义低鉴相频率VCO频率校准微调变量 
		uint16_t   FCAL_HPFD_ADJ = 0;				//定义高鉴相频率VCO频率校准微调变量
		uint16_t   LD_EN = 0;					    	//定义锁定检测使能变量
	
/********定义R[1]寄存器控制变量*********/

		uint16_t   CAL_CLK_DIV = 0;					//定义VCO校准时钟分频变量

/********定义R[4]寄存器控制变量*********/

		uint16_t   ACAL_CMP_DLY = 0;				//定义VCO幅度调整时延变量

/********定义R[8]寄存器控制变量*********/

		uint16_t   VCO_IDAC_OVR = 0;				//定义自动幅度调整使能变量
		uint16_t   VCO_CAPCTRL_OVR = 0;			//定义自动频率调整使能变量

/********定义R[9]寄存器控制变量*********/

		uint16_t   OSC_2X = 0;							//定义参考频率2倍频变量
		uint16_t   REF_EN = 0;							//定义参考路径使能变量

/********定义R[10]寄存器控制变量*********/

		uint16_t   MULT = 0;								//定义参考频率乘法器变量
		
/********定义R[11]寄存器控制变量*********/

		uint16_t   PLL_R = 0;								//定义参考频率分频变量		

/********定义R[12]寄存器控制变量*********/

		uint16_t   Pll_R_PRE = 0;						//定义参考频率预分频变量	

/********定义R[13]寄存器控制变量*********/

		uint16_t   CP_EN = 0;								//定义电荷泵使能变量
		uint16_t   PFD_CTL = 0;							//定义PFD模式变量

/********定义R[14]寄存器控制变量*********/

		uint16_t   CP_IDN = 0;							//定义电荷泵电流DN变量
		uint16_t   CP_IUP = 0;							//定义电荷泵电流UP变量
		uint16_t   CP_ICOARSE = 0;          //定义电荷泵增益倍数变量
		
/********定义R[19]寄存器控制变量*********/

		uint16_t   VCO_IDAC = 0;						//定义VCO幅度变量
	
/********定义R[20]寄存器控制变量*********/

		uint16_t   ACAL_VCO_IDAC_STRT = 0;	//定义

/********定义R[22]寄存器控制变量*********/

		uint16_t   VCO_CAPCTRL = 0;						//定义VCO频率变量

/********定义R[23]寄存器控制变量*********/

		uint16_t   FCAL_VCO_SEL_START = 0;		//定义
		uint16_t   VCO_SEL = 0;
		uint16_t   VCO_SEL_FORCE = 0;					//

/********定义R[30]寄存器控制变量*********/

		uint16_t   MASH_DITHER = 0;					//定义
		uint16_t   VCO_2X_EN = 0;						//定义VCO倍频器使能变量
				
/********定义R[31]寄存器控制变量*********/

		uint16_t   VCO_DISTB_PD = 0;				//定义VCO与端口B之间缓冲器关断使能变量
		uint16_t   VCO_DISTA_PD = 0;				//定义VCO与端口A之间缓冲器关断使能变量
		uint16_t   CHDIV_DIST_PD = 0;				//定义VCO与通道分频器之间缓冲器关断使能变量

/********定义R[34]寄存器控制变量*********/

		uint16_t   CHDIV_EN = 0;						//定义通道分频器使能变量

/********定义R[35]寄存器控制变量*********/

		uint16_t   CHDIV_SEG2 = 0;					//定义通道分频器第2部分变量
		uint16_t   CHDIV_SEG3_EN = 0;				//定义通道分频器第3部分使能变量
		uint16_t   CHDIV_SEG2_EN = 0;				//定义通道分频器第2部分使能变量
		uint16_t   CHDIV_SEG1 = 0;					//定义通道分频器第1部分变量
		uint16_t   CHDIV_SEG1_EN = 0;				//定义通道分频器第1部分使能变量

/********定义R[36]寄存器控制变量*********/

		uint16_t   CHDIV_DISTB_EN = 0;			//定义端口B与通道分频器之间缓冲器使能变量
		uint16_t   CHDIV_DISTA_EN = 0;			//定义端口A与通道分频器之间缓冲器使能变量
		uint16_t   CHDIV_SEG_SEL = 0;				//定义通道分频器选择变量
		uint16_t   CHDIV_SEG3 = 0;          //定义通道分频器第3部分变量

/********定义R[37]寄存器控制变量*********/

		uint16_t   PLL_N_PRE = 0;					//定义整数预分频变量

/********定义R[38]寄存器控制变量*********/

		uint16_t   PLL_N = 0;							//定义整数部分变量

/********定义R[39]寄存器控制变量*********/

		uint16_t   PFD_DLY = 0;							//定义PFD延时周期

/********定义R[40]寄存器控制变量*********/

		uint32_t   PLL_DEN = 0;							//定义小数部分分母变量
		uint16_t   PLL_DEN_H = 0;						//定义小数部分分母高16位变量

/********定义R[41]寄存器控制变量*********/

		uint16_t   PLL_DEN_L = 0;						//定义小数部分分母低16位变量

/********定义R[42]寄存器控制变量*********/
		
		uint32_t	 MASH_SEED = 0;						//定义MASH_SEED变量
		uint16_t   MASH_SEED_H = 0;					//定义MASH_SEED高16位变量
		
/********定义R[43]寄存器控制变量*********/

		uint16_t   MASH_SEED_L = 0;						//定义MASH_SEED低16位变量

/********定义R[44]寄存器控制变量*********/

		uint32_t   PLL_NUM = 0;							//定义小数部分分子变量
		uint16_t   PLL_NUM_H = 0;						//定义小数部分分子高16位变量		

/********定义R[45]寄存器控制变量*********/

		uint16_t   PLL_NUM_L = 0;						//定义小数部分分子低16位变量
		
/********定义R[46]寄存器控制变量*********/	

		uint16_t   OUTA_POW = 0;						//定义端口A输出功率变量
		uint16_t   OUTB_PD = 0;							//定义端口B关断使能变量
		uint16_t   OUTA_PD = 0;							//定义端口A关断使能变量
		uint16_t	 MASH_EN = 0;							//定义sigma-delta调制使能变量
		uint16_t   MASH_ORDER = 0;				  //定义sigma-delta调制阶数变量
		
/********定义R[47]寄存器控制变量*********/		
		
		uint16_t   OUTA_MUX = 0;						//定义端口A信号输出通道变量
		uint16_t   OUTB_POW = 0;						//定义端口B输出功率变量
		
/********定义R[48]寄存器控制变量*********/		
		
		uint16_t   OUTB_MUX = 0;						//定义端口B信号输出通道变量
		
/********定义R[59]寄存器控制变量*********/			
		
		uint16_t   MUXOUT_HDRV = 0;					//定义MUXOUT高电流输出使能变量
		
/********定义R[61]寄存器控制变量*********/			
		
		uint16_t   LD_TYPE = 0;							//定义锁定检测类型变量		
		
/********定义R[64]寄存器控制变量*********/			
		
		uint16_t   ACAL_FAST = 0;		 				//定义快速幅度校准使能变量
		uint16_t   FCAL_FAST = 0;	        	//定义快速频率校准使能变量		
		uint16_t   AJUMP_SIZE = 0;					//定义幅度调整步进增量变量
		uint16_t   FJUMP_SIZE = 0;					//定义频率调整步进增量变量
	
	
double   	 Outputfrequency = 0;		//定义输出信号频率变量
double 		 Resolution = 0;				//频率分辨率变量
double	   Resvco = 0;						//定义VCO通道分辨率变量		
double     Fpfd = 0;							//定义鉴相频率变量
double     FRE_VCO=0;
double	 Int_frc = 0;						//定义求解INT和FRAC的中间变量
double     Frac_Value=0;

uint32_t   Dividervalue = 0;			//定义RF分频器数值
uint32_t   Refin = 0;							//输入参考频率变量	
uint32_t   Frequencyint = 0;      //定义频率整数部分变量
uint32_t	 Frequencyfra = 0;      //定义频率小数部分变量
uint32_t   Grecd = 0;							//定义最大公约数变量（Modulus和Fractionalvalue的最大公约数）


//单音频率写入配置：频率、通道分辨率、功率
void Frequencyfixed(double f1, double t1,u8 dBm)
{	
		 Registerdata(f1,t1,dBm);
//			SendData(0x00231E);					//复位芯片
//			SendData(0x00231C);					//去掉芯片复位
			
//			SendDataArray();			     //依次写入寄存器参数
			SendData(R[0]);							//再写入1次R[0]，确保芯片工作起来
}

//LM2592-GPIO初始化
void LMX2592_GPIO_INIT(void)
{		
		GPIO_InitTypeDef GPIO_InitStructure;																								/*定义一个GPIO_InitTypeDef类型的结构体*/
		RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA, ENABLE); 															/*开启GPIOA的外设时钟*/
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_6;								/*选择要控制的GPIOA引脚*/
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;																		/*设置引脚模式为通用推挽输出*/
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;																		/*设置引脚速率为50MHz */ 
		GPIO_Init(GPIOA, &GPIO_InitStructure);																							/*调用库函数，初始化GPIOA*/	
		
		GPIO_ResetBits(GPIOA, GPIO_Pin_6);	
	
		GPIO_SetBits(GPIOA, GPIO_Pin_2|GPIO_Pin_3);	
		
		delay_ms(10);
		LMX2592_CE=1; 
//		SendData(0x00231E);					//复位芯片
		SendData(0x00231E);					//复位芯片
	  SendData(0x00231C);					//去掉芯片复位
		RegDataInit_Lmx2592();  //LMX2592初始化寄存器配置
		SendDataArray();			//依次写入寄存器参数
}

void RegDataInit_Lmx2592(void)
{
	/********R[0]寄存器相关参数初始化配置*********/
		  POWERDOWN = 0;							
		  RESET_SOFT = 0;							
		  MUXOUT_SEL = 1;							
		  FCAL_EN = 1;			  //VCO内部频率校准，1为打开，0为关闭					
		  ACAL_EN = 1;				//VCO内部幅度校准，1为打开，0为关闭 //校准关闭后，锁定时间可以到400uS, 打开后到800uS				  
		  FCAL_LPFD_ADJ = 0;    	  	
	  	FCAL_HPFD_ADJ = 0;					
			LD_EN = 1;									

			R[0] = 0x000200;						
			R[0] += POWERDOWN;
			R[0] += RESET_SOFT<<1;
			R[0] += MUXOUT_SEL<<2;
			R[0] += FCAL_EN<<3;
			R[0] += ACAL_EN<<4;
			R[0] += FCAL_LPFD_ADJ<<5;
			R[0] += FCAL_HPFD_ADJ<<7;
			R[0] += LD_EN<<13;
	
	/********R[1]寄存器相关参数初始化配置*********/
			CAL_CLK_DIV = 0;		  			
			R[1] = 0x010808;						
			R[1] += CAL_CLK_DIV;
	/********R[2]寄存器相关参数初始化配置*********/
			R[2] = 0x020500;						
	/********R[4]寄存器相关参数初始化配置*********/	

			ACAL_CMP_DLY = 5;					

			R[4] = 0x040043;						
			R[4] += ACAL_CMP_DLY<<8;

	/********R[7]寄存器相关参数初始化配置*********/

			R[7] = 0x0728B2;						
			
	/********R[8]寄存器相关参数初始化配置*********/

			VCO_IDAC_OVR = 0;						
			VCO_CAPCTRL_OVR = 0;				

			R[8] = 0x081084;						
			R[8] += VCO_IDAC_OVR<<13;
			R[8] += VCO_CAPCTRL_OVR<<10;


	/********R[9]寄存器相关参数初始化配置*********/

			OSC_2X = 0;									
			REF_EN = 1;									

			R[9] = 0x090102;						
			R[9] += OSC_2X<<11;
			R[9] += REF_EN<<9;

	/********R[10]寄存器相关参数初始化配置*********/

			MULT = 1;										

			R[10] = 0x0A1058;						
			R[10] += MULT<<7;

	/********R[11]寄存器相关参数初始化配置*********/

			PLL_R = 1;								

			R[11] = 0x0B0008;					
			R[11] += PLL_R<<4;

			
	/********R[12]寄存器相关参数初始化配置*********/

			Pll_R_PRE = 1;						

			R[12] = 0x0C7000;					
			R[12] += Pll_R_PRE;	

	/********R[13]寄存器相关参数初始化配置*********/

			CP_EN = 1;								
			PFD_CTL = 0;							

			R[13] = 0x0D0000;					
			R[13] += CP_EN<<14;	
			R[13] += PFD_CTL<<8;

	/********R[14]寄存器相关参数初始化配置*********/

			CP_IDN = 3;								
			CP_IUP = 3;								
			CP_ICOARSE = 3;        	  

			R[14] = 0x0E0000;					
			R[14] += CP_IDN<<7;	
			R[14] += CP_IUP<<2;
			R[14] += CP_ICOARSE;

	/********R[19]寄存器相关参数初始化配置*********/

			VCO_IDAC = 350;						
			
			R[19] = 0x130005;					
			R[19] += VCO_IDAC<<3;

	/********R[20]寄存器相关参数初始化配置*********/

			ACAL_VCO_IDAC_STRT = 300;		

			R[20] = 0x140000;						
			R[20] += ACAL_VCO_IDAC_STRT;

	/********R[22]寄存器相关参数初始化配置*********/

			VCO_CAPCTRL = 0;						
			
			R[22] = 0x162300;						
			R[22] += VCO_CAPCTRL;

	/********R[23]寄存器相关参数初始化配置*********/

			FCAL_VCO_SEL_START = 0;			
			VCO_SEL = 1;								
			VCO_SEL_FORCE = 0;					

			R[23] = 0x178342;						
			R[23] += FCAL_VCO_SEL_START<<14;
			R[23] += VCO_SEL<<11;
			R[23] += VCO_SEL_FORCE<<10;

	/********R[24]寄存器相关参数初始化配置*********/
			
			R[24] = 0x180509;						

	/********R[25]寄存器相关参数初始化配置*********/
	
			R[25] = 0x190000;						

	/********R[28]寄存器相关参数初始化配置*********/
	
			R[28] = 0x1C2924;						
			
	/********R[29]寄存器相关参数初始化配置*********/
	
			R[29] = 0x1D0084;							

	/********R[32]寄存器相关参数初始化配置*********/
	
			R[32] = 0x20210A;						
			
	/********R[33]寄存器相关参数初始化配置*********/
	
			R[33] = 0x212A0A;								

	/********R[48]寄存器相关参数初始化配置*********/	
		
			OUTB_MUX = 1;								

			R[48] = 0x3003FC;						
			R[48] += OUTB_MUX;

	/********R[59]寄存器相关参数初始化配置*********/		
		
			MUXOUT_HDRV = 0;						
			R[59] = 0x3B0000;						
			R[59] += MUXOUT_HDRV<<5;

	/********R[61]寄存器相关参数初始化配置*********/		
		
			LD_TYPE = 1;								
			R[61] = 0x3D0000;						
			R[61] += LD_TYPE;
			
	/********R[62]寄存器相关参数初始化配置*********/	
			
			R[62] = 0x3E0000;						
		
	/********R[64]寄存器相关参数初始化配置*********/		
		
			ACAL_FAST = 1;	  //快速锁定时，校准使能设置	 				
			FCAL_FAST = 1;	        	
			AJUMP_SIZE = 3;						
			FJUMP_SIZE = 15;					

			R[64] = 0x400010;						
			R[64] += ACAL_FAST<<9;
			R[64] += FCAL_FAST<<8;
			R[64] += AJUMP_SIZE<<5;
			R[64] += FJUMP_SIZE;

}

/*********寄存器值计算函数********
功能：根据输入变量计算寄存器值
输入：Frequencyout（输出频率，单位KHz），Ref（参考频率，单位KHz），Chanspace（分辨率，单位KHz）
*******************************/
void Registerdata(double Freqout, double Space,u8 Power)
{
			Outputfrequency = Freqout; 		   	/*输出信号频率，单位KHz*/
			Resolution = Space;								/*频率分辨率，单位KHz*/
			Refin = 50000;       						/*输入参考频率，单位KHz*/
	
//			Refin = 100000; 
	
			if ( Outputfrequency > 7100000)
				{							
								VCO_2X_EN = 1;					
								PLL_N_PRE = 1;					
								CHDIV_DIST_PD = 1;			
								CHDIV_DISTA_EN = 0;			
								VCO_DISTA_PD = 0;				
								OUTA_MUX = 1;						
								CHDIV_EN = 0;						
								CHDIV_SEG1 = 0;					
								CHDIV_SEG1_EN = 0;			
								CHDIV_SEG2 = 0;					
								CHDIV_SEG2_EN = 0;			
								CHDIV_SEG3 = 0;         
								CHDIV_SEG3_EN = 0;			
								CHDIV_SEG_SEL = 0;			
								Dividervalue = 1;
				}
				
			else if ( Outputfrequency >= 3550000 && Outputfrequency <= 7100000 )
						{
								VCO_2X_EN = 0;					
								PLL_N_PRE = 0;					
								CHDIV_DIST_PD = 1;			
								CHDIV_DISTA_EN = 0;			
								VCO_DISTA_PD = 0;				
								OUTA_MUX = 1;						
								
								CHDIV_EN = 0;						
								CHDIV_SEG1 = 0;					
								CHDIV_SEG1_EN = 0;			
								CHDIV_SEG2 = 0;					
								CHDIV_SEG2_EN = 0;			
								CHDIV_SEG3 = 0;         
								CHDIV_SEG3_EN = 0;			
								CHDIV_SEG_SEL = 0;			
								
								Dividervalue = 1;
						}
						
			else if ( Outputfrequency >= 1775000 && Outputfrequency < 3550000 )
						{
								VCO_2X_EN = 0;					
								PLL_N_PRE = 0;					
								CHDIV_DIST_PD = 0;			
								CHDIV_DISTA_EN = 1;			
								VCO_DISTA_PD = 1;				
								OUTA_MUX = 0;						
							
								CHDIV_EN = 1;						
								CHDIV_SEG1 = 0;					
								CHDIV_SEG1_EN = 1;			
								CHDIV_SEG2 = 0;					
								CHDIV_SEG2_EN = 0;			
								CHDIV_SEG3 = 0;         
								CHDIV_SEG3_EN = 0;			
								CHDIV_SEG_SEL = 1;			
							
								Dividervalue = 2;
						}
						
			else if ( Outputfrequency >= 1184000 && Outputfrequency < 1775000)
						{
								VCO_2X_EN = 0;					
								PLL_N_PRE = 0;					
								CHDIV_DIST_PD = 0;			
								CHDIV_DISTA_EN = 1;			
								VCO_DISTA_PD = 1;				
								OUTA_MUX = 0;						
							
								CHDIV_EN = 1;						
								CHDIV_SEG1 = 1;					
								CHDIV_SEG1_EN = 1;			
								CHDIV_SEG2 = 0;					
								CHDIV_SEG2_EN = 0;			
								CHDIV_SEG3 = 0;         
								CHDIV_SEG3_EN = 0;			
								CHDIV_SEG_SEL = 1;			
							
								Dividervalue = 3;				
						}
						
			else if ( Outputfrequency >= 888000 && Outputfrequency < 1184000 )
						{
								VCO_2X_EN = 0;					
								PLL_N_PRE = 0;					
								CHDIV_DIST_PD = 0;			
								CHDIV_DISTA_EN = 1;			
								VCO_DISTA_PD = 1;				
								OUTA_MUX = 0;						
							
								CHDIV_EN = 1;						
								CHDIV_SEG1 = 0;					
								CHDIV_SEG1_EN = 1;			
								CHDIV_SEG2 = 1;					
								CHDIV_SEG2_EN = 1;			
								CHDIV_SEG3 = 0;         
								CHDIV_SEG3_EN = 0;			
								CHDIV_SEG_SEL = 2;			
								
								Dividervalue = 4;				
						}
						
			else if ( Outputfrequency >= 592000 && Outputfrequency < 888000 )
						{
								VCO_2X_EN = 0;					
								PLL_N_PRE = 0;					
								CHDIV_DIST_PD = 0;			
								CHDIV_DISTA_EN = 1;			
								VCO_DISTA_PD = 1;				
								OUTA_MUX = 0;						
							
								CHDIV_EN = 1;						
								CHDIV_SEG1 = 1;					
								CHDIV_SEG1_EN = 1;			
								CHDIV_SEG2 = 1;					
								CHDIV_SEG2_EN = 1;			
								CHDIV_SEG3 = 0;         
								CHDIV_SEG3_EN = 0;			
								CHDIV_SEG_SEL = 2;			
								
								Dividervalue = 6;				
						}
						
			else if ( Outputfrequency >= 444000 && Outputfrequency < 592000 )
						{
								VCO_2X_EN = 0;					
								PLL_N_PRE = 0;					
								CHDIV_DIST_PD = 0;			
								CHDIV_DISTA_EN = 1;			
								VCO_DISTA_PD = 1;				
								OUTA_MUX = 0;						
							
								CHDIV_EN = 1;						
								CHDIV_SEG1 = 0;					
								CHDIV_SEG1_EN = 1;			
								CHDIV_SEG2 = 2;					
								CHDIV_SEG2_EN = 1;			
								CHDIV_SEG3 = 0;         
								CHDIV_SEG3_EN = 0;			
								CHDIV_SEG_SEL = 2;			
								
								Dividervalue = 8;				
						}
						
			else if ( Outputfrequency >= 296000 && Outputfrequency < 444000 )
						{
								VCO_2X_EN = 0;					
								PLL_N_PRE = 0;					
								CHDIV_DIST_PD = 0;			
								CHDIV_DISTA_EN = 1;			
								VCO_DISTA_PD = 1;				
								OUTA_MUX = 0;						
							
								CHDIV_EN = 1;						
								CHDIV_SEG1 = 1;					
								CHDIV_SEG1_EN = 1;			
								CHDIV_SEG2 = 2;					
								CHDIV_SEG2_EN = 1;			
								CHDIV_SEG3 = 0;         
								CHDIV_SEG3_EN = 0;			
								CHDIV_SEG_SEL = 2;			
								
								Dividervalue = 12;				
						}
						
			else if ( Outputfrequency >= 222000 && Outputfrequency < 296000 )
						{
								VCO_2X_EN = 0;					
								PLL_N_PRE = 0;					
								CHDIV_DIST_PD = 0;			
								CHDIV_DISTA_EN = 1;			
								VCO_DISTA_PD = 1;				
								OUTA_MUX = 0;						
							
								CHDIV_EN = 1;						
								CHDIV_SEG1 = 0;					
								CHDIV_SEG1_EN = 1;			
								CHDIV_SEG2 = 8;					
								CHDIV_SEG2_EN = 1;			
								CHDIV_SEG3 = 0;         
								CHDIV_SEG3_EN = 0;			
								CHDIV_SEG_SEL = 2;			
								
								Dividervalue = 16;				
						}
						
			else if ( Outputfrequency >= 148000 && Outputfrequency < 222000 )
						{
								VCO_2X_EN = 0;					
								PLL_N_PRE = 0;					
								CHDIV_DIST_PD = 0;			
								CHDIV_DISTA_EN = 1;			
								VCO_DISTA_PD = 1;				
								OUTA_MUX = 0;						
							
								CHDIV_EN = 1;						
								CHDIV_SEG1 = 1;					
								CHDIV_SEG1_EN = 1;			
								CHDIV_SEG2 = 8;					
								CHDIV_SEG2_EN = 1;			
								CHDIV_SEG3 = 0;         
								CHDIV_SEG3_EN = 0;			
								CHDIV_SEG_SEL = 2;			
								
								Dividervalue = 24;				
						}
						
			else if ( Outputfrequency >= 111000 && Outputfrequency < 148000 )
						{
								VCO_2X_EN = 0;					
								PLL_N_PRE = 0;					
								CHDIV_DIST_PD = 0;			
								CHDIV_DISTA_EN = 1;			
								VCO_DISTA_PD = 1;				
								OUTA_MUX = 0;						
							
								CHDIV_EN = 1;						
								CHDIV_SEG1 = 0;					
								CHDIV_SEG1_EN = 1;			
								CHDIV_SEG2 = 8;					
								CHDIV_SEG2_EN = 1;			
								CHDIV_SEG3 = 1;         
								CHDIV_SEG3_EN = 1;			
								CHDIV_SEG_SEL = 4;			
								
								Dividervalue = 32;				
						}
						
			else if ( Outputfrequency >= 99000 && Outputfrequency < 111000 )
						{
								VCO_2X_EN = 0;					
								PLL_N_PRE = 0;					
								CHDIV_DIST_PD = 0;			
								CHDIV_DISTA_EN = 1;			
								VCO_DISTA_PD = 1;				
								OUTA_MUX = 0;						
							
								CHDIV_EN = 1;						
								CHDIV_SEG1 = 1;					
								CHDIV_SEG1_EN = 1;			
								CHDIV_SEG2 = 4;					
								CHDIV_SEG2_EN = 1;			
								CHDIV_SEG3 = 1;         
								CHDIV_SEG3_EN = 1;			
								CHDIV_SEG_SEL = 4;			
								
								Dividervalue = 36;				
						}
						
			else if ( Outputfrequency >= 74000 && Outputfrequency < 99000 )
						{
								VCO_2X_EN = 0;					
								PLL_N_PRE = 0;					
								CHDIV_DIST_PD = 0;			
								CHDIV_DISTA_EN = 1;			
								VCO_DISTA_PD = 1;				
								OUTA_MUX = 0;						
							
								CHDIV_EN = 1;						
								CHDIV_SEG1 = 1;					
								CHDIV_SEG1_EN = 1;			
								CHDIV_SEG2 = 8;					
								CHDIV_SEG2_EN = 1;			
								CHDIV_SEG3 = 1;         
								CHDIV_SEG3_EN = 1;			
								CHDIV_SEG_SEL = 4;			
								
								Dividervalue = 48;				
						}
						
			else if ( Outputfrequency >= 56000 && Outputfrequency < 74000 )
						{
								VCO_2X_EN = 0;					
								PLL_N_PRE = 0;					
								CHDIV_DIST_PD = 0;			
								CHDIV_DISTA_EN = 1;			
								VCO_DISTA_PD = 1;				
								OUTA_MUX = 0;						
							
								CHDIV_EN = 1;						
								CHDIV_SEG1 = 0;					
								CHDIV_SEG1_EN = 1;			
								CHDIV_SEG2 = 8;					
								CHDIV_SEG2_EN = 1;			
								CHDIV_SEG3 = 2;         
								CHDIV_SEG3_EN = 1;			
								CHDIV_SEG_SEL = 4;			
								
								Dividervalue = 64;				
						}
						
			else if ( Outputfrequency >= 37000 && Outputfrequency < 56000 )
						{
								VCO_2X_EN = 0;					
								PLL_N_PRE = 0;					
								CHDIV_DIST_PD = 0;			
								CHDIV_DISTA_EN = 1;			
								VCO_DISTA_PD = 1;				
								OUTA_MUX = 0;						
							
								CHDIV_EN = 1;						
								CHDIV_SEG1 = 0;					
								CHDIV_SEG1_EN = 1;			
								CHDIV_SEG2 = 8;					
								CHDIV_SEG2_EN = 1;			
								CHDIV_SEG3 = 4;         
								CHDIV_SEG3_EN = 1;			
								CHDIV_SEG_SEL = 4;			
								
								Dividervalue = 96;				
						}
						
			else if ( Outputfrequency >= 28000 && Outputfrequency < 37000 )
						{
								VCO_2X_EN = 0;					
								PLL_N_PRE = 0;					
								CHDIV_DIST_PD = 0;			
								CHDIV_DISTA_EN = 1;			
								VCO_DISTA_PD = 1;				
								OUTA_MUX = 0;						
							
								CHDIV_EN = 1;						
								CHDIV_SEG1 = 0;					
								CHDIV_SEG1_EN = 1;			
								CHDIV_SEG2 = 8;					
								CHDIV_SEG2_EN = 1;			
								CHDIV_SEG3 = 8;         
								CHDIV_SEG3_EN = 1;			
								CHDIV_SEG_SEL = 4;			
								
								Dividervalue = 128;				
						}
						
			else if ( Outputfrequency >= 20000 && Outputfrequency < 28000 )
						{
								VCO_2X_EN = 0;					
								PLL_N_PRE = 0;					
								CHDIV_DIST_PD = 0;			
								CHDIV_DISTA_EN = 1;			
								VCO_DISTA_PD = 1;				
								OUTA_MUX = 0;						
							
								CHDIV_EN = 1;						
								CHDIV_SEG1 = 1;					
								CHDIV_SEG1_EN = 1;			
								CHDIV_SEG2 = 8;					
								CHDIV_SEG2_EN = 1;			
								CHDIV_SEG3 = 8;         
								CHDIV_SEG3_EN = 1;			
								CHDIV_SEG_SEL = 4;			
								
								Dividervalue = 192;				
						}

/* 计算整数和小数部分参数   */
						
				Fpfd = Refin*(1.0+OSC_2X)*MULT/(PLL_R*Pll_R_PRE);      //求解鉴相器频率

				Resvco = Resolution*Dividervalue;		//计算VCO的通道分辨率
						
				FRE_VCO=Outputfrequency*Dividervalue;  //VCO震荡频率
				
				PLL_DEN = Fpfd*(2.0+2*PLL_N_PRE)/Dividervalue/Resolution;								//求解小数部分的分母

				Int_frc =	FRE_VCO/(Fpfd*(2.0+2*PLL_N_PRE));			//求解整数部分与小数部分之和，并保留

		  	PLL_N = (uint32_t)(FRE_VCO/(Fpfd*(2.0+2*PLL_N_PRE)));		//求解整数部分
						
				Frac_Value=(Int_frc-PLL_N)*PLL_DEN; //求解小数部分的分子
						
				PLL_NUM= Frac_Value;
						
				
				if (!PLL_NUM )						//如果是整数N分频，那么小数部分的分母PLL_DEN取1，小数值取0
				{
						PLL_DEN = 1;
				}
				
				else
				{
						Grecd = Gcd(PLL_DEN, PLL_NUM);				//取最大公约数
						PLL_DEN = PLL_DEN/Grecd;							
						PLL_NUM = PLL_NUM/Grecd;							
				}
				
				if ( !PLL_NUM )
				{
						MASH_EN = 0;
						MASH_ORDER = 0;
					  PFD_DLY = 1;						
				}
				else if ( PLL_N >=11 && PLL_N <16 )
				{
						MASH_EN = 1;
						MASH_ORDER = 1;
						PFD_DLY = 1;
				}
				else if ( PLL_N >=16 && PLL_N <18 )
				{
						MASH_EN = 1;
						MASH_ORDER = 2;
					  PFD_DLY = 2;
				}
				else if ( PLL_N >=18 && PLL_N <30 )
				{
						MASH_EN = 1;
						MASH_ORDER = 3;
					  PFD_DLY = 2;
				}				
				else if ( PLL_N >=30)
				{
						MASH_EN = 1;
						MASH_ORDER = 4;
					  PFD_DLY = 8;
				}				
				
	/********R[30]寄存器相关参数初始化配置*********/

			MASH_DITHER = 0;						

			R[30] = 0x1E0034;						
			R[30] += MASH_DITHER<<10;
			R[30] += VCO_2X_EN;
				
	/********R[31]寄存器相关参数初始化配置*********/

			VCO_DISTB_PD = 1;						
			
			R[31] = 0x1F0001;						
			R[31] += VCO_DISTB_PD<<10;
			R[31] += VCO_DISTA_PD<<9;
			R[31] += CHDIV_DIST_PD<<7;
				
	/********R[34]寄存器相关参数初始化配置*********/

			R[34] = 0x22C3CA;						
			R[34] += CHDIV_EN<<5;

	/********R[35]寄存器相关参数初始化配置*********/
			
			R[35] = 0x230019;							
			R[35] += CHDIV_SEG2<<9;
			R[35] += CHDIV_SEG3_EN<<8;
			R[35] += CHDIV_SEG2_EN<<7;
			R[35] += CHDIV_SEG1<<2;
			R[35] += CHDIV_SEG1_EN<<1;

	/********R[36]寄存器相关参数初始化配置*********/

			CHDIV_DISTB_EN = 0;			

			R[36] = 0x240000;						
			R[36] += CHDIV_DISTB_EN<<11;
			R[36] += CHDIV_DISTA_EN<<10;
			R[36] += CHDIV_SEG_SEL<<4;
			R[36] += CHDIV_SEG3;

	/********R[37]寄存器相关参数初始化配置*********/

			R[37] = 0x254000;						
			R[37] += PLL_N_PRE<<12;

	/********R[38]寄存器相关参数初始化配置*********/

			R[38] = 0x260000;						
			R[38] += PLL_N<<1;

	/********R[39]寄存器相关参数初始化配置*********/

			R[39] = 0x278004;							
			R[39] += PFD_DLY<<8;

	/********R[40]寄存器相关参数初始化配置*********/

			PLL_DEN_H = (uint16_t) (PLL_DEN>>16);   	

			R[40] = 0x280000;						
			R[40] += PLL_DEN_H;
			
	/********R[41]寄存器相关参数初始化配置*********/
			
			PLL_DEN_L = (uint16_t) PLL_DEN;					
			
			R[41] = 0x290000;						
			R[41] += PLL_DEN_L;

	/********R[42]寄存器相关参数初始化配置*********/
		
			MASH_SEED = 0;						//MASH_SEED
			MASH_SEED_H = (uint16_t) (MASH_SEED>>16);					

			R[42] = 0x2A0000;						
			R[42] += MASH_SEED_H;

	/********R[43]寄存器相关参数初始化配置*********/

			MASH_SEED_L = (uint16_t) MASH_SEED;						
			
			R[43] = 0x2B0000;							
			R[43] += MASH_SEED_L;
			
	/********R[44]寄存器相关参数初始化配置*********/

			PLL_NUM_H = (uint16_t) PLL_NUM>>16;				
			
			R[44] = 0x2C0000;							
			R[44] += PLL_NUM_H;
			
	/********R[45]寄存器相关参数初始化配置*********/

			PLL_NUM_L = (uint16_t) PLL_NUM;						
			
			R[45] = 0x2D0000;						
			R[45] += PLL_NUM_L;
		
	/********R[46]寄存器相关参数初始化配置*********/

			OUTA_POW = Power;											
			OUTB_PD = 0;												
			OUTA_PD = 0;												
			
			R[46] = 0x2E0000;						
			R[46] += OUTA_POW<<8;
			R[46] += OUTB_PD<<7;
			R[46] += OUTA_PD<<6;
			R[46] += MASH_EN<<5;
			R[46] += MASH_ORDER;
			
	/********R[47]寄存器相关参数初始化配置*********/	
			
			OUTB_POW = 63;									
			R[47] = 0x2F00C0;							
			R[47] += OUTA_MUX<<11;
			R[47] += OUTB_POW;
			
			
			SendData(R[47]);	
			SendData(R[46]);	
			SendData(R[45]);	
			SendData(R[44]);	
			SendData(R[43]);	
			SendData(R[42]);	
			SendData(R[41]);	
			SendData(R[40]);	
			SendData(R[39]);	
			SendData(R[38]);	
			SendData(R[37]);	
			SendData(R[36]);	
			SendData(R[35]);	
			SendData(R[34]);	
			SendData(R[33]);	
			SendData(R[32]);	
			SendData(R[31]);	
			SendData(R[30]);	
}

//寄存器配置写入函数
void SendDataArray(void)
{
			SendData(R[64]);
			SendData(R[62]);	
			SendData(R[61]);	
			SendData(R[59]);	
			SendData(R[48]);	
//			SendData(R[47]);	
//			SendData(R[46]);	
//			SendData(R[45]);	
//			SendData(R[44]);	
//			SendData(R[43]);	
//			SendData(R[42]);	
//			SendData(R[41]);	
//			SendData(R[40]);	
//			SendData(R[39]);	
//			SendData(R[38]);	
//			SendData(R[37]);	
//			SendData(R[36]);	
//			SendData(R[35]);	
//			SendData(R[34]);	
//			SendData(R[33]);	
//			SendData(R[32]);	
//			SendData(R[31]);	
//			SendData(R[30]);	
			SendData(R[29]);	
			SendData(R[28]);	
			SendData(R[25]);	
			SendData(R[24]);	
			SendData(R[23]);	
			SendData(R[22]);	
			SendData(R[20]);	
			SendData(R[19]);	
			SendData(R[14]);	
			SendData(R[13]);	
			SendData(R[12]);	
			SendData(R[11]);	
			SendData(R[10]);	
			SendData(R[9]);	
			SendData(R[8]);	
			SendData(R[7]);	
			SendData(R[4]);	
			SendData(R[2]);	
			SendData(R[1]);	
			SendData(R[0]);	
}



//SPI模拟数据写入
void SendData(uint32_t a)															/*定义发送数据函数*/
{

		uint32_t  k;			//定义中间变量
    uint8_t i;				//定义循环次数变量
    k = a;						//将控制字的值赋给k
	
		LMX2592_LE=0;
		LMX2592_CLK=1;
          for(i=0;i<24;i++)															//LMX2594控制字为24位3字节，设置循环次数
          {  
								LMX2592_CLK=0;
								if (k&0x800000)												//如果最高位是1，则将SDI引脚置高
								{
										LMX2592_DATA=1;			
								}
								else																		//如果最高位是0，则将SDI引脚置低
								{
										LMX2592_DATA=0;				
								}												
								LMX2592_CLK=1;				//SCK管脚拉高
								k=k<<1;																	//将控制字左移1位，下一个循环写入
	        }
	  LMX2592_LE=1;										//写入完毕，CSB拉高，将控制字装入相应寄存器																	
}


//求解最大公约数
uint32_t Gcd(uint32_t a, uint32_t b)
		{
				uint32_t c = 0;
				while(b!=0)
					{
						c=a%b;
						a=b;
						b=c;
					}
				return a;
		}

//软件数据扫频，非RAMP扫频
void SWEEP_INIT(uint32_t starthz, uint32_t stophz, uint32_t stephz)
{
	uint32_t i = 0;
	if (starthz < stophz) //从低到高
	{

		for (i = 0; i < ((stophz - starthz) / stephz); i++)
		{
			Frequencyfixed(starthz + i * stephz,1000,63);
			delay_ms(10);
		}
	}
	else //从高到低
	{
		for (i = ((starthz - stophz) / stephz); i > 0; i--)
		{
			Frequencyfixed(starthz + i * stephz,1000,63);
			delay_ms(10);
		}
	}
}













