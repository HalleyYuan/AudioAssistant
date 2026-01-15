/**
  ******************************************************************************
  * @file    wm8978.h
  * @brief   WM8978音频编解码器驱动头文件（移植自参考工程）
  * @note    完全按照参考工程的实现顺序移植，适配HAL库和软件IIC
  ******************************************************************************
  */

#ifndef __WM8978_H
#define __WM8978_H

#include "main.h"

// WM8978 I2C地址定义
// 如果AD0引脚(4脚)接地,IIC地址为0X4A(二进制左移一位).
// 如果AD0接V3.3,则IIC地址为0X4B(二进制左移一位).
#define WM8978_ADDR        0x1A    // WM8978的固定I2C地址,固定为0x1A

// EQ1频段截止频率定义
#define EQ1_80Hz           0x00
#define EQ1_105Hz          0x01
#define EQ1_135Hz          0x02
#define EQ1_175Hz          0x03

// EQ2频段中心频率定义
#define EQ2_230Hz          0x00
#define EQ2_300Hz          0x01
#define EQ2_385Hz          0x02
#define EQ2_500Hz          0x03

// EQ3频段中心频率定义
#define EQ3_650Hz          0x00
#define EQ3_850Hz          0x01
#define EQ3_1100Hz         0x02
#define EQ3_1400Hz         0x03

// EQ4频段中心频率定义
#define EQ4_1800Hz         0x00
#define EQ4_2400Hz         0x01
#define EQ4_3200Hz         0x02
#define EQ4_4100Hz         0x03

// EQ5频段截止频率定义
#define EQ5_5300Hz         0x00
#define EQ5_6900Hz         0x01
#define EQ5_9000Hz         0x02
#define EQ5_11700Hz        0x03

// 函数声明
uint8_t WM8978_Init(void);                                    // WM8978初始化
void WM8978_ADDA_Cfg(uint8_t dacen, uint8_t adcen);          // WM8978 DAC/ADC配置
void WM8978_Input_Cfg(uint8_t micen, uint8_t lineinen, uint8_t auxen);  // WM8978输入通道配置
void WM8978_Output_Cfg(uint8_t dacen, uint8_t bpsen);        // WM8978输出配置
void WM8978_MIC_Gain(uint8_t gain);                          // WM8978麦克风增益设置
void WM8978_LINEIN_Gain(uint8_t gain);                       // WM8978 LINE IN增益设置
void WM8978_AUX_Gain(uint8_t gain);                          // WM8978 AUX增益设置
uint8_t WM8978_Write_Reg(uint8_t reg, uint16_t val);         // WM8978写寄存器
uint16_t WM8978_Read_Reg(uint8_t reg);                       // WM8978读寄存器
void WM8978_HPvol_Set(uint8_t voll, uint8_t volr);           // 设置耳机音量
void WM8978_SPKvol_Set(uint8_t volx);                        // 设置扬声器音量
void WM8978_I2S_Cfg(uint8_t fmt, uint8_t len);               // 配置I2S接口模式
void WM8978_3D_Set(uint8_t depth);                           // 设置3D增强
void WM8978_EQ_3D_Dir(uint8_t dir);                          // 设置EQ/3D应用方向
void WM8978_EQ1_Set(uint8_t cfreq, uint8_t gain);            // 设置EQ1
void WM8978_EQ2_Set(uint8_t cfreq, uint8_t gain);            // 设置EQ2
void WM8978_EQ3_Set(uint8_t cfreq, uint8_t gain);            // 设置EQ3
void WM8978_EQ4_Set(uint8_t cfreq, uint8_t gain);            // 设置EQ4
void WM8978_EQ5_Set(uint8_t cfreq, uint8_t gain);            // 设置EQ5

#endif /* __WM8978_H */
