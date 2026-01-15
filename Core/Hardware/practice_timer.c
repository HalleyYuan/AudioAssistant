/**
  ******************************************************************************
  * @file    practice_timer.c
  * @brief   Practice计时器功能实现
  * @author  项目开发者
  * @date    2025-01-xx
  * @note    实现正计时和倒计时功能
  ******************************************************************************
  */

#include "practice_timer.h"
#include "music_assistant.h"
#include "debug_uart.h"
#include "metronome_audio.h"
#include "wm8978.h"
#include "i2s.h"
#include "audio_play.h"
#include "lvgl.h"
#include "lvgl/src/widgets/lv_btnmatrix.h"
#include <stdio.h>
#include <string.h>

/* 计时器状态 */
static practice_timer_t timer = {
    .mode = TIMER_MODE_COUNT_UP,
    .state = TIMER_STATE_IDLE,
    .elapsed_time = 0,
    .target_time = 0,
    .remaining_time = 0,
    .hours = 0,
    .minutes = 0,
    .seconds = 0,
    .last_tick = 0
};

/* UI控件指针 */
static lv_obj_t *timer_page = NULL;
static lv_obj_t *mode_btn_countup = NULL;      // 正计时按钮
static lv_obj_t *mode_btn_countdown = NULL;    // 倒计时按钮
static lv_obj_t *time_label = NULL;            // 时间显示标签
static lv_obj_t *hour_roller = NULL;           // 小时选择器（倒计时用）
static lv_obj_t *minute_roller = NULL;         // 分钟选择器（倒计时用）
static lv_obj_t *second_roller = NULL;         // 秒数选择器（倒计时用）
static lv_obj_t *colon1_label = NULL;          // 第一个冒号标签（倒计时用）
static lv_obj_t *colon2_label = NULL;          // 第二个冒号标签（倒计时用）
static lv_obj_t *start_pause_btn = NULL;       // 开始/暂停按钮
static lv_obj_t *start_pause_label = NULL;     // 开始/暂停按钮标签
static lv_obj_t *stop_btn = NULL;             // 停止按钮

/* 私有函数声明 */
static void create_timer_page(void);
static void mode_btn_countup_cb(lv_event_t *e);
static void mode_btn_countdown_cb(lv_event_t *e);
static void start_pause_btn_cb(lv_event_t *e);
static void stop_btn_cb(lv_event_t *e);
static void update_time_display(void);
static void update_button_states(void);
static void format_time_string(uint32_t seconds, char *buf, size_t buf_size);
static void timer_complete_callback(void);
static void msgbox_confirm_btn_cb(lv_event_t *e);
static void beep_timer_cb(lv_timer_t *t);
static void init_audio_system_for_beep(void);

// 静态选项字符串常量（与小时字符串格式完全一致）
static const char hour_options_str[] = "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23";

// 分钟和秒数选项字符串（静态字符串，单行格式，与小时字符串格式完全一致）
static const char minute_second_options_str[] = "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59";

// 静态变量：保存完成消息框指针
static lv_obj_t *complete_msgbox = NULL;

// 静态变量：保存"嘀嘀嘀"声定时器指针
static lv_timer_t *beep_timer = NULL;

/**
 * @brief  初始化音频系统用于播放"嘀嘀嘀"声
 * @note   参考节拍器启动时的完整配置流程
 */
static void init_audio_system_for_beep(void)
{
    // 确保WM8978输出已启用（参考节拍器代码）
    WM8978_ADDA_Cfg(1, 0);      // 使能DAC，禁用ADC
    WM8978_Input_Cfg(0, 0, 0);  // 关闭所有输入通道
    WM8978_Output_Cfg(1, 0);    // 使能DAC输出，关闭BYPASS
    
    // 配置I2S格式（节拍器使用16bit音频）
    WM8978_I2S_Cfg(2, 0);  // I2S格式，16bit
    I2S2_Init(I2S_Standard_Phillips, I2S_Mode_MasterTx,
             I2S_CPOL_Low, I2S_DataFormat_16bextended);  // 16bit扩展格式
    I2S2_SampleRate_Set(44100);  // 44.1KHz采样率
    
    // 设置音量（使用播放器的音量设置，如果播放器未初始化则使用默认值63）
    uint8_t volume = (audio_player.volume > 0) ? audio_player.volume : 63;
    WM8978_HPvol_Set(volume, volume);
    WM8978_SPKvol_Set(volume);
    
    HAL_Delay(10);  // 等待配置稳定
    
    // 初始化节拍音频模块
    Metronome_Audio_Init();
    
    LOGI("[Timer] Audio system initialized for beep\r\n");
}

/**
 * @brief  "嘀嘀嘀"声定时器回调（定期播放声音）
 */
static void beep_timer_cb(lv_timer_t *t)
{
    LV_UNUSED(t);
    
    // 播放中等强度的"嘀"声（参考节拍器代码）
    // 使用 METRONOME_STRENGTH_MEDIUM (2) 作为固定频率
    Metronome_Audio_PlayBeat(METRONOME_STRENGTH_MEDIUM);
}

/**
 * @brief  确认按钮回调（关闭完成消息框并恢复倒计时设置界面）
 */
static void msgbox_confirm_btn_cb(lv_event_t *e)
{
    // 获取按钮矩阵对象
    lv_obj_t *btnm = lv_event_get_target(e);
    
    // 获取选中的按钮索引
    uint16_t btn_id = lv_btnmatrix_get_selected_btn(btnm);
    if(btn_id == LV_BTNMATRIX_BTN_NONE)
        return;
    
    // 停止"嘀嘀嘀"声
    if(beep_timer != NULL)
    {
        lv_timer_del(beep_timer);
        beep_timer = NULL;
    }
    // 停止音频播放
    Metronome_Audio_Stop();
    
    // 使用 lv_msgbox_close() 正确删除消息框（包括可能的父容器和遮罩层）
    if(complete_msgbox != NULL && lv_obj_is_valid(complete_msgbox))
    {
        lv_msgbox_close(complete_msgbox);
        complete_msgbox = NULL;
    }
    
    // 确保是倒计时模式
    if(timer.mode == TIMER_MODE_COUNT_DOWN)
    {
        // 重置倒计时时间为设置的时间
        timer.remaining_time = timer.target_time;
        timer.state = TIMER_STATE_IDLE;
        
        // 恢复倒计时设置界面：显示选择器，隐藏时间标签
        if(hour_roller != NULL)
            lv_obj_clear_flag(hour_roller, LV_OBJ_FLAG_HIDDEN);
        if(minute_roller != NULL)
            lv_obj_clear_flag(minute_roller, LV_OBJ_FLAG_HIDDEN);
        if(second_roller != NULL)
            lv_obj_clear_flag(second_roller, LV_OBJ_FLAG_HIDDEN);
        if(colon1_label != NULL)
            lv_obj_clear_flag(colon1_label, LV_OBJ_FLAG_HIDDEN);
        if(colon2_label != NULL)
            lv_obj_clear_flag(colon2_label, LV_OBJ_FLAG_HIDDEN);
        if(time_label != NULL)
            lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
        
        // 更新按钮状态
        update_button_states();
        
        LOGI("[Timer] Returned to countdown setup interface\r\n");
    }
}

/**
 * @brief  格式化时间为 HH:MM:SS 字符串
 * @param  seconds: 总秒数
 * @param  buf: 输出缓冲区
 * @param  buf_size: 缓冲区大小
 */
static void format_time_string(uint32_t seconds, char *buf, size_t buf_size)
{
    uint32_t hours = seconds / 3600;
    uint32_t minutes = (seconds % 3600) / 60;
    uint32_t secs = seconds % 60;
    
    snprintf(buf, buf_size, "%02lu:%02lu:%02lu", hours, minutes, secs);
}

/**
 * @brief  更新时间显示
 */
static void update_time_display(void)
{
    char time_str[16];
    
    if(timer.mode == TIMER_MODE_COUNT_UP)
    {
        // 正计时模式：显示已过时间
        format_time_string(timer.elapsed_time, time_str, sizeof(time_str));
    }
    else
    {
        // 倒计时模式：显示剩余时间
        format_time_string(timer.remaining_time, time_str, sizeof(time_str));
    }
    
    if(time_label != NULL)
    {
        lv_label_set_text(time_label, time_str);
    }
}

/**
 * @brief  更新按钮状态
 */
static void update_button_states(void)
{
    if(start_pause_label == NULL || stop_btn == NULL)
        return;
    
    // 更新开始/暂停按钮文字和颜色
    if(timer.state == TIMER_STATE_IDLE)
    {
        lv_label_set_text(start_pause_label, "Start");
        lv_obj_clear_state(start_pause_btn, LV_STATE_DISABLED);
        // 绿色（Start）
        lv_obj_set_style_bg_color(start_pause_btn, lv_color_hex(0x00AA00), 0);
        lv_obj_set_style_bg_color(start_pause_btn, lv_color_hex(0x00CC00), LV_STATE_PRESSED);
    }
    else if(timer.state == TIMER_STATE_RUNNING)
    {
        lv_label_set_text(start_pause_label, "Pause");
        lv_obj_clear_state(start_pause_btn, LV_STATE_DISABLED);
        // 黄色（Pause）
        lv_obj_set_style_bg_color(start_pause_btn, lv_color_hex(0xAAAA00), 0);
        lv_obj_set_style_bg_color(start_pause_btn, lv_color_hex(0xCCCC00), LV_STATE_PRESSED);
    }
    else if(timer.state == TIMER_STATE_PAUSED)
    {
        lv_label_set_text(start_pause_label, "Resume");
        lv_obj_clear_state(start_pause_btn, LV_STATE_DISABLED);
        // 绿色（Resume）
        lv_obj_set_style_bg_color(start_pause_btn, lv_color_hex(0x00AA00), 0);
        lv_obj_set_style_bg_color(start_pause_btn, lv_color_hex(0x00CC00), LV_STATE_PRESSED);
    }
    
    // 更新停止按钮状态
    if(timer.state == TIMER_STATE_IDLE)
    {
        lv_obj_add_state(stop_btn, LV_STATE_DISABLED);
    }
    else
    {
        lv_obj_clear_state(stop_btn, LV_STATE_DISABLED);
    }
}

/**
 * @brief  计时器完成回调（倒计时到达00:00:00）
 */
static void timer_complete_callback(void)
{
    // 停止计时器
    timer.state = TIMER_STATE_IDLE;
    timer.remaining_time = 0;
    
    // 更新显示（显示 00:00:00）
    update_time_display();
    update_button_states();
    
    // 如果之前的消息框还在，先删除它
    if(complete_msgbox != NULL && lv_obj_is_valid(complete_msgbox))
    {
        lv_msgbox_close(complete_msgbox);
        complete_msgbox = NULL;
    }
    
    // 如果之前的"嘀嘀嘀"定时器还在，先删除它
    if(beep_timer != NULL)
    {
        lv_timer_del(beep_timer);
        beep_timer = NULL;
    }
    
    // 完整初始化音频系统（包括WM8978和I2S配置）
    // 参考节拍器启动时的完整配置流程
    init_audio_system_for_beep();
    
    // 创建"嘀嘀嘀"声定时器（每500ms播放一次）
    beep_timer = lv_timer_create(beep_timer_cb, 500, NULL);
    // 不设置 repeat_count，默认无限重复
    // 立即播放第一声
    Metronome_Audio_PlayBeat(METRONOME_STRENGTH_MEDIUM);
    
    // 创建按钮文本数组（只有确认按钮，以 NULL 结尾）
    static const char *btns[] = {"OK", NULL};
    
    // 显示完成提示（带确认按钮，不自动关闭）
    // 使用 lv_layer_top() 确保消息框在最上层，false 表示不创建模态背景
    complete_msgbox = lv_msgbox_create(lv_layer_top(), "Timer Complete", "Countdown finished!", btns, false);
    lv_obj_center(complete_msgbox);
    
    // 获取按钮矩阵并添加回调（按钮矩阵使用 VALUE_CHANGED 事件）
    lv_obj_t *btnm = lv_msgbox_get_btns(complete_msgbox);
    if(btnm != NULL)
    {
        lv_obj_add_event_cb(btnm, msgbox_confirm_btn_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
    
    // 禁用关闭按钮，只能通过确认按钮关闭
    lv_obj_t *close_btn = lv_msgbox_get_close_btn(complete_msgbox);
    if(close_btn != NULL)
    {
        lv_obj_add_flag(close_btn, LV_OBJ_FLAG_HIDDEN);
    }
    
    LOGI("[Timer] Countdown completed! Beep started.\r\n");
}

/**
 * @brief  正计时按钮回调
 */
static void mode_btn_countup_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    
    if(timer.mode == TIMER_MODE_COUNT_UP)
        return;  // 已经是正计时模式
    
    // 停止当前计时
    timer.state = TIMER_STATE_IDLE;
    timer.elapsed_time = 0;
    
    // 切换到正计时模式
    timer.mode = TIMER_MODE_COUNT_UP;
    
    // 隐藏倒计时选择器，显示时间标签
    if(hour_roller != NULL)
        lv_obj_add_flag(hour_roller, LV_OBJ_FLAG_HIDDEN);
    if(minute_roller != NULL)
        lv_obj_add_flag(minute_roller, LV_OBJ_FLAG_HIDDEN);
    if(second_roller != NULL)
        lv_obj_add_flag(second_roller, LV_OBJ_FLAG_HIDDEN);
    if(colon1_label != NULL)
        lv_obj_add_flag(colon1_label, LV_OBJ_FLAG_HIDDEN);
    if(colon2_label != NULL)
        lv_obj_add_flag(colon2_label, LV_OBJ_FLAG_HIDDEN);
    if(time_label != NULL)
        lv_obj_clear_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    
    // 更新模式按钮样式
    lv_obj_add_state(mode_btn_countup, LV_STATE_CHECKED);
    lv_obj_clear_state(mode_btn_countdown, LV_STATE_CHECKED);
    
    // 更新显示
    update_time_display();
    update_button_states();
    
    LOGI("[Timer] Switched to count-up mode\r\n");
}

/**
 * @brief  倒计时按钮回调
 */
static void mode_btn_countdown_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    
    if(timer.mode == TIMER_MODE_COUNT_DOWN)
        return;  // 已经是倒计时模式
    
    // 停止当前计时
    timer.state = TIMER_STATE_IDLE;
    timer.remaining_time = 0;
    
    // 切换到倒计时模式
    timer.mode = TIMER_MODE_COUNT_DOWN;
    
    // 如果处于空闲状态，显示选择器；如果正在运行，显示时间标签
    if(timer.state == TIMER_STATE_IDLE)
    {
        // 显示选择器，隐藏时间标签
        if(hour_roller != NULL)
            lv_obj_clear_flag(hour_roller, LV_OBJ_FLAG_HIDDEN);
        if(minute_roller != NULL)
            lv_obj_clear_flag(minute_roller, LV_OBJ_FLAG_HIDDEN);
        if(second_roller != NULL)
            lv_obj_clear_flag(second_roller, LV_OBJ_FLAG_HIDDEN);
        if(colon1_label != NULL)
            lv_obj_clear_flag(colon1_label, LV_OBJ_FLAG_HIDDEN);
        if(colon2_label != NULL)
            lv_obj_clear_flag(colon2_label, LV_OBJ_FLAG_HIDDEN);
        if(time_label != NULL)
            lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        // 显示时间标签，隐藏选择器
        if(hour_roller != NULL)
            lv_obj_add_flag(hour_roller, LV_OBJ_FLAG_HIDDEN);
        if(minute_roller != NULL)
            lv_obj_add_flag(minute_roller, LV_OBJ_FLAG_HIDDEN);
        if(second_roller != NULL)
            lv_obj_add_flag(second_roller, LV_OBJ_FLAG_HIDDEN);
        if(colon1_label != NULL)
            lv_obj_add_flag(colon1_label, LV_OBJ_FLAG_HIDDEN);
        if(colon2_label != NULL)
            lv_obj_add_flag(colon2_label, LV_OBJ_FLAG_HIDDEN);
        if(time_label != NULL)
            lv_obj_clear_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 更新模式按钮样式
    lv_obj_add_state(mode_btn_countdown, LV_STATE_CHECKED);
    lv_obj_clear_state(mode_btn_countup, LV_STATE_CHECKED);
    
    // 更新显示
    update_time_display();
    update_button_states();
    
    LOGI("[Timer] Switched to count-down mode\r\n");
}

/**
 * @brief  开始/暂停按钮回调
 */
static void start_pause_btn_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    
    if(timer.mode == TIMER_MODE_COUNT_DOWN && timer.state == TIMER_STATE_IDLE)
    {
        // 倒计时模式：从选择器获取时间
        if(hour_roller != NULL && minute_roller != NULL && second_roller != NULL)
        {
            timer.hours = (uint8_t)lv_roller_get_selected(hour_roller);
            timer.minutes = (uint8_t)lv_roller_get_selected(minute_roller);
            timer.seconds = (uint8_t)lv_roller_get_selected(second_roller);
            timer.target_time = timer.hours * 3600 + timer.minutes * 60 + timer.seconds;
            timer.remaining_time = timer.target_time;
            
            // 如果时间为0，不启动
            if(timer.target_time == 0)
            {
                LOGI("[Timer] Cannot start with 00:00:00\r\n");
                return;
            }
            
            // 隐藏选择器，显示时间标签
            lv_obj_add_flag(hour_roller, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(minute_roller, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(second_roller, LV_OBJ_FLAG_HIDDEN);
            if(colon1_label != NULL)
                lv_obj_add_flag(colon1_label, LV_OBJ_FLAG_HIDDEN);
            if(colon2_label != NULL)
                lv_obj_add_flag(colon2_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(time_label, LV_OBJ_FLAG_HIDDEN);
        }
    }
    
    // 切换状态
    if(timer.state == TIMER_STATE_IDLE || timer.state == TIMER_STATE_PAUSED)
    {
        timer.state = TIMER_STATE_RUNNING;
        timer.last_tick = HAL_GetTick();
    }
    else if(timer.state == TIMER_STATE_RUNNING)
    {
        timer.state = TIMER_STATE_PAUSED;
    }
    
    update_button_states();
    LOGI("[Timer] State changed to: %d\r\n", timer.state);
}

/**
 * @brief  停止按钮回调
 */
static void stop_btn_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    
    // 停止计时
    timer.state = TIMER_STATE_IDLE;
    
    if(timer.mode == TIMER_MODE_COUNT_UP)
    {
        // 正计时：重置为0
        timer.elapsed_time = 0;
    }
    else
    {
        // 倒计时：重置为设置的时间，显示选择器
        timer.remaining_time = timer.target_time;
        
        // 显示选择器，隐藏时间标签
        if(hour_roller != NULL)
            lv_obj_clear_flag(hour_roller, LV_OBJ_FLAG_HIDDEN);
        if(minute_roller != NULL)
            lv_obj_clear_flag(minute_roller, LV_OBJ_FLAG_HIDDEN);
        if(second_roller != NULL)
            lv_obj_clear_flag(second_roller, LV_OBJ_FLAG_HIDDEN);
        if(colon1_label != NULL)
            lv_obj_clear_flag(colon1_label, LV_OBJ_FLAG_HIDDEN);
        if(colon2_label != NULL)
            lv_obj_clear_flag(colon2_label, LV_OBJ_FLAG_HIDDEN);
        if(time_label != NULL)
            lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    }
    
    update_time_display();
    update_button_states();
    
    LOGI("[Timer] Timer stopped and reset\r\n");
}

/**
 * @brief  创建计时器页面
 */
static void create_timer_page(void)
{
    // 清空当前屏幕
    lv_obj_clean(lv_scr_act());
    
    // 创建页面容器
    timer_page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(timer_page, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(timer_page, lv_color_black(), 0);
    lv_obj_set_style_border_width(timer_page, 0, 0);
    lv_obj_set_style_pad_all(timer_page, 0, 0);
    lv_obj_clear_flag(timer_page, LV_OBJ_FLAG_SCROLLABLE);
    
    // 创建模式切换按钮（顶部，往下移避免与back按钮重叠）
    mode_btn_countup = lv_btn_create(timer_page);
    lv_obj_set_size(mode_btn_countup, 100, 40);
    lv_obj_align(mode_btn_countup, LV_ALIGN_TOP_MID, -60, 70);
    lv_obj_add_event_cb(mode_btn_countup, mode_btn_countup_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *countup_label = lv_label_create(mode_btn_countup);
    lv_label_set_text(countup_label, "Count Up");
    lv_obj_center(countup_label);
    
    mode_btn_countdown = lv_btn_create(timer_page);
    lv_obj_set_size(mode_btn_countdown, 100, 40);
    lv_obj_align(mode_btn_countdown, LV_ALIGN_TOP_MID, 60, 70);
    lv_obj_add_event_cb(mode_btn_countdown, mode_btn_countdown_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *countdown_label = lv_label_create(mode_btn_countdown);
    lv_label_set_text(countdown_label, "Count Down");
    lv_obj_center(countdown_label);
    
    // 创建时间显示标签（中间）
    time_label = lv_label_create(timer_page);
    lv_label_set_text(time_label, "00:00:00");
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(time_label, lv_color_white(), 0);
    lv_obj_center(time_label);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -20);
    
    // 创建小时选择器（倒计时用，初始隐藏）
    hour_roller = lv_roller_create(timer_page);
    lv_roller_set_options(hour_roller, hour_options_str, LV_ROLLER_MODE_INFINITE);
    lv_obj_set_size(hour_roller, 60, 150);
    lv_obj_align(hour_roller, LV_ALIGN_CENTER, -80, -20);
    lv_obj_add_flag(hour_roller, LV_OBJ_FLAG_HIDDEN);
    
    // 创建分钟选择器（倒计时用，初始隐藏）
    minute_roller = lv_roller_create(timer_page);
    lv_roller_set_options(minute_roller, minute_second_options_str, LV_ROLLER_MODE_NORMAL);
    lv_obj_set_size(minute_roller, 60, 150);
    lv_obj_align(minute_roller, LV_ALIGN_CENTER, 0, -20);
    lv_obj_add_flag(minute_roller, LV_OBJ_FLAG_HIDDEN);
    
    // 创建秒数选择器（倒计时用，初始隐藏）
    second_roller = lv_roller_create(timer_page);
    lv_roller_set_options(second_roller, minute_second_options_str, LV_ROLLER_MODE_NORMAL);
    lv_obj_set_size(second_roller, 60, 150);
    lv_obj_align(second_roller, LV_ALIGN_CENTER, 80, -20);
    lv_obj_add_flag(second_roller, LV_OBJ_FLAG_HIDDEN);
    
    // 创建冒号标签（倒计时用，初始隐藏）
    colon1_label = lv_label_create(timer_page);
    lv_label_set_text(colon1_label, ":");
    lv_obj_set_style_text_font(colon1_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(colon1_label, lv_color_white(), 0);
    lv_obj_align(colon1_label, LV_ALIGN_CENTER, -40, -20);
    lv_obj_add_flag(colon1_label, LV_OBJ_FLAG_HIDDEN);
    
    colon2_label = lv_label_create(timer_page);
    lv_label_set_text(colon2_label, ":");
    lv_obj_set_style_text_font(colon2_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(colon2_label, lv_color_white(), 0);
    lv_obj_align(colon2_label, LV_ALIGN_CENTER, 40, -20);
    lv_obj_add_flag(colon2_label, LV_OBJ_FLAG_HIDDEN);
    
    // 创建开始/暂停按钮（底部左侧）- 绿色（Start）/黄色（Pause）
    start_pause_btn = lv_btn_create(timer_page);
    lv_obj_set_size(start_pause_btn, 120, 50);
    lv_obj_align(start_pause_btn, LV_ALIGN_BOTTOM_MID, -70, -30);
    lv_obj_add_event_cb(start_pause_btn, start_pause_btn_cb, LV_EVENT_CLICKED, NULL);
    // 设置默认绿色（Start状态）
    lv_obj_set_style_bg_color(start_pause_btn, lv_color_hex(0x00AA00), 0);
    lv_obj_set_style_bg_color(start_pause_btn, lv_color_hex(0x00CC00), LV_STATE_PRESSED);
    start_pause_label = lv_label_create(start_pause_btn);
    lv_label_set_text(start_pause_label, "Start");
    lv_obj_center(start_pause_label);
    
    // 创建停止按钮（底部右侧）- 红色
    stop_btn = lv_btn_create(timer_page);
    lv_obj_set_size(stop_btn, 120, 50);
    lv_obj_align(stop_btn, LV_ALIGN_BOTTOM_MID, 70, -30);
    lv_obj_add_event_cb(stop_btn, stop_btn_cb, LV_EVENT_CLICKED, NULL);
    // 设置红色
    lv_obj_set_style_bg_color(stop_btn, lv_color_hex(0xAA0000), 0);
    lv_obj_set_style_bg_color(stop_btn, lv_color_hex(0xCC0000), LV_STATE_PRESSED);
    lv_obj_t *stop_label = lv_label_create(stop_btn);
    lv_label_set_text(stop_label, "Stop");
    lv_obj_center(stop_label);
    
    // 创建返回按钮
    lv_obj_t *back_btn = lv_btn_create(timer_page);
    lv_obj_set_size(back_btn, 80, 40);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back_btn, btn_back_cb, LV_EVENT_CLICKED, NULL);
    
    // 设置初始状态
    lv_obj_add_state(mode_btn_countup, LV_STATE_CHECKED);
    update_button_states();
    
    LOGI("[Timer] Practice timer page created\r\n");
}

/**
 * @brief  初始化Practice计时器页面
 */
void Practice_Timer_Init(void)
{
    // 重置计时器状态
    timer.mode = TIMER_MODE_COUNT_UP;
    timer.state = TIMER_STATE_IDLE;
    timer.elapsed_time = 0;
    timer.target_time = 0;
    timer.remaining_time = 0;
    timer.hours = 0;
    timer.minutes = 0;
    timer.seconds = 0;
    timer.last_tick = 0;
    
    // 创建页面
    create_timer_page();
}

/**
 * @brief  更新计时器显示（在主循环中调用）
 */
void Practice_Timer_Update(void)
{
    // 只在运行状态下更新
    if(timer.state != TIMER_STATE_RUNNING)
        return;
    
    uint32_t current_tick = HAL_GetTick();
    uint32_t elapsed_ms = current_tick - timer.last_tick;
    
    // 每秒更新一次
    if(elapsed_ms >= 1000)
    {
        if(timer.mode == TIMER_MODE_COUNT_UP)
        {
            // 正计时：递增
            timer.elapsed_time++;
        }
        else
        {
            // 倒计时：递减
            if(timer.remaining_time > 0)
            {
                timer.remaining_time--;
            }
            else
            {
                // 倒计时完成
                timer_complete_callback();
                return;
            }
        }
        
        // 更新显示
        update_time_display();
        
        // 更新tick
        timer.last_tick = current_tick;
    }
}

/**
 * @brief  获取计时器状态
 */
timer_state_t Practice_Timer_GetState(void)
{
    return timer.state;
}

/**
 * @brief  获取计时器模式
 */
timer_mode_t Practice_Timer_GetMode(void)
{
    return timer.mode;
}
