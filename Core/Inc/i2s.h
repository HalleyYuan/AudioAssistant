/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    i2s.h
  * @brief   This file contains all the function prototypes for
  *          the i2s.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __I2S_H__
#define __I2S_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern I2S_HandleTypeDef hi2s2;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_I2S2_Init(void);

/* USER CODE BEGIN Prototypes */

// I2S标准库兼容函数
void I2S2_Init(uint16_t I2S_Standard, uint16_t I2S_Mode, uint16_t I2S_Clock_Polarity, uint16_t I2S_DataFormat);
void I2S2ext_Init(uint16_t I2S_Standard, uint16_t I2S_Mode, uint16_t I2S_Clock_Polarity, uint16_t I2S_DataFormat);  // I2S2ext初始化
uint8_t I2S2_SampleRate_Set(uint32_t samplerate);
void I2S2_TX_DMA_Init(uint8_t* buf0, uint8_t *buf1, uint16_t num);
void I2S2ext_RX_DMA_Init(uint8_t* buf0, uint8_t *buf1, uint16_t num);  // I2S2ext RX DMA初始化（双缓冲）
void I2S_Play_Start(void);
void I2S_Play_Stop(void);
void I2S_Rec_Start(void);   // 启动I2S2ext接收
void I2S_Rec_Stop(void);    // 停止I2S2ext接收

// I2S DMA回调函数指针
extern void (*i2s_tx_callback)(void);
extern void (*i2s_rx_callback)(void);

// I2S标准库常量定义
#define I2S_Standard_Phillips       0x00
#define I2S_Mode_SlaveRx            0x01
#define I2S_Mode_MasterTx           0x02
#define I2S_Mode_SlaveTx            0x03
#define I2S_Mode_MasterRx           0x04
#define I2S_CPOL_Low                0x00
#define I2S_DataFormat_16b          0x00  // 标准16bit
#define I2S_DataFormat_16bextended  0x01  // 16bit扩展格式
#define I2S_DataFormat_24b          0x03  // 24bit格式
#define I2S_DataFormat_32b          0x05  // 32bit格式

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __I2S_H__ */

