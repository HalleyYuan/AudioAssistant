#include "metronome_audio.h"
#include "metronome_samples.h"  // 包含预存储的音频样本
#include "i2s.h"
#include "wm8978.h"  // WM8978音频编解码器控制
#include "debug_uart.h"  // 调试输出
#include <string.h>

// 外部DMA句柄
extern DMA_HandleTypeDef hdma_spi2_tx;
extern I2S_HandleTypeDef hi2s2;

// 播放状态
static volatile uint8_t metronome_playing = 0;

// 方案1：标志位机制（中断中设置，主循环中处理）
static volatile uint8_t metronome_play_request = 0;      // 播放请求标志
static volatile uint8_t metronome_play_request_strength = 0;  // 请求播放的强度
static volatile uint8_t metronome_play_complete_flag = 0; // 播放完成标志

// 静音数据（全0，用于空拍和停止时清空FIFO）
// 注意：使用静态数组避免栈溢出，大小与节拍样本相同
static const int16_t silence_sample[METRONOME_BEAT_SAMPLES] = {0};

/**
 * @brief  初始化节拍音频模块（Kivy思路）
 */
void Metronome_Audio_Init(void)
{
    metronome_playing = 0;
    metronome_play_request = 0;
    metronome_play_request_strength = 0;
    metronome_play_complete_flag = 0;
    
    // 确保I2S采样率正确（44.1KHz）
    I2S2_SampleRate_Set(44100);
}

/**
 * @brief  播放指定强度的节拍（Kivy思路：单次播放固定时长）
 * @param  strength: 节拍强度 (0=不出声, 1=弱, 2=中, 3=强)
 * 注意：这个函数可以在中断中调用，只设置标志位，实际播放在主循环中处理
 */
void Metronome_Audio_PlayBeat(uint8_t strength)
{
    // LOGI("[METRONOME_AUDIO] Metronome_Audio_PlayBeat(strength=%d) called\r\n", strength);
    
    // 方案1：只在中断中设置标志位，不执行实际播放操作
    // 如果正在播放，检查是否需要打断（高优先级改进：避免打断正在播放的节拍）
    if(metronome_playing) {
        // 如果当前正在播放，不打断，等待播放完成
        // 这样可以避免节拍重叠或中断
        // LOGI("[METRONOME_AUDIO] Already playing, skip request\r\n");
        return;
    }
    
    // 设置播放请求标志（可以在中断中安全调用）
    metronome_play_request = 1;
    metronome_play_request_strength = strength;
    // LOGI("[METRONOME_AUDIO] Play request set: strength=%d\r\n", strength);
}

/**
 * @brief  实际执行播放操作（在主循环中调用）
 * @param  strength: 节拍强度 (0=静音, 1=弱, 2=中, 3=强)
 * @note   空拍（strength=0）时发送静音数据而不是直接返回，
 *         这样可以清空I2S FIFO，避免输出残留数据造成杂音
 */
static void Metronome_Audio_PlayBeat_Internal(uint8_t strength)
{
    // LOGI("[METRONOME_AUDIO] PlayBeat_Internal(strength=%d) called\r\n", strength);
    
    const int16_t *sample = NULL;
    uint32_t length = METRONOME_BEAT_SAMPLES;
    
    // 根据强度选择预存储的样本（类似Kivy的SoundLoader.load）
    switch(strength) {
        case METRONOME_STRENGTH_WEAK:
            sample = beat_sample_weak;
            // LOGI("[METRONOME_AUDIO] Selected weak sample\r\n");
            break;
        case METRONOME_STRENGTH_MEDIUM:
            sample = beat_sample_medium;
            // LOGI("[METRONOME_AUDIO] Selected medium sample\r\n");
            break;
        case METRONOME_STRENGTH_STRONG:
            sample = beat_sample_strong;
            // LOGI("[METRONOME_AUDIO] Selected strong sample\r\n");
            break;
        case METRONOME_STRENGTH_SILENT:
            // 空拍：直接返回，不启动DMA/I2S
            // 注意：启动/停止DMA/I2S会产生"噗噗声"，所以空拍时完全不启动硬件
            // 如果之前有播放，DMA/I2S会在播放完成后自动停止（通过DMA完成回调）
            // LOGI("[METRONOME_AUDIO] Silent beat, skip playback (no hardware start)\r\n");
            return;
        default:
            // LOGI("[METRONOME_AUDIO] Invalid strength=%d, using silence\r\n", strength);
            sample = silence_sample;  // 无效强度也使用静音，避免杂音
            break;
    }
    
    if(sample == NULL) {
        // LOGI("[METRONOME_AUDIO] ERROR: sample is NULL, using silence!\r\n");
        sample = silence_sample;  // 安全处理：NULL时使用静音
    }
    
    // LOGI("[METRONOME_AUDIO] Sample address=0x%08X, length=%lu\r\n", (uint32_t)sample, length);
    
    // LOGI("[METRONOME_AUDIO] Stopping DMA and I2S before new playback...\r\n");
    
    // 检查硬件当前状态（用于调试）
    // uint8_t dma_running = (DMA1_Stream4->CR & DMA_SxCR_EN) ? 1 : 0;
    // uint8_t i2s_enabled = (SPI2->I2SCFGR & SPI_I2SCFGR_I2SE) ? 1 : 0;
    // LOGI("[METRONOME_AUDIO] Current hardware state - DMA=%d, I2S=%d\r\n", dma_running, i2s_enabled);
    
    // 停止顺序：先禁用DMA请求，再等待DMA完成，最后禁用I2S并清空FIFO
    // 这样可以避免FIFO残留数据造成杂音
    
    // 步骤1：禁用I2S的DMA请求（防止新的DMA传输）
    SPI2->CR2 &= ~SPI_CR2_TXDMAEN;
    
    // 步骤2：等待DMA传输完成或停止（单次传输需要等待当前样本播放完）
    // 注意：使用HAL_Delay而不是空循环，避免CPU占用过高
    uint32_t timeout = 100;  // 约100ms超时（足够50ms样本播放完）
    while((DMA1_Stream4->CR & DMA_SxCR_EN) && (timeout > 0)) {
        HAL_Delay(1);  // 每次延时1ms
        timeout--;
    }
    
    // 步骤3：强制停止DMA控制器（如果还在运行）
    if(DMA1_Stream4->CR & DMA_SxCR_EN) {
        // LOGI("[METRONOME_AUDIO] Force stopping DMA...\r\n");
        DMA1_Stream4->CR &= ~DMA_SxCR_EN;
        while(DMA1_Stream4->CR & DMA_SxCR_EN);  // 确认DMA已停止
    }
    
    // 步骤4：禁用I2S（在DMA停止后禁用，避免FIFO残留）
    if(SPI2->I2SCFGR & SPI_I2SCFGR_I2SE) {
        // LOGI("[METRONOME_AUDIO] Disabling I2S...\r\n");
        __HAL_I2S_DISABLE(&hi2s2);
    }
    
    // 步骤5：等待I2S FIFO清空（增加延时确保完全清空）
    // 注意：I2S FIFO在44.1kHz下清空需要约0.7ms，但为了确保完全清空并减少杂音，使用10ms延时
    // 注意：不发送静音数据清空FIFO，因为启动/停止DMA会产生"噗噗声"
    HAL_Delay(10);  // 10ms延时，确保FIFO完全清空（增加延时减少杂音）
    
    // LOGI("[METRONOME_AUDIO] DMA and I2S stopped, FIFO cleared\r\n");
    
    // 配置DMA为单次传输模式（不是循环模式）
    DMA1_Stream4->CR = 0;
    while(DMA1_Stream4->CR & DMA_SxCR_EN);  // 确保DMA已停止
    
    // 配置DMA寄存器（单次模式）
    DMA1_Stream4->PAR = (uint32_t)&(SPI2->DR);  // 外设地址
    DMA1_Stream4->M0AR = (uint32_t)sample;      // 内存地址（预存储的样本）
    DMA1_Stream4->NDTR = length;                // 传输数量
    // LOGI("[METRONOME_AUDIO] DMA configured: PAR=0x%08X, M0AR=0x%08X, NDTR=%lu\r\n", 
    //      DMA1_Stream4->PAR, DMA1_Stream4->M0AR, DMA1_Stream4->NDTR);
    
    // 配置DMA控制寄存器：单次模式（不是循环模式）
    // 注意：不设置DMA_SxCR_CIRC位（位5），就是单次模式
    DMA1_Stream4->CR = DMA_CHANNEL_0 |
                       DMA_PRIORITY_HIGH |
                       DMA_MDATAALIGN_HALFWORD |
                       DMA_PDATAALIGN_HALFWORD |
                       DMA_MINC_ENABLE |
                       // 不设置DMA_SxCR_CIRC（位5），就是单次模式
                       DMA_SxCR_DIR_0 |
                       DMA_SxCR_TCIE;  // 传输完成中断
    
    // 确保中断已启用
    HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
    
    // 启动顺序（参考I2S_Play_Start）：
    // LOGI("[METRONOME_AUDIO] Starting DMA and I2S...\r\n");
    // 1. 先启用DMA
    DMA1_Stream4->CR |= DMA_SxCR_EN;
    // LOGI("[METRONOME_AUDIO] DMA enabled, CR=0x%08X\r\n", DMA1_Stream4->CR);
    
    // 2. 然后启用I2S的DMA请求
    SPI2->CR2 |= SPI_CR2_TXDMAEN;
    // LOGI("[METRONOME_AUDIO] I2S DMA request enabled, CR2=0x%08X\r\n", SPI2->CR2);
    
    // 3. 最后启用I2S（开始传输）
    // 注意：使用HAL库函数确保I2S正确启动
    __HAL_I2S_ENABLE(&hi2s2);
    // LOGI("[METRONOME_AUDIO] I2S enabled, I2SCFGR=0x%08X\r\n", SPI2->I2SCFGR);
    
    // 设置播放标志
    metronome_playing = 1;
    // LOGI("[METRONOME_AUDIO] Playback started, metronome_playing=1\r\n");
}

/**
 * @brief  停止节拍音频
 * @note   正确的停止顺序可以避免杂音：
 *         1. 先禁用I2S的DMA请求，防止新的DMA传输
 *         2. 等待DMA传输完成或停止（可能需要等待当前样本播放完）
 *         3. 停止DMA控制器
 *         4. 禁用I2S，停止音频输出
 *         5. 等待I2S FIFO清空（通过延时等待FIFO自然清空）
 *         6. 清理所有标志位（防止后续处理造成杂音）
 *         注意：不发送静音数据，因为启动/停止DMA会产生"噗噗声"
 */
void Metronome_Audio_Stop(void)
{
    // LOGI("[METRONOME_AUDIO] Stop: Called (metronome_playing=%d, request=%d, complete=%d)\r\n", 
    //      metronome_playing, metronome_play_request, metronome_play_complete_flag);
    
    // 关键：立即清理所有标志位，防止后续处理造成杂音
    // 注意：必须在停止硬件之前清理，避免Metronome_Audio_Process()继续处理
    // LOGI("[METRONOME_AUDIO] Stop: Clearing flags (before: request=%d, complete=%d, strength=%d)\r\n",
    //      metronome_play_request, metronome_play_complete_flag, metronome_play_request_strength);
    metronome_play_request = 0;
    metronome_play_complete_flag = 0;
    metronome_play_request_strength = 0;
    // LOGI("[METRONOME_AUDIO] Stop: Flags cleared (after: request=%d, complete=%d, strength=%d)\r\n",
    //      metronome_play_request, metronome_play_complete_flag, metronome_play_request_strength);
    
    // 关键修复：无论metronome_playing状态如何，都要检查并停止硬件
    // 因为暂停时metronome_playing可能已经是0（DMA完成回调已设置），但硬件可能还在运行
    // 检查硬件实际状态，而不是依赖软件标志位
    uint8_t dma_running = (DMA1_Stream4->CR & DMA_SxCR_EN) ? 1 : 0;
    uint8_t i2s_enabled = (SPI2->I2SCFGR & SPI_I2SCFGR_I2SE) ? 1 : 0;
    // LOGI("[METRONOME_AUDIO] Stop: Hardware state - DMA=%d, I2S=%d, flag=%d\r\n", 
    //      dma_running, i2s_enabled, metronome_playing);
    
    // 如果硬件还在运行（DMA或I2S），必须停止它
    if(dma_running || i2s_enabled || metronome_playing) {
        // LOGI("[METRONOME_AUDIO] Stop: Hardware still running, stopping...\r\n");
        
        // 步骤1：先禁用I2S的DMA请求（防止新的DMA传输触发）
        // 注意：必须在停止DMA之前禁用，否则可能产生新的传输请求
        SPI2->CR2 &= ~SPI_CR2_TXDMAEN;
        
        // 步骤2：等待DMA传输完成或停止
        // 注意：等待当前传输完成，避免数据不完整造成杂音
        // 单次传输模式下，需要等待当前样本（50ms）播放完成
        // 使用HAL_Delay而不是空循环，避免CPU占用过高
        uint32_t timeout = 100;  // 超时计数（约100ms，足够50ms样本播放完）
        while((DMA1_Stream4->CR & DMA_SxCR_EN) && (timeout > 0)) {
            HAL_Delay(1);  // 每次延时1ms
            timeout--;
        }
        
        // 步骤3：强制停止DMA控制器（如果还在运行）
        if(DMA1_Stream4->CR & DMA_SxCR_EN) {
            // LOGI("[METRONOME_AUDIO] Stop: Force stopping DMA...\r\n");
            DMA1_Stream4->CR &= ~DMA_SxCR_EN;
            while(DMA1_Stream4->CR & DMA_SxCR_EN);  // 等待确认停止
        }
        
        // 步骤4：禁用I2S（停止音频输出）
        // 注意：在DMA停止后再禁用I2S，避免FIFO残留数据
        if(SPI2->I2SCFGR & SPI_I2SCFGR_I2SE) {
            // LOGI("[METRONOME_AUDIO] Stop: Disabling I2S...\r\n");
            __HAL_I2S_DISABLE(&hi2s2);
        }
        
        // 步骤5：等待I2S FIFO清空（避免残留数据输出造成杂音）
        // 注意：I2S FIFO通常有32位深度，在44.1kHz下需要约0.7ms才能完全清空
        // 增加延时确保FIFO完全清空，避免残留数据造成杂音
        // 注意：不发送静音数据清空FIFO，因为启动/停止DMA会产生"噗噗声"
        HAL_Delay(10);  // 10ms延时，确保FIFO完全清空（增加延时减少杂音）
        
        metronome_playing = 0;
        // LOGI("[METRONOME_AUDIO] Stop: Hardware stopped, FIFO cleared, flags cleared\r\n");
    } else {
        // LOGI("[METRONOME_AUDIO] Stop: Hardware already stopped, only flags cleared\r\n");
        metronome_playing = 0;  // 确保标志位也清零
    }
}

/**
 * @brief  检查节拍音频是否正在播放
 * @return 1=播放中, 0=已停止
 */
uint8_t Metronome_Audio_IsPlaying(void)
{
    return metronome_playing;
}

/**
 * @brief  DMA传输完成回调（单次播放完成后自动停止）
 * 注意：这个函数在中断中调用，只设置标志位，实际停止在主循环中处理
 */
void Metronome_Audio_DMA_Callback(void)
{
    // LOGI("[METRONOME_AUDIO] DMA_Callback: Transfer complete, metronome_playing=%d\r\n", metronome_playing);
    if(metronome_playing) {
        // 方案1：只在中断中设置标志位，不执行实际停止操作
        metronome_play_complete_flag = 1;
        // LOGI("[METRONOME_AUDIO] DMA_Callback: Flag set\r\n");
    } else {
        // LOGI("[METRONOME_AUDIO] DMA_Callback: Not playing, skip flag\r\n");
    }
}

/**
 * @brief  主循环中调用，处理播放请求和完成标志
 * 方案1：将DMA配置和停止操作从中断移到主循环，提高中断响应速度
 */
void Metronome_Audio_Process(void)
{
    // 调试：记录每次调用的状态
    // static uint32_t process_count = 0;
    // process_count++;
    // if(process_count % 20 == 0) {  // 每20次（约1秒）输出一次状态
    //     LOGI("[METRONOME_AUDIO] Process: Count=%lu, playing=%d, request=%d, complete=%d, running=%d\r\n",
    //          process_count, metronome_playing, metronome_play_request, 
    //          metronome_play_complete_flag, 0);  // metronome_running需要外部获取
    // }
    
    // 处理播放完成标志
    if(metronome_play_complete_flag) {
        // LOGI("[METRONOME_AUDIO] Process: Play complete flag set (playing=%d)\r\n", metronome_playing);
        metronome_play_complete_flag = 0;
        
        // 在主循环中执行停止操作（移除中断中的阻塞操作）
        if(metronome_playing) {
            // LOGI("[METRONOME_AUDIO] Process: Stopping DMA and I2S...\r\n");
            
            // 使用与Metronome_Audio_Stop()相同的停止顺序，避免杂音
            // 步骤1：禁用I2S的DMA请求
            SPI2->CR2 &= ~SPI_CR2_TXDMAEN;
            
            // 步骤2：等待DMA传输完成（单次传输需要等待当前样本播放完）
            uint32_t timeout = 100;  // 约100ms超时
            while((DMA1_Stream4->CR & DMA_SxCR_EN) && (timeout > 0)) {
                HAL_Delay(1);  // 每次延时1ms
                timeout--;
            }
            
            // 步骤3：强制停止DMA控制器（如果还在运行）
            if(DMA1_Stream4->CR & DMA_SxCR_EN) {
                DMA1_Stream4->CR &= ~DMA_SxCR_EN;
                while(DMA1_Stream4->CR & DMA_SxCR_EN);
            }
            
            // 步骤4：禁用I2S
            __HAL_I2S_DISABLE(&hi2s2);
            
            // 步骤5：等待FIFO清空（增加延时确保完全清空）
            HAL_Delay(10);  // 10ms延时，确保FIFO完全清空（增加延时减少杂音）
            
            metronome_playing = 0;
            // LOGI("[METRONOME_AUDIO] Process: Stopped, metronome_playing=0\r\n");
        }
    }
    
    // 处理播放请求标志
    if(metronome_play_request) {
        // LOGI("[METRONOME_AUDIO] Process: Play request flag set, strength=%d (playing=%d)\r\n", 
        //      metronome_play_request_strength, metronome_playing);
        metronome_play_request = 0;
        uint8_t strength = metronome_play_request_strength;
        
        // 在主循环中执行播放操作（移除中断中的阻塞操作）
        // 再次检查是否正在播放（双重保护）
        if(!metronome_playing) {
            // LOGI("[METRONOME_AUDIO] Process: Not playing, calling PlayBeat_Internal(strength=%d)...\r\n", strength);
            Metronome_Audio_PlayBeat_Internal(strength);
            // LOGI("[METRONOME_AUDIO] Process: PlayBeat_Internal returned\r\n");
        } else {
            // LOGI("[METRONOME_AUDIO] Process: Already playing, skip request\r\n");
        }
    }
}
