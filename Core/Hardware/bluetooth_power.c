/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    bluetooth_power.c
  * @brief   蓝牙模块电源控制实现
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

/* Includes ------------------------------------------------------------------*/
#include "bluetooth_power.h"
#include "main.h"

/* Private defines -----------------------------------------------------------*/
/* 蓝牙电源控制GPIO定义 - 强制使用PC7 */
/* 注意：代码中直接使用PC7，忽略CubeMX配置（即使CubeMX配置的是PC4也使用PC7） */
#define BT_POWER_GPIO_PORT    GPIOC
#define BT_POWER_GPIO_PIN     GPIO_PIN_7

/* Private variables ---------------------------------------------------------*/
static bt_power_state_t bt_power_state = BT_POWER_OFF;  /* 当前电源状态 */

/* Private function prototypes -----------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  初始化蓝牙电源控制GPIO
  * @retval None
  * @note   默认状态为断电（低电平）
  * @note   如果CubeMX未配置PC7，这里会强制配置为输出模式
  */
void BT_Power_Init(void)
{
    /* 确保GPIO已配置为输出模式（即使CubeMX未配置也能工作） */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = BT_POWER_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BT_POWER_GPIO_PORT, &GPIO_InitStruct);
    
    /* 设置默认状态为断电（低电平） */
    HAL_GPIO_WritePin(BT_POWER_GPIO_PORT, BT_POWER_GPIO_PIN, GPIO_PIN_RESET);
    bt_power_state = BT_POWER_OFF;
}

/**
  * @brief  打开蓝牙模块电源
  * @retval None
  * @note   GPIO输出高电平(3.3V)，通过三极管驱动电路给蓝牙模块供电
  * @note   电路分析：PC7高电平 → Q1导通 → Q2截止 → VCC_BLUE = +5V
  */
void BT_Power_On(void)
{
    /* 确保GPIOC时钟已使能 */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    
    /* 确保GPIO已配置为输出模式 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = BT_POWER_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BT_POWER_GPIO_PORT, &GPIO_InitStruct);
    
    /* 输出高电平 */
    HAL_GPIO_WritePin(BT_POWER_GPIO_PORT, BT_POWER_GPIO_PIN, GPIO_PIN_SET);
    bt_power_state = BT_POWER_ON;
    
    /* 等待电路稳定 */
    HAL_Delay(50);  // 增加延时，确保蓝牙模块完全上电
}

/**
  * @brief  关闭蓝牙模块电源
  * @retval None
  * @note   GPIO输出低电平(0V)，切断蓝牙模块供电
  */
void BT_Power_Off(void)
{
    HAL_GPIO_WritePin(BT_POWER_GPIO_PORT, BT_POWER_GPIO_PIN, GPIO_PIN_RESET);
    bt_power_state = BT_POWER_OFF;
}

/**
  * @brief  切换蓝牙模块电源状态
  * @retval None
  * @note   如果当前是开启状态则关闭，如果当前是关闭状态则开启
  */
void BT_Power_Toggle(void)
{
    if(bt_power_state == BT_POWER_ON) {
        BT_Power_Off();
    } else {
        BT_Power_On();
    }
}

/**
  * @brief  获取蓝牙模块电源状态
  * @retval bt_power_state_t 电源状态（BT_POWER_ON 或 BT_POWER_OFF）
  */
bt_power_state_t BT_Power_GetState(void)
{
    return bt_power_state;
}

/**
  * @brief  设置蓝牙模块电源状态
  * @param  state: 电源状态（BT_POWER_ON 或 BT_POWER_OFF）
  * @retval None
  */
void BT_Power_SetState(bt_power_state_t state)
{
    if(state == BT_POWER_ON) {
        BT_Power_On();
    } else {
        BT_Power_Off();
    }
}
