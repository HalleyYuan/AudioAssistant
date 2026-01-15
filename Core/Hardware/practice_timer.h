#ifndef __PRACTICE_TIMER_H
#define __PRACTICE_TIMER_H

#include "main.h"
#include "lvgl.h"

/* 计时器模式 */
typedef enum {
    TIMER_MODE_COUNT_UP,      // 正计时模式
    TIMER_MODE_COUNT_DOWN     // 倒计时模式
} timer_mode_t;

/* 计时器状态 */
typedef enum {
    TIMER_STATE_IDLE,         // 空闲状态
    TIMER_STATE_RUNNING,      // 运行中
    TIMER_STATE_PAUSED        // 暂停
} timer_state_t;

/* 计时器数据结构 */
typedef struct {
    timer_mode_t mode;         // 当前模式
    timer_state_t state;       // 当前状态
    uint32_t elapsed_time;     // 已过时间（秒，正计时用）
    uint32_t target_time;      // 目标时间（秒，倒计时用）
    uint32_t remaining_time;   // 剩余时间（秒，倒计时用）
    uint8_t hours;             // 设置的小时（倒计时用）
    uint8_t minutes;           // 设置的分钟（倒计时用）
    uint8_t seconds;          // 设置的秒数（倒计时用）
    uint32_t last_tick;        // 上次更新的tick值
} practice_timer_t;

/* 初始化Practice计时器页面 */
void Practice_Timer_Init(void);

/* 更新计时器显示（在主循环中调用） */
void Practice_Timer_Update(void);

/* 获取计时器状态 */
timer_state_t Practice_Timer_GetState(void);

/* 获取计时器模式 */
timer_mode_t Practice_Timer_GetMode(void);

#endif /* __PRACTICE_TIMER_H */
