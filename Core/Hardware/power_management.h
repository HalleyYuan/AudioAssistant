#ifndef __POWER_MANAGEMENT_H__
#define __POWER_MANAGEMENT_H__

#include "main.h"
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 电源管理状态枚举 */
typedef enum {
    POWER_STATE_NORMAL,        ///< 正常运行状态
    POWER_STATE_SCREEN_OFF,     ///< 息屏准备状态
    POWER_STATE_STOP,           ///< STOP低功耗模式
    POWER_STATE_WAKING_UP       ///< 唤醒恢复中
} power_state_t;

/* 唤醒源类型 */
typedef enum {
    WAKEUP_SOURCE_NONE,        ///< 无唤醒源
    WAKEUP_SOURCE_TOUCH,       ///< 触摸屏唤醒
    WAKEUP_SOURCE_RTC          ///< RTC唤醒
} wakeup_source_t;

/**
 * @brief  初始化电源管理模块
 * @retval None
 * @note   初始化RTC和触摸中断配置
 */
void Power_Init(void);

/**
 * @brief  进入息屏低功耗模式
 * @retval None
 * @note   保存状态、关闭外设、配置唤醒源、进入STOP模式
 */
void Power_EnterScreenOff(void);

/**
 * @brief  唤醒恢复
 * @retval None
 * @note   恢复系统时钟、外设、UI状态
 */
void Power_WakeUp(void);

/**
 * @brief  获取当前电源状态
 * @retval 当前电源状态
 */
power_state_t Power_GetState(void);

/**
 * @brief  触摸中断处理函数（在EXTI中断中调用）
 * @retval None
 * @note   检测触摸按下，记录时间戳
 */
void Power_TouchInterruptHandler(void);

/**
 * @brief  触摸扫描任务（在主循环中调用）
 * @retval None
 * @note   检测触摸长按3秒，触发唤醒
 */
void Power_TouchScanTask(void);

/**
 * @brief  RTC唤醒中断回调函数
 * @param  hrtc: RTC句柄指针
 * @retval None
 * @note   在HAL_RTCEx_WakeUpTimerEventCallback中调用
 */
void Power_RTCWakeupCallback(RTC_HandleTypeDef *hrtc);

/**
 * @brief  配置RTC唤醒定时器
 * @param  duration_ms: 唤醒时间（毫秒）
 * @retval None
 */
void Power_ConfigRTCWakeup(uint32_t duration_ms);

/**
 * @brief  获取触摸按下时长（毫秒）
 * @retval 触摸按下时长（毫秒），如果未按下则返回0
 */
uint32_t Power_GetTouchPressDuration(void);

/**
 * @brief  检查是否应该进入STOP模式
 * @retval 1: 应该进入STOP模式，0: 不应该进入
 * @note   检查触摸状态，如果触摸已按下，等待3秒后再进入STOP
 */
uint8_t Power_ShouldEnterStop(void);

#ifdef __cplusplus
}
#endif

#endif /* __POWER_MANAGEMENT_H__ */
