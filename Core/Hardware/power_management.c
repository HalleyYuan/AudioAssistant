#include "power_management.h"
#include "touch.h"
#include "LCD.h"
#include "audio_play.h"
#include "recorder.h"
#include "metronome.h"
#include "music_assistant.h"
#include "wm8978.h"
#include "bluetooth_power.h"
#include "main.h"
#include "lvgl.h"  // LVGL库
#include <string.h>

/* 前向声明 */
void SystemClock_Config(void);

/* 电源管理状态 */
static volatile power_state_t power_state = POWER_STATE_NORMAL;

/* 触摸按下时间戳（毫秒） */
static volatile uint32_t touch_press_start_time = 0;

/* 触摸按下标志 */
static volatile uint8_t touch_pressed = 0;

/* 进入STOP模式的时间戳 */
// static uint32_t stop_entry_time = 0;  // 暂时未使用

/* 唤醒源 */
// static wakeup_source_t wakeup_source = WAKEUP_SOURCE_NONE;  // 暂时未使用

/* RTC句柄（需要在外部定义，如果未配置RTC则使用条件编译） */
#ifdef HAL_RTC_MODULE_ENABLED
extern RTC_HandleTypeDef hrtc;
#endif

/* 保存的页面状态 */
static page_t saved_page = PAGE_MAIN_MENU;

/**
 * @brief  初始化电源管理模块
 * @retval None
 */
void Power_Init(void)
{
    power_state = POWER_STATE_NORMAL;
    touch_press_start_time = 0;
    touch_pressed = 0;
    // wakeup_source = WAKEUP_SOURCE_NONE;
}

/**
 * @brief  保存系统状态
 * @retval None
 */
static void Power_SaveState(void)
{
    extern page_t current_page;
    saved_page = current_page;
    
    // 其他状态保存（如果需要）
    // 例如：播放位置、节拍器状态等
}

/**
 * @brief  关闭外设
 * @retval None
 */
static void Power_DisablePeripherals(void)
{
    // 停止音频播放
    extern void Audio_Stop(void);
    Audio_Stop();
    
    // 停止录音（如果正在录音）
    extern recorder_state_t Recorder_GetState(void);
    if(Recorder_GetState() == RECORDER_STATE_RECORDING)
    {
        extern void Recorder_Stop(void);
        Recorder_Stop();
    }
    
    // 停止节拍器（如果正在运行）
    if(Metronome_IsPlaying())
    {
        Metronome_Stop();
    }
    
    // 关闭LCD背光（PB13）
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_RESET);
    
    // WM8978可以进入低功耗模式（可选）
    // WM8978_Sleep();
}

/**
 * @brief  恢复外设
 * @retval None
 */
static void Power_EnablePeripherals(void)
{
    // 恢复系统时钟（HAL库会自动处理，但确保调用）
    SystemClock_Config();
    
    // 开启LCD背光（PB13）
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET);
    
    // 重新初始化LCD（如果需要）
    // LCD_Init();
    
    // 恢复WM8978（如果之前关闭了）
    // WM8978_WakeUp();
}

/**
 * @brief  配置触摸中断为唤醒源
 * @retval None
 */
static void Power_ConfigTouchWakeup(void)
{
    // 触摸中断已经在gpio.c中配置为外部中断
    // 确保中断优先级足够高
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

/**
 * @brief  配置RTC唤醒定时器
 * @param  duration_ms: 唤醒时间（毫秒）
 * @retval None
 */
void Power_ConfigRTCWakeup(uint32_t duration_ms)
{
#ifdef HAL_RTC_MODULE_ENABLED
    if(duration_ms >= 1000)
    {
        // 大于等于1秒，使用1Hz时钟
        uint32_t seconds = duration_ms / 1000;
        if(seconds > 65535) seconds = 65535;
        
        HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, seconds - 1, RTC_WAKEUPCLOCK_CK_SPRE_16BITS);
    }
    else
    {
        // 小于1秒，使用2048Hz时钟
        uint32_t ticks = (duration_ms * 2048) / 1000;
        if(ticks > 65535) ticks = 65535;
        
        HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, ticks - 1, RTC_WAKEUPCLOCK_RTCCLK_DIV16);
    }
#else
    // RTC未启用，使用软件延时（不推荐，仅用于测试）
    (void)duration_ms;
#endif
}

/**
 * @brief  进入息屏低功耗模式
 * @retval None
 */
void Power_EnterScreenOff(void)
{
    // 1. 保存状态
    Power_SaveState();
    
    // 2. 关闭外设
    Power_DisablePeripherals();
    
    // 3. 清空屏幕，设置黑色背景
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
    
    // 4. 更新状态
    power_state = POWER_STATE_SCREEN_OFF;
    
    // 5. 检测触摸状态
    TP_Scan_Task();  // 扫描一次触摸，更新tp_dev状态
    
    if(tp_dev.sta & TP_PRES_DOWN)
    {
        // 触摸已按下，记录时间戳
        touch_press_start_time = HAL_GetTick();
        touch_pressed = 1;
        
        // 配置RTC唤醒定时器为短间隔（100ms），用于检测触摸时长
        Power_ConfigRTCWakeup(100);
        power_state = POWER_STATE_WAKING_UP;  // 等待触摸长按
    }
    else
    {
        // 触摸未按下，配置RTC唤醒定时器为3秒
        Power_ConfigRTCWakeup(3000);
        touch_pressed = 0;
    }
    
    // 6. 配置触摸中断为唤醒源
    Power_ConfigTouchWakeup();
    
    // 7. 进入STOP模式
    power_state = POWER_STATE_STOP;
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
    
    // 9. 唤醒后，系统会从这里继续执行
    // 恢复系统时钟（HAL库会自动调用SystemClock_Config，但确保调用）
    SystemClock_Config();
}

/**
 * @brief  唤醒恢复
 * @retval None
 */
void Power_WakeUp(void)
{
    // 1. 恢复外设
    Power_EnablePeripherals();
    
    // 2. 清除触摸状态
    touch_pressed = 0;
    touch_press_start_time = 0;
    
    // 3. 恢复UI状态
    extern page_t current_page;
    current_page = saved_page;
    
    // 根据保存的页面恢复UI（现阶段统一回主菜单）
    extern void music_assistant_init(void);
    extern void music_assistant_show_password_unlock(void);
    music_assistant_init();
    // 如果开启了密码保护，唤醒后先进入解锁界面
    music_assistant_show_password_unlock();
    
    // 4. 刷新屏幕
    lv_refr_now(NULL);
    
    // 5. 更新状态
    power_state = POWER_STATE_NORMAL;
    // wakeup_source = WAKEUP_SOURCE_NONE;
}

/**
 * @brief  获取当前电源状态
 * @retval 当前电源状态
 */
power_state_t Power_GetState(void)
{
    return power_state;
}

/**
 * @brief  触摸中断处理函数（在EXTI中断中调用）
 * @retval None
 */
void Power_TouchInterruptHandler(void)
{
    // 触摸中断已经在touch.c的HAL_GPIO_EXTI_Callback中处理
    // 这里只需要设置标志，实际扫描在主循环中进行
}

/**
 * @brief  触摸扫描任务（在主循环中调用）
 * @retval None
 */
void Power_TouchScanTask(void)
{
    // 只在息屏状态下处理触摸长按检测
    if(power_state != POWER_STATE_SCREEN_OFF && 
       power_state != POWER_STATE_STOP &&
       power_state != POWER_STATE_WAKING_UP)
    {
        return;
    }
    
    // 扫描触摸
    TP_Scan_Task();
    
    // 检测触摸状态
    if(tp_dev.sta & TP_PRES_DOWN)
    {
        if(!touch_pressed)
        {
            // 触摸刚按下，记录时间戳
            touch_press_start_time = HAL_GetTick();
            touch_pressed = 1;
        }
        else
        {
            // 触摸持续按下，检测是否达到3秒
            uint32_t press_duration = HAL_GetTick() - touch_press_start_time;
            
            if(press_duration >= 3000)
            {
                // 达到3秒，执行唤醒
                // wakeup_source = WAKEUP_SOURCE_TOUCH;
                Power_WakeUp();
            }
        }
    }
    else
    {
        // 触摸释放
        if(touch_pressed)
        {
            touch_pressed = 0;
            touch_press_start_time = 0;
            
            // 如果正在等待长按，重新配置RTC唤醒
            if(power_state == POWER_STATE_WAKING_UP)
            {
                Power_ConfigRTCWakeup(3000);
                power_state = POWER_STATE_SCREEN_OFF;
            }
        }
    }
}

/**
 * @brief  RTC唤醒中断回调函数
 * @param  hrtc: RTC句柄指针
 * @retval None
 */
void Power_RTCWakeupCallback(RTC_HandleTypeDef *hrtc)
{
    (void)hrtc;  // 避免未使用参数警告
    
#ifdef HAL_RTC_MODULE_ENABLED
    // 清除RTC唤醒标志
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
    
    // 恢复系统时钟
    SystemClock_Config();
    
    // 判断唤醒源
    if(power_state == POWER_STATE_WAKING_UP)
    {
        // 短间隔唤醒（100ms），检测触摸时长
        if(touch_pressed)
        {
            uint32_t press_duration = HAL_GetTick() - touch_press_start_time;
            
            if(press_duration >= 3000)
            {
                // 达到3秒，执行唤醒
                // wakeup_source = WAKEUP_SOURCE_TOUCH;
                Power_WakeUp();
                return;
            }
            else
            {
                // 未达到3秒，继续等待
                Power_ConfigRTCWakeup(100);
                HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
                return;
            }
        }
        else
        {
            // 触摸已释放，重新配置为3秒唤醒
            Power_ConfigRTCWakeup(3000);
            power_state = POWER_STATE_SCREEN_OFF;
            HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
            return;
        }
    }
    else if(power_state == POWER_STATE_SCREEN_OFF || power_state == POWER_STATE_STOP)
    {
        // 3秒唤醒，检测触摸状态
        TP_Scan_Task();  // 扫描一次触摸
        
        if(tp_dev.sta & TP_PRES_DOWN)
        {
            // 触摸仍按下，执行唤醒
            // wakeup_source = WAKEUP_SOURCE_RTC;
            Power_WakeUp();
        }
        else
        {
            // 触摸未按下，重新进入STOP模式
            Power_ConfigRTCWakeup(3000);
            HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
        }
    }
#else
    // RTC未启用，无法使用RTC唤醒
    (void)hrtc;
#endif
}

/**
 * @brief  获取触摸按下时长（毫秒）
 * @retval 触摸按下时长（毫秒），如果未按下则返回0
 */
uint32_t Power_GetTouchPressDuration(void)
{
    if(!touch_pressed)
    {
        return 0;
    }
    
    return HAL_GetTick() - touch_press_start_time;
}

/**
 * @brief  检查是否应该进入STOP模式
 * @retval 1: 应该进入STOP模式，0: 不应该进入
 */
uint8_t Power_ShouldEnterStop(void)
{
    // 如果触摸已按下，等待3秒后再进入STOP
    if(touch_pressed)
    {
        uint32_t press_duration = HAL_GetTick() - touch_press_start_time;
        if(press_duration < 3000)
        {
            return 0;  // 未达到3秒，不进入STOP
        }
    }
    
    return 1;  // 可以进入STOP模式
}
