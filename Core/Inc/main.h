/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
#define CCMRAM __attribute__((section("ccmram")))
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LCD_CS_Pin GPIO_PIN_6
#define LCD_CS_GPIO_Port GPIOE
#define LED_RUN_Pin GPIO_PIN_9
#define LED_RUN_GPIO_Port GPIOF
#define LCD_RS_Pin GPIO_PIN_0
#define LCD_RS_GPIO_Port GPIOC
#define LCD_RST_Pin GPIO_PIN_1
#define LCD_RST_GPIO_Port GPIOC
#define LED2_Pin GPIO_PIN_6
#define LED2_GPIO_Port GPIOA
#define LED3_Pin GPIO_PIN_7
#define LED3_GPIO_Port GPIOA
#define LCD_BL_Pin GPIO_PIN_13
#define LCD_BL_GPIO_Port GPIOB
#define BT_POWER_Pin GPIO_PIN_7
#define BT_POWER_GPIO_Port GPIOC
#define SD_CS_Pin GPIO_PIN_7
#define SD_CS_GPIO_Port GPIOD
#define CTP_INT_Pin GPIO_PIN_6
#define CTP_INT_GPIO_Port GPIOB
#define CTP_INT_EXTI_IRQn EXTI9_5_IRQn
#define CTP_RST_Pin GPIO_PIN_7
#define CTP_RST_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#define LED_RUN_Pin GPIO_PIN_9
#define LED_RUN_GPIO_Port GPIOF
/* 蓝牙电源控制GPIO定义 - 使用PC4（由CubeMX配置生成） */
/* 注意：如果CubeMX已配置PC4，会自动生成BT_POWER_Pin和BT_POWER_GPIO_Port定义 */
/* 如果CubeMX未配置，请取消下面的注释并手动定义 */
// #define BT_POWER_Pin GPIO_PIN_4
// #define BT_POWER_GPIO_Port GPIOC
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
