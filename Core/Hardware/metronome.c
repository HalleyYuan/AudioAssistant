#include "metronome.h"
#include "audio_play.h"
#include "music_assistant.h"
#include "metronome_audio.h"  // 使用Kivy思路的预存储方案
#include "wm8978.h"  // WM8978音频编解码器控制
#include "i2s.h"     // I2S2初始化函数
#include "debug_uart.h"  // 调试输出

// 节拍器状态
static uint8_t metronome_running = 0;
static uint16_t metronome_bpm = METRONOME_DEFAULT_BPM;
static uint8_t metronome_beat_count = 4;  // 节拍数（默认4拍，支持2-16拍）
static uint8_t metronome_current_beat = 0;
static uint32_t metronome_tick_count = 0;
static uint32_t metronome_period_ticks = 0;  // 定时器周期（以定时器tick为单位）
static uint32_t metronome_start_tick = 0;  // 节拍器启动时的tick计数（用于同步）

// 保存播放器状态（用于互斥）
static uint8_t audio_player_was_playing = 0;

    // 中优先级改进：保存上一次的节拍强度，用于检测变化（支持最多16拍）
static uint8_t metronome_last_beat_strength[METRONOME_MAX_BEAT_COUNT] = {0};

// 定时器配置：TIM3使用84MHz时钟，预分频84-1，所以定时器频率为1MHz
// 定时器周期 = 1MHz / 1000 = 1000Hz，即1ms一个tick
#define TIMER_FREQ_HZ  1000  // 1ms per tick

/**
 * @brief  计算定时器周期（根据BPM）
 * @param  bpm: 每分钟节拍数
 * @return 定时器周期（tick数）
 */
static uint32_t CalculatePeriod(uint16_t bpm)
{
    if(bpm == 0) {
        return 0xFFFFFFFF;  // 最大周期
    }
    // 每拍的时间（毫秒）= 60000 / BPM
    // 定时器周期（tick）= 每拍时间（ms）
    uint32_t period_ms = 60000 / bpm;
    return period_ms;  // 因为1 tick = 1ms
}

/**
 * @brief  节拍器初始化
 */
void Metronome_Init(void)
{
    metronome_running = 0;
    metronome_bpm = METRONOME_DEFAULT_BPM;
    metronome_beat_count = 4;  // 默认4拍
    metronome_current_beat = 0;
    metronome_tick_count = 0;
    metronome_period_ticks = CalculatePeriod(metronome_bpm);
    
    // 初始化节拍强度记录
    for(int i = 0; i < METRONOME_MAX_BEAT_COUNT; i++) {
        metronome_last_beat_strength[i] = 0;
    }
    
    // 初始化节拍音频模块（Kivy思路：预存储方案）
    Metronome_Audio_Init();
}

/**
 * @brief  启动节拍器
 */
void Metronome_Start(void)
{
    // LOGI("[METRONOME] Metronome_Start() called\r\n");
    
    if(metronome_running) {
        // LOGI("[METRONOME] Already running, return\r\n");
        return;  // 已经在运行
    }
    
    // 与播放器互斥：如果播放器正在播放，先暂停它
    if(audio_player.state == AUDIO_STATE_PLAY) {
        // LOGI("[METRONOME] Audio player is playing, pausing it...\r\n");
        audio_player_was_playing = 1;
        Audio_Pause();
        // 等待音频播放器完全停止I2S/DMA
        HAL_Delay(50);
        // LOGI("[METRONOME] Audio player paused\r\n");
    } else {
        audio_player_was_playing = 0;
        // LOGI("[METRONOME] Audio player not playing\r\n");
    }
    
    // 确保WM8978输出已启用（节拍器需要音频输出）
    // LOGI("[METRONOME] Configuring WM8978 for metronome...\r\n");
    WM8978_ADDA_Cfg(1, 0);      // 使能DAC，禁用ADC
    WM8978_Input_Cfg(0, 0, 0);  // 关闭所有输入通道
    WM8978_Output_Cfg(1, 0);    // 使能DAC输出，关闭BYPASS
    
    // 配置I2S格式（节拍器使用16bit音频）
    WM8978_I2S_Cfg(2, 0);  // I2S格式，16bit
    I2S2_Init(I2S_Standard_Phillips, I2S_Mode_MasterTx,
             I2S_CPOL_Low, I2S_DataFormat_16bextended);  // 16bit扩展格式
    I2S2_SampleRate_Set(44100);  // 44.1KHz采样率
    
    // 设置音量（使用播放器的音量设置）
    WM8978_HPvol_Set(audio_player.volume, audio_player.volume);
    WM8978_SPKvol_Set(audio_player.volume);
    
    HAL_Delay(10);
    // LOGI("[METRONOME] WM8978 configured for metronome\r\n");
    
    // 重新计算周期（保留用于兼容性，虽然现在使用时间同步）
    metronome_period_ticks = CalculatePeriod(metronome_bpm);
    metronome_tick_count = 0;
    metronome_current_beat = 0;
    metronome_running = 1;
    
    // 记录启动时间（用于同步，类似Kivy的定时器）
    metronome_start_tick = HAL_GetTick();
    
    // 立即播放第一拍（类似Kivy的第一次beat()调用）
    extern metronome_t metronome;  // 从music_assistant.h
    
    // 中优先级改进：保存初始节拍强度
    for(int i = 0; i < metronome_beat_count; i++) {
        metronome_last_beat_strength[i] = metronome.beat_strength[i];
    }
    
    // LOGI("[METRONOME] BPM=%d, beat_count=%d, first_beat_strength=%d\r\n", 
    //      metronome_bpm, metronome_beat_count, metronome.beat_strength[0]);
    
    // LOGI("[METRONOME] Calling Metronome_Audio_PlayBeat(strength=%d)...\r\n", metronome.beat_strength[0]);
    Metronome_Audio_PlayBeat(metronome.beat_strength[0]);
    
    // 启动定时器（用于UI显示当前节拍）
    HAL_TIM_Base_Start_IT(&htim3);
    // LOGI("[METRONOME] Timer started, metronome_running=1\r\n");
}

/**
 * @brief  停止节拍器
 */
void Metronome_Stop(void)
{
    // LOGI("[METRONOME] Metronome_Stop() called, metronome_running=%d\r\n", metronome_running);
    
    if(!metronome_running) {
        // LOGI("[METRONOME] Already stopped, return\r\n");
        return;  // 已经停止
    }
    
    // LOGI("[METRONOME] Setting metronome_running=0\r\n");
    metronome_running = 0;
    
    // 停止节拍音频（Kivy思路：停止播放）
    // LOGI("[METRONOME] Calling Metronome_Audio_Stop()...\r\n");
    Metronome_Audio_Stop();
    // LOGI("[METRONOME] Metronome_Audio_Stop() returned\r\n");
    
    // 停止定时器（注意：定时器可能被LVGL使用，所以不停止，只停止中断处理）
    // HAL_TIM_Base_Stop_IT(&htim3);  // 不停止，因为LVGL需要定时器
    
    // 恢复播放器状态（如果之前在播放）
    if(audio_player_was_playing && audio_player.state == AUDIO_STATE_PAUSE) {
        // LOGI("[METRONOME] Resuming audio player...\r\n");
        Audio_Resume();
        audio_player_was_playing = 0;
    }
    
    // LOGI("[METRONOME] Metronome_Stop() completed\r\n");
}

/**
 * @brief  设置BPM
 * @param  bpm: 每分钟节拍数 (10-500)
 */
void Metronome_SetBPM(uint16_t bpm)
{
    // 限制最大BPM
    if(bpm > METRONOME_MAX_BPM) {
        bpm = METRONOME_MAX_BPM;
    }
    // 限制最小BPM（支持10-500范围）
    if(bpm > 0 && bpm < METRONOME_MIN_BPM) {
        bpm = METRONOME_MIN_BPM;
    }
    
    // 如果正在运行，BPM改变时需要重新同步（高优先级改进：BPM改变时的同步处理）
    if(metronome_running) {
        // 保存旧BPM用于计算（在更新之前）
        uint16_t old_bpm = metronome_bpm;
        
        // 更新BPM
        metronome_bpm = bpm;
        
        // 更新周期计算
        metronome_period_ticks = CalculatePeriod(metronome_bpm);
        metronome_tick_count = 0;
        
        // 重新计算起始时间，确保节拍同步
        // 计算当前在循环中的位置，然后重新设置起始时间
        uint32_t elapsed_ms = HAL_GetTick() - metronome_start_tick;
        uint32_t old_beat_duration_ms = 60000 / old_bpm;  // 旧BPM的每拍时间
        uint32_t old_cycle_duration_ms = metronome_beat_count * old_beat_duration_ms;
        
        if(old_cycle_duration_ms > 0) {
            // 计算当前在旧循环中的位置（0到beat_count-1）
            uint32_t position_in_old_cycle = (elapsed_ms % old_cycle_duration_ms) / old_beat_duration_ms;
            if(position_in_old_cycle >= metronome_beat_count) {
                position_in_old_cycle = metronome_beat_count - 1;
            }
            
            // 计算当前拍已经过的时间
            uint32_t position_in_beat = (elapsed_ms % old_cycle_duration_ms) % old_beat_duration_ms;
            
            // 重新设置起始时间，保持当前节拍位置
            uint32_t new_beat_duration_ms = 60000 / metronome_bpm;  // 新BPM的每拍时间
            metronome_start_tick = HAL_GetTick() - (position_in_old_cycle * new_beat_duration_ms + position_in_beat);
        } else {
            // 如果计算失败，直接重置起始时间
            metronome_start_tick = HAL_GetTick();
        }
    } else {
        // 如果不在运行，直接更新BPM
        metronome_bpm = bpm;
    }
}

/**
 * @brief  获取BPM
 * @return 当前BPM值
 */
uint16_t Metronome_GetBPM(void)
{
    return metronome_bpm;
}

/**
 * @brief  检查节拍器是否正在运行
 * @return 1=运行中, 0=已停止
 */
uint8_t Metronome_IsPlaying(void)
{
    return metronome_running;
}

/**
 * @brief  获取当前节拍索引
 * @return 当前节拍索引 (0到beat_count-1)
 */
uint8_t Metronome_GetCurrentBeat(void)
{
    return metronome_current_beat;
}

/**
 * @brief  设置节拍数
 * @param  beat_count: 节拍数 (2-16)
 */
void Metronome_SetBeatCount(uint8_t beat_count)
{
    if(beat_count < 2) {
        beat_count = 2;
    }
    if(beat_count > METRONOME_MAX_BEAT_COUNT) {
        beat_count = METRONOME_MAX_BEAT_COUNT;
    }
    
    metronome_beat_count = beat_count;
    
    // 如果正在运行，需要重新同步
    if(metronome_running) {
        // 确保当前节拍索引在有效范围内
        if(metronome_current_beat >= metronome_beat_count) {
            metronome_current_beat = 0;
        }
        // 重新计算起始时间
        metronome_start_tick = HAL_GetTick();
    } else {
        metronome_current_beat = 0;
    }
}

/**
 * @brief  获取节拍数
 * @return 当前节拍数 (2-16)
 */
uint8_t Metronome_GetBeatCount(void)
{
    return metronome_beat_count;
}

/**
 * @brief  检查节拍强度变化（在主循环中调用）
 * 中优先级改进：当用户改变节拍强度时，立即应用到当前播放的节拍
 */
void Metronome_CheckBeatStrengthChange(void)
{
    if(!metronome_running) {
        return;
    }
    
    extern metronome_t metronome;  // 从music_assistant.h
    
    // 检查是否有节拍强度改变
    uint8_t strength_changed = 0;
    for(int i = 0; i < metronome_beat_count; i++) {
        if(metronome.beat_strength[i] != metronome_last_beat_strength[i]) {
            strength_changed = 1;
            metronome_last_beat_strength[i] = metronome.beat_strength[i];
        }
    }
    
    // 如果当前节拍的强度改变了，且当前节拍正在播放，立即应用新强度
    // 注意：由于节拍音频是单次播放（50ms），如果正在播放，新的强度会在下次节拍时应用
    // 这里我们检查当前节拍的强度是否改变，如果改变且不在播放，可以立即播放新强度
    if(strength_changed) {
        // 如果当前节拍不在播放，可以立即应用新强度
        if(!Metronome_Audio_IsPlaying()) {
            // 重新播放当前节拍（使用新的强度）
            Metronome_Audio_PlayBeat(metronome.beat_strength[metronome_current_beat]);
        }
        // 如果正在播放，新强度会在下次节拍切换时自动应用（在Metronome_Update中）
    }
}

/**
 * @brief  节拍器更新（在定时器中断中调用，类似Kivy的Clock.schedule_interval）
 * 注意：这个函数在中断中调用，应该尽量简短
 * Kivy思路：定时触发播放，每次播放固定时长的样本
 */
void Metronome_Update(void)
{
    // 调试：记录调用（但不要太频繁，每100次输出一次）
    static uint32_t update_count = 0;
    update_count++;
    
    if(!metronome_running) {
        // if(update_count % 1000 == 0) {  // 每1000次（约1秒）输出一次
        //     LOGI("[METRONOME] Update: Not running (count=%lu)\r\n", update_count);
        // }
        return;
    }
    
    // 中优先级改进：边界检查 - BPM为0时直接返回
    if(metronome_bpm == 0) {
        return;
    }
    
    // 根据实际经过的时间计算当前节拍索引（类似Kivy的定时器触发）
    uint32_t current_tick = HAL_GetTick();
    
    // 中优先级改进：防止溢出 - 如果时间差过大，重新同步
    uint32_t elapsed_ms;
    if(current_tick >= metronome_start_tick) {
        elapsed_ms = current_tick - metronome_start_tick;
    } else {
        // 发生溢出，重新同步
        metronome_start_tick = current_tick;
        elapsed_ms = 0;
    }
    
    // 中优先级改进：防止溢出 - 如果经过时间过长（超过1分钟），重新同步
    // 这样可以避免长时间运行后的精度问题
    if(elapsed_ms > 60000) {
        // 重新计算起始时间，保持当前节拍位置
        uint32_t beat_duration_ms = 60000 / metronome_bpm;
        uint32_t cycle_duration_ms = metronome_beat_count * beat_duration_ms;
        if(cycle_duration_ms > 0) {
            uint32_t position_in_cycle = (elapsed_ms % cycle_duration_ms) / beat_duration_ms;
            if(position_in_cycle >= metronome_beat_count) {
                position_in_cycle = metronome_beat_count - 1;
            }
            uint32_t position_in_beat = (elapsed_ms % cycle_duration_ms) % beat_duration_ms;
            metronome_start_tick = current_tick - (position_in_cycle * beat_duration_ms + position_in_beat);
            elapsed_ms = current_tick - metronome_start_tick;
        } else {
            metronome_start_tick = current_tick;
            elapsed_ms = 0;
        }
    }
    
    uint32_t beat_duration_ms = 60000 / metronome_bpm;  // 每拍持续时间（毫秒）
    uint32_t cycle_duration_ms = metronome_beat_count * beat_duration_ms;  // 循环持续时间
    
    // 中优先级改进：边界检查 - 确保beat_duration_ms不为0
    if(beat_duration_ms == 0 || cycle_duration_ms == 0) {
        return;
    }
    
    // 计算当前在循环中的位置（0到beat_count-1）
    uint32_t position_in_cycle = (elapsed_ms % cycle_duration_ms) / beat_duration_ms;
    if(position_in_cycle < metronome_beat_count) {
        uint8_t new_beat = position_in_cycle;
        
        // 如果切换到新节拍，请求播放音频（类似Kivy的self.sound.play()）
        // 高优先级改进：添加播放状态检查，避免打断正在播放的节拍
        if(new_beat != metronome_current_beat) {
            metronome_current_beat = new_beat;
            
            // 获取节拍强度并请求播放（Kivy思路：定时触发播放）
            // 注意：这里只设置标志位，实际播放在主循环中处理
            extern metronome_t metronome;  // 从music_assistant.h
            if(metronome_current_beat < metronome.beat_count) {
                uint8_t strength = metronome.beat_strength[metronome_current_beat];
                // LOGI("[METRONOME] Update: Beat changed %d->%d, calling Metronome_Audio_PlayBeat(strength=%d)\r\n",
                //      metronome_current_beat, new_beat, strength);
                Metronome_Audio_PlayBeat(strength);
                // LOGI("[METRONOME] Update: Metronome_Audio_PlayBeat returned\r\n");
            } else {
                // LOGI("[METRONOME] Update: Beat changed %d->%d, but beat_index out of range\r\n",
                //      metronome_current_beat, new_beat);
            }
        }
    }
}
