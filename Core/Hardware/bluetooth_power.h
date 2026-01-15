/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    bluetooth_power.h
  * @brief   蓝牙模块电源控制头文件
  *          通过GPIO控制三极管驱动电路，实现蓝牙模块5V供电的开关控制
  ******************************************************************************
  * @attention
  *
  * 硬件连接说明：
  * - GPIO输出3.3V -> 三极管基极 -> 三极管驱动5V电源给蓝牙模块供电
  * - GPIO输出高电平(3.3V)：蓝牙模块上电
  * - GPIO输出低电平(0V)：蓝牙模块断电
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __BLUETOOTH_POWER_H
#define __BLUETOOTH_POWER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f4xx_hal.h"

/* Exported types ------------------------------------------------------------*/
/**
  * @brief  蓝牙电源状态枚举
  */
typedef enum {
    BT_POWER_OFF = 0,  /* 蓝牙模块断电 */
    BT_POWER_ON  = 1   /* 蓝牙模块上电 */
} bt_power_state_t;

/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/

/**
  * @brief  初始化蓝牙电源控制GPIO
  * @retval None
  * @note   默认状态为断电（低电平）
  */
void BT_Power_Init(void);

/**
  * @brief  打开蓝牙模块电源
  * @retval None
  * @note   GPIO输出高电平(3.3V)，通过三极管驱动电路给蓝牙模块供电
  */
void BT_Power_On(void);

/**
  * @brief  关闭蓝牙模块电源
  * @retval None
  * @note   GPIO输出低电平(0V)，切断蓝牙模块供电
  */
void BT_Power_Off(void);

/**
  * @brief  切换蓝牙模块电源状态
  * @retval None
  * @note   如果当前是开启状态则关闭，如果当前是关闭状态则开启
  */
void BT_Power_Toggle(void);

/**
  * @brief  获取蓝牙模块电源状态
  * @retval bt_power_state_t 电源状态（BT_POWER_ON 或 BT_POWER_OFF）
  */
bt_power_state_t BT_Power_GetState(void);

/**
  * @brief  设置蓝牙模块电源状态
  * @param  state: 电源状态（BT_POWER_ON 或 BT_POWER_OFF）
  * @retval None
  */
void BT_Power_SetState(bt_power_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* __BLUETOOTH_POWER_H */
