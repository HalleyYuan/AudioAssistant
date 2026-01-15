/**
  ******************************************************************************
  * @file    audio_play.c
  * @brief   音频播放模块实现 - WAV文件播放、I2S DMA传输、WM8978控制
  * @author  项目开发者
  * @date    2025-01-xx
  * @note    支持16bit/24bit WAV格式，使用I2S2+DMA双缓冲传输，WM8978音频编解码器
  ******************************************************************************
  */

#include "audio_play.h"
#include "i2s.h"
#include "debug_uart.h"
#include <string.h>
#include <stdlib.h>

/* 全局变量 ---------------------------------------------------------*/
AudioPlayer_t audio_player = {  ///< 音频播放器状态结构
    .state = AUDIO_STATE_STOP,
    .volume = 30,
    .total_time = 0,
    .current_time = 0
};

__audiodev audiodev;            ///< 音频设备控制结构（文件句柄、DMA缓冲区等）
__wavctrl wavctrl;              ///< WAV文件控制结构（采样率、通道数、数据位置等）
volatile uint8_t wavtransferend = 0;  ///< DMA传输完成标志
volatile uint8_t wavwitchbuf = 0;      ///< DMA缓冲区切换标志（0=buf1, 1=buf2）

/**
  * @brief  音频系统初始化
  * @retval None
  * @note   初始化WM8978音频编解码器，配置音频路径（DAC输出），设置音量
  * @author 项目开发者
  * @date   2025-01-xx
 */
void Audio_Init(void)
{
    // 初始化WM8978（包含所有寄存器配置）
    if(WM8978_Init() != 0) {
        // 初始化失败，可以通过LED指示
        return;
    }
    HAL_Delay(50);
    
    // 配置音频路径（参考例程audioplay.c第115-117行）
    WM8978_ADDA_Cfg(1, 0);      // 使能DAC，禁用ADC
    WM8978_Input_Cfg(0, 0, 0);  // 关闭所有输入通道（MIC, LINE IN, AUX）
    WM8978_Output_Cfg(1, 0);    // 使能DAC输出，关闭BYPASS
    
    HAL_Delay(10);
    
    // 设置音量（0-63，这里设置为最大）
    WM8978_HPvol_Set(63, 63);  // 耳机音量最大
    WM8978_SPKvol_Set(63);     // 扬声器音量最大
    
    HAL_Delay(50);
}

/**
  * @brief  开始音频播放（内部函数）
  * @retval None
  * @note   设置播放状态标志并启动I2S DMA传输
 */
void audio_start(void)
{
    audiodev.status = 3 << 0;
    I2S_Play_Start();
}

/**
  * @brief  停止音频播放（内部函数）
  * @retval None
  * @note   清除播放状态标志并停止I2S DMA传输
 */
void audio_stop(void)
{
    audiodev.status = 0;
    I2S_Play_Stop();
}

/**
  * @brief  WAV文件头解析和初始化
  * @param  fname: WAV文件路径
  * @param  wavx: WAV控制结构指针（输出参数）
  * @retval 0=成功, 1=文件打开失败, 2=格式错误, 3=数据块未找到
  * @note   解析WAV文件头，提取采样率、通道数、位深、数据起始位置等信息
  *          支持标准WAV格式和包含FACT块的WAV格式
 */
uint8_t wav_decode_init(uint8_t* fname, __wavctrl* wavx)
{
    FIL *ftemp;
    uint8_t *buf;
    uint32_t br = 0;
    uint8_t res = 1;  // 默认返回失败，只有成功时才返回0

    ChunkRIFF *riff;
    ChunkFMT *fmt;
    ChunkFACT *fact;
    ChunkDATA *data;

    // 初始化输出结构，避免未初始化值
    wavx->totsec = 0;
    wavx->cursec = 0;

    ftemp = (FIL*)malloc(sizeof(FIL));
    buf = (uint8_t*)malloc(512);

    if(ftemp && buf) {
        res = f_open(ftemp, (TCHAR*)fname, FA_READ);
        if(res == FR_OK) {
            f_read(ftemp, buf, 512, &br);
            riff = (ChunkRIFF *)buf;

            if(riff->Format == 0x45564157) {  // "WAVE"
                fmt = (ChunkFMT *)(buf + 12);
                fact = (ChunkFACT *)(buf + 12 + 8 + fmt->ChunkSize);

                if(fact->ChunkID == 0x74636166 || fact->ChunkID == 0x5453494C)
                    wavx->datastart = 12 + 8 + fmt->ChunkSize + 8 + fact->ChunkSize;
                else
                    wavx->datastart = 12 + 8 + fmt->ChunkSize;

                data = (ChunkDATA *)(buf + wavx->datastart);

                if(data->ChunkID == 0x61746164) {  // "data"
                    wavx->audioformat = fmt->AudioFormat;
                    wavx->nchannels = fmt->NumOfChannels;
                    wavx->samplerate = fmt->SampleRate;
                    wavx->bitrate = fmt->ByteRate * 8;
                    wavx->blockalign = fmt->BlockAlign;
                    wavx->bps = fmt->BitsPerSample;
                    wavx->datasize = data->ChunkSize;
                    wavx->datastart = wavx->datastart + 8;
                    
                    // 计算总时长：数据大小 / 字节率
                    // 优先使用WAV头中的ByteRate字段（更可靠），如果为0则手动计算
                    uint32_t byte_rate = fmt->ByteRate;
                    if(byte_rate == 0) {
                        // 如果ByteRate为0，手动计算：采样率 * 通道数 * 位深/8
                        byte_rate = wavx->samplerate * wavx->nchannels * (wavx->bps / 8);
                    }
                    if(byte_rate > 0) {
                        wavx->totsec = wavx->datasize / byte_rate;
                    } else {
                        wavx->totsec = 0;
                    }
                    wavx->cursec = 0;
                } else {
                    res = 3;
                }
            } else {
                res = 2;
            }
        } else {
            res = 1;
        }
    }

    if(ftemp) {
    f_close(ftemp);
    free(ftemp);
    }
    if(buf) {
    free(buf);
    }
    return res;
}

/**
  * @brief  填充WAV音频数据到DMA缓冲区
  * @param  buf: DMA缓冲区指针
  * @param  size: 缓冲区大小（字节）
  * @param  bits: 音频位深（16或24）
  * @retval 实际读取的字节数
  * @note   16bit格式：直接读取，DMA配置为HALFWORD，I2S硬件自动处理16bit扩展格式
  *          24bit格式：需要转换格式（24bit转32bit）
 */
uint32_t wav_buffill(uint8_t *buf, uint16_t size, uint8_t bits)
{
    uint16_t readlen = 0;
    uint32_t bread;
    uint16_t i;
    uint8_t *p;

    if(bits == 24) {
        // 24bit音频：需要转换格式
        readlen = (size / 4) * 3;
        f_read(audiodev.file, audiodev.tbuf, readlen, (UINT*)&bread);
        p = audiodev.tbuf;
        for(i = 0; i < size;) {
            buf[i++] = p[1];
            buf[i] = p[2];
            i += 2;
            buf[i++] = p[0];
            p += 3;
        }
        bread = (bread * 4) / 3;
    } else {
        // 16bit音频：直接读取，DMA配置为HALFWORD，I2S硬件会自动处理16bit扩展格式
        f_read(audiodev.file, buf, size, (UINT*)&bread);
        if(bread < size) {
            for(i = bread; i < size - bread; i++)
                buf[i] = 0;
        }
    }
    return bread;
}

/**
  * @brief  WAV I2S DMA传输完成回调函数
  * @retval None
  * @note   当DMA传输完成时调用，设置传输完成标志和缓冲区切换标志
  *          如果播放已停止，将缓冲区填充为0（静音）
 */
void wav_i2s_dma_tx_callback(void)
{
    uint16_t i;

    if(DMA1_Stream4->CR & (1 << 19)) {
        wavwitchbuf = 0;
        if((audiodev.status & 0x01) == 0) {
            for(i = 0; i < WAV_I2S_TX_DMA_BUFSIZE; i++)
                audiodev.i2sbuf1[i] = 0;
        }
    } else {
        wavwitchbuf = 1;
        if((audiodev.status & 0x01) == 0) {
            for(i = 0; i < WAV_I2S_TX_DMA_BUFSIZE; i++)
                audiodev.i2sbuf2[i] = 0;
        }
    }
    wavtransferend = 1;
}

/**
  * @brief  获取当前播放时间（内部函数）
  * @param  fx: 文件句柄指针
  * @param  wavx: WAV控制结构指针
  * @retval None
  * @note   根据文件指针位置计算当前播放时间（秒），如果总时长未计算则先计算
 */
void wav_get_curtime(FIL *fx, __wavctrl *wavx)
{
    long long fpos;
    // totsec 只计算一次，不要每次覆盖
    if(wavx->totsec == 0) {
        // 使用和 Audio_GetWavDuration 相同的计算方法
        uint32_t byte_rate = wavx->samplerate * wavx->nchannels * (wavx->bps / 8);
        if(byte_rate > 0) {
            wavx->totsec = wavx->datasize / byte_rate;
        }
    }
    fpos = fx->fptr - wavx->datastart;
    if(wavx->datasize > 0) {
        wavx->cursec = fpos * wavx->totsec / wavx->datasize;
    }
}

/**
  * @brief  播放WAV文件（阻塞方式，已废弃，建议使用Audio_Load+Audio_Resume）
  * @param  fname: WAV文件路径
  * @retval 0=成功, 0xFF=失败
  * @note   阻塞式播放，直到文件播放完成或出错。当前代码中已不使用此函数
  *          建议使用非阻塞方式：Audio_Load加载 + Audio_Resume播放 + Audio_Update更新
 */
uint8_t wav_play_song(uint8_t* fname)
{
    uint8_t res;
    uint32_t fillnum;

    audiodev.file = (FIL*)malloc(sizeof(FIL));
    audiodev.i2sbuf1 = (uint8_t*)malloc(WAV_I2S_TX_DMA_BUFSIZE);
    audiodev.i2sbuf2 = (uint8_t*)malloc(WAV_I2S_TX_DMA_BUFSIZE);
    audiodev.tbuf = (uint8_t*)malloc(WAV_I2S_TX_DMA_BUFSIZE);

    if(audiodev.file && audiodev.i2sbuf1 && audiodev.i2sbuf2 && audiodev.tbuf) {
        res = wav_decode_init(fname, &wavctrl);

        if(res == 0) {
            if(wavctrl.bps == 16) {
                // 16bit音频：使用I2S格式，16bit扩展帧格式（参考例程）
                WM8978_I2S_Cfg(2, 0);  // I2S格式，16bit
                I2S2_Init(I2S_Standard_Phillips, I2S_Mode_MasterTx,
                         I2S_CPOL_Low, I2S_DataFormat_16bextended);  // 使用16bit扩展格式
            } else if(wavctrl.bps == 24) {
                // 24bit音频：使用I2S格式，24bit数据
                WM8978_I2S_Cfg(2, 2);  // I2S格式，24bit
                I2S2_Init(I2S_Standard_Phillips, I2S_Mode_MasterTx,
                         I2S_CPOL_Low, I2S_DataFormat_24b);
            }
            
            HAL_Delay(10);

            I2S2_SampleRate_Set(wavctrl.samplerate);
            I2S2_TX_DMA_Init(audiodev.i2sbuf1, audiodev.i2sbuf2,
                           WAV_I2S_TX_DMA_BUFSIZE / 2);
            i2s_tx_callback = wav_i2s_dma_tx_callback;

            audio_stop();
            res = f_open(audiodev.file, (TCHAR*)fname, FA_READ);

            if(res == 0) {
                f_lseek(audiodev.file, wavctrl.datastart);
                fillnum = wav_buffill(audiodev.i2sbuf1, WAV_I2S_TX_DMA_BUFSIZE, wavctrl.bps);
                fillnum = wav_buffill(audiodev.i2sbuf2, WAV_I2S_TX_DMA_BUFSIZE, wavctrl.bps);

                audio_start();
                audio_player.state = AUDIO_STATE_PLAY;

                while(res == 0) {
                    while(wavtransferend == 0);
                    wavtransferend = 0;

                    if(fillnum != WAV_I2S_TX_DMA_BUFSIZE) {
                        break;
                    }

                    if(wavwitchbuf)
                        fillnum = wav_buffill(audiodev.i2sbuf2, WAV_I2S_TX_DMA_BUFSIZE, wavctrl.bps);
                    else
                        fillnum = wav_buffill(audiodev.i2sbuf1, WAV_I2S_TX_DMA_BUFSIZE, wavctrl.bps);

                    wav_get_curtime(audiodev.file, &wavctrl);
                    audio_player.current_time = wavctrl.cursec;
                    audio_player.total_time = wavctrl.totsec;

                    if((audiodev.status & 0x01) == 0) {
                        HAL_Delay(10);
                    } else {
                        break;
                    }
                }
                audio_stop();
            } else {
                res = 0xFF;
            }
        } else {
            res = 0xFF;
        }
    } else {
        res = 0xFF;
    }

    free(audiodev.tbuf);
    free(audiodev.i2sbuf1);
    free(audiodev.i2sbuf2);
    free(audiodev.file);

    audio_player.state = AUDIO_STATE_STOP;
    return res;
}

/**
  * @brief  加载音频文件（非阻塞方式）
  * @param  filename: 音频文件路径
  * @param  auto_play: 是否自动播放（1=自动播放, 0=仅加载不播放）
  * @retval None
  * @note   解析WAV文件头，分配DMA缓冲区，配置I2S和WM8978，预填充缓冲区
  *          如果auto_play=1则立即开始播放，否则处于暂停状态等待Audio_Resume调用
  *          调用前如果正在播放，会自动停止并释放资源
  * @author 项目开发者
  * @date   2025-01-xx
 */
void Audio_Load(const char* filename, uint8_t auto_play)
{
    uint8_t res;

    // 如果当前正在播放或暂停，先停止（释放资源）
    // 注意：如果外部已经调用了Audio_Stop()，这里的状态应该是STOP，不会重复停止
    if(audio_player.state == AUDIO_STATE_PLAY || audio_player.state == AUDIO_STATE_PAUSE) {
        Audio_Stop();
        HAL_Delay(100);
    }
    
    // 确保状态是STOP
    audio_player.state = AUDIO_STATE_STOP;
    audio_player.total_time = 0;  // 先清零，避免显示旧值
    audio_player.current_time = 0;

    // 先解析WAV文件头获取信息（不分配大缓冲区）
    // 这样可以先检查文件是否有效，避免浪费内存
    LOGI("[AUDIO_LOAD] Calling wav_decode_init for: %s\r\n", filename);
    res = wav_decode_init((uint8_t*)filename, &wavctrl);
    LOGI("[AUDIO_LOAD] wav_decode_init returned: %d\r\n", res);
    if(res != 0) {
        // WAV文件解析失败，直接返回
        LOGI("[AUDIO_LOAD ERROR] WAV decode failed, res=%d\r\n", res);
        return;
    }
    
    // 文件头解析成功，立即设置总时长
    audio_player.total_time = wavctrl.totsec;
    LOGI("[AUDIO_LOAD] Set audio_player.total_time=%lu (wavctrl.totsec=%lu)\r\n", 
         audio_player.total_time, wavctrl.totsec);

    // 现在分配播放所需的大缓冲区
    // 计算总内存需求并检查
    uint32_t total_mem_needed = sizeof(FIL) + WAV_I2S_TX_DMA_BUFSIZE * 3;
    LOGI("[AUDIO] Allocating %lu bytes for audio buffers\r\n", total_mem_needed);

    audiodev.file = (FIL*)malloc(sizeof(FIL));
    if(audiodev.file == NULL) {
        LOGI("[AUDIO ERROR] Failed to allocate FIL structure (%lu bytes)\r\n", sizeof(FIL));
        return;
    }
    
    audiodev.i2sbuf1 = (uint8_t*)malloc(WAV_I2S_TX_DMA_BUFSIZE);
    if(audiodev.i2sbuf1 == NULL) {
        LOGI("[AUDIO ERROR] Failed to allocate i2sbuf1 (%d bytes)\r\n", WAV_I2S_TX_DMA_BUFSIZE);
        free(audiodev.file);
        return;
    }
    
    audiodev.i2sbuf2 = (uint8_t*)malloc(WAV_I2S_TX_DMA_BUFSIZE);
    if(audiodev.i2sbuf2 == NULL) {
        LOGI("[AUDIO ERROR] Failed to allocate i2sbuf2 (%d bytes)\r\n", WAV_I2S_TX_DMA_BUFSIZE);
        free(audiodev.i2sbuf1);
        free(audiodev.file);
        return;
    }

    audiodev.tbuf = (uint8_t*)malloc(WAV_I2S_TX_DMA_BUFSIZE);
    if(audiodev.tbuf == NULL) {
        LOGI("[AUDIO ERROR] Failed to allocate tbuf (%d bytes)\r\n", WAV_I2S_TX_DMA_BUFSIZE);
        free(audiodev.i2sbuf2);
        free(audiodev.i2sbuf1);
        free(audiodev.file);
        return;
    }
    
    LOGI("[AUDIO] Memory allocation successful, total: %lu bytes\r\n", total_mem_needed);

    // 重新确认WM8978音频路径配置（确保DAC和输出使能）
    LOGI("[AUDIO_LOAD] Configuring WM8978 for playback...\r\n");
    WM8978_ADDA_Cfg(1, 0);      // 使能DAC，禁用ADC
    WM8978_Input_Cfg(0, 0, 0);  // 关闭所有输入通道
    WM8978_Output_Cfg(1, 0);    // 使能DAC输出，关闭BYPASS
    
    // 检查WM8978关键寄存器
    uint16_t r2 = WM8978_Read_Reg(2);
    uint16_t r3 = WM8978_Read_Reg(3);
    uint16_t r4 = WM8978_Read_Reg(4);
    uint16_t r50 = WM8978_Read_Reg(50);
    uint16_t r51 = WM8978_Read_Reg(51);
    LOGI("[AUDIO_LOAD] WM8978 status: R2=0x%04X, R3=0x%04X, R4=0x%04X, R50=0x%04X, R51=0x%04X\r\n",
         r2, r3, r4, r50, r51);
    LOGI("[AUDIO_LOAD] R50/R51: DAC_EN=%d, BYPASS_EN=%d (should be DAC=1, BYPASS=0)\r\n",
         (r50 & 0x01), ((r50 >> 1) & 0x01));
    
    HAL_Delay(10);

    if(wavctrl.bps == 16) {
        // 16bit音频：使用I2S格式，16bit扩展帧格式（参考例程）
        LOGI("[AUDIO_LOAD] Configuring for 16bit audio...\r\n");
        WM8978_I2S_Cfg(2, 0);  // I2S格式，16bit
        I2S2_Init(I2S_Standard_Phillips, I2S_Mode_MasterTx,
                 I2S_CPOL_Low, I2S_DataFormat_16bextended);  // 使用16bit扩展格式
    } else if(wavctrl.bps == 24) {
        // 24bit音频：使用I2S格式，24bit数据
        LOGI("[AUDIO_LOAD] Configuring for 24bit audio...\r\n");
        WM8978_I2S_Cfg(2, 2);  // I2S格式，24bit
        I2S2_Init(I2S_Standard_Phillips, I2S_Mode_MasterTx,
                 I2S_CPOL_Low, I2S_DataFormat_24b);
    }
    
    // 再次检查R4配置
    r4 = WM8978_Read_Reg(4);
    LOGI("[AUDIO_LOAD] WM8978 R4 after I2S_Cfg: 0x%04X\r\n", r4);
    
    // 确保音量设置正确（播放时可能被录音功能修改）
    WM8978_HPvol_Set(audio_player.volume, audio_player.volume);
    WM8978_SPKvol_Set(audio_player.volume);
    LOGI("[AUDIO_LOAD] Volume set to %d\r\n", audio_player.volume);
    
    HAL_Delay(10);

    I2S2_SampleRate_Set(wavctrl.samplerate);
    I2S2_TX_DMA_Init(audiodev.i2sbuf1, audiodev.i2sbuf2, WAV_I2S_TX_DMA_BUFSIZE / 2);
    i2s_tx_callback = wav_i2s_dma_tx_callback;

    // 重新打开文件用于播放（之前已经打开过用于计算时长，需要重新打开）
    res = f_open(audiodev.file, (TCHAR*)filename, FA_READ);
    if(res != FR_OK) {
        free(audiodev.file);
        free(audiodev.i2sbuf1);
        free(audiodev.i2sbuf2);
        free(audiodev.tbuf);
        // 文件打开失败，重置total_time为0，表示加载失败
        audio_player.total_time = 0;
        return;
    }

    f_lseek(audiodev.file, wavctrl.datastart);

    wav_buffill(audiodev.i2sbuf1, WAV_I2S_TX_DMA_BUFSIZE, wavctrl.bps);
    wav_buffill(audiodev.i2sbuf2, WAV_I2S_TX_DMA_BUFSIZE, wavctrl.bps);

    // total_time已经在wav_decode_init成功后设置（第349行），这里不需要重复设置
    // 但确保current_time被重置
    audio_player.current_time = 0;

    // 根据auto_play参数决定是否立即播放
    if(auto_play) {
        // 设置为播放状态并启动
        audio_player.state = AUDIO_STATE_PLAY;
        audiodev.status = 3 << 0;  // 设置播放状态标志
        
        // 确保I2S和DMA正确启动
        HAL_Delay(10);  // 短暂延时，确保DMA配置完成
        I2S_Play_Start();  // 启动I2S和DMA
        HAL_Delay(10);  // 短暂延时，确保启动完成
        
        LOGI("[AUDIO_LOAD] Started playback, total_time=%lu\r\n", audio_player.total_time);
    } else {
        // 设置为暂停状态，不启动播放
        audio_player.state = AUDIO_STATE_PAUSE;
        audiodev.status = 0;  // 暂停状态：清除播放标志
        
        LOGI("[AUDIO_LOAD] Loaded (paused), total_time=%lu\r\n", audio_player.total_time);
    }
}

/**
  * @brief  播放音频文件（高级接口，自动播放）
  * @param  filename: 音频文件路径
  * @retval None
  * @note   调用Audio_Load并设置auto_play=1，立即开始播放
 */
void Audio_Play(const char* filename)
{
    Audio_Load(filename, 1);  // 自动播放
}

/**
  * @brief  暂停音频播放
  * @retval None
  * @note   如果正在播放，则暂停播放（停止DMA传输，但保持文件打开和缓冲区状态）
 */
void Audio_Pause(void)
{
    if(audio_player.state == AUDIO_STATE_PLAY) {
        audiodev.status &= ~(1 << 0);
        audio_player.state = AUDIO_STATE_PAUSE;
    }
}

/**
  * @brief  恢复音频播放
  * @retval None
  * @note   如果处于暂停状态，则恢复播放（重新启动DMA传输）
  *          如果DMA已停止，会重新配置并启动DMA
 */
void Audio_Resume(void)
{
    if(audio_player.state == AUDIO_STATE_PAUSE) {
        audiodev.status = 3 << 0;  // 设置播放状态标志
        audio_player.state = AUDIO_STATE_PLAY;
        
        // 检查DMA是否已停止（可能在跳转时被停止了）
        // 如果DMA已停止，需要重新启动
        if((DMA1_Stream4->CR & DMA_SxCR_EN) == 0) {
            // DMA已停止，重新启动
            HAL_Delay(10);
            I2S_Play_Start();
            HAL_Delay(10);
        } else {
            // DMA未停止，只需要重新启用I2S
            SPI2->I2SCFGR |= SPI_I2SCFGR_I2SE;
        }
    }
}

/**
  * @brief  停止音频播放并释放资源
  * @retval None
  * @note   停止播放，关闭文件，释放所有分配的缓冲区内存，重置播放状态
 */
void Audio_Stop(void)
{
    if(audio_player.state != AUDIO_STATE_STOP) {
        audiodev.status |= 0x01;
        audio_stop();
        audio_player.state = AUDIO_STATE_STOP;
        audio_player.current_time = 0;
    }
    
    /* 释放所有分配的内存资源 */
    if(audiodev.file != NULL) {
        if(f_close(audiodev.file) != FR_OK) {
            /* 文件可能已经关闭，忽略错误 */
        }
        free(audiodev.file);
        audiodev.file = NULL;
    }
    
    if(audiodev.i2sbuf1 != NULL) {
        free(audiodev.i2sbuf1);
        audiodev.i2sbuf1 = NULL;
    }
    
    if(audiodev.i2sbuf2 != NULL) {
        free(audiodev.i2sbuf2);
        audiodev.i2sbuf2 = NULL;
    }
    
    if(audiodev.tbuf != NULL) {
        free(audiodev.tbuf);
        audiodev.tbuf = NULL;
}

    /* 重置总时长，表示没有加载的音频 */
    audio_player.total_time = 0;
    LOGI("[AUDIO] Audio_Stop() - All resources freed\r\n");
}

/**
  * @brief  设置音频音量
  * @param  vol: 音量值（0-63，63为最大音量）
  * @retval None
  * @note   同时设置WM8978的耳机输出和扬声器输出音量
 */
void Audio_SetVolume(uint8_t vol)
{
    if(vol > 63) vol = 63;
    audio_player.volume = vol;
    WM8978_HPvol_Set(vol, vol);
    WM8978_SPKvol_Set(vol);
}

/**
  * @brief  音频播放状态更新（需要在主循环中定期调用）
  * @retval None
  * @note   检查DMA传输完成标志，填充下一个缓冲区，更新播放时间
  *          检测播放结束（文件结束或用户停止），自动停止并释放资源
  *          建议在主循环中每5-10ms调用一次
  */
void Audio_Update(void)
{
    static uint32_t last_time_update = 0;
    uint32_t now = HAL_GetTick();

    if(audio_player.state == AUDIO_STATE_PLAY) {
        // 每100ms更新一次时间（基于实际文件位置，10Hz更新频率）
        if(now - last_time_update >= 100) {
            last_time_update = now;
            
            // 基于文件位置计算当前播放时间（更精确）
            if(audiodev.file != NULL && wavctrl.datasize > 0) {
                uint32_t file_pos = f_tell(audiodev.file);
                if(file_pos >= wavctrl.datastart) {
                    uint32_t played_bytes = file_pos - wavctrl.datastart;
                    // 确保wavctrl.bitrate不为0，避免除零错误
                    // bitrate是每秒位数（bits per second），需要除以8转换为字节/秒
                    if (wavctrl.bitrate > 0) {
                        audio_player.current_time = played_bytes / (wavctrl.bitrate / 8); // 字节数 -> 秒
                    } else {
                        audio_player.current_time = 0; // 避免除零
                    }
                    
                    // 确保不超过总时长
                    if(audio_player.current_time > audio_player.total_time) {
                        audio_player.current_time = audio_player.total_time;
                    }
                }
            }
        }

        if(wavtransferend) {
            wavtransferend = 0;

            uint32_t fillnum;
            if(wavwitchbuf)
                fillnum = wav_buffill(audiodev.i2sbuf2, WAV_I2S_TX_DMA_BUFSIZE, wavctrl.bps);
            else
                fillnum = wav_buffill(audiodev.i2sbuf1, WAV_I2S_TX_DMA_BUFSIZE, wavctrl.bps);

            if(fillnum != WAV_I2S_TX_DMA_BUFSIZE) {
                audio_stop();
                f_close(audiodev.file);
                free(audiodev.file);
                free(audiodev.i2sbuf1);
                free(audiodev.i2sbuf2);
                free(audiodev.tbuf);

                audio_player.state = AUDIO_STATE_STOP;
                audio_player.current_time = 0;
                return;
            }
        }

        if((audiodev.status & 0x01) == 0 || f_eof(audiodev.file)) {
            audio_stop();
            f_close(audiodev.file);
            free(audiodev.file);
            free(audiodev.i2sbuf1);
            free(audiodev.i2sbuf2);
            free(audiodev.tbuf);

            audio_player.state = AUDIO_STATE_STOP;
            audio_player.current_time = 0;
        }
    }
}

/**
  * @brief  跳转到指定播放时间位置
  * @param  target_time: 目标时间（秒）
  * @retval 0=成功, 1=失败（文件未加载或参数无效）
  * @note   计算目标文件位置，跳转文件指针，重新填充DMA缓冲区
  *          如果之前正在播放，跳转后继续播放；如果之前是暂停，跳转后保持暂停
 */
uint8_t Audio_Seek(uint32_t target_time)
{
    // 只有在播放或暂停状态才能跳转
    if(audio_player.state == AUDIO_STATE_STOP || audiodev.file == NULL) {
        return 1;
    }

    // 计算目标位置（字节）
    uint32_t byte_rate = wavctrl.samplerate * wavctrl.nchannels * (wavctrl.bps / 8);
    if(byte_rate == 0) {
        return 1;
    }

    // 限制目标时间在有效范围内
    if(target_time > wavctrl.totsec) {
        target_time = wavctrl.totsec;
    }

    // 计算目标文件位置
    uint32_t target_pos = wavctrl.datastart + (target_time * byte_rate);
    
    // 确保不超过数据结束位置
    uint32_t data_end = wavctrl.datastart + wavctrl.datasize;
    if(target_pos > data_end) {
        target_pos = data_end;
    }

    // 保存当前播放状态
    uint8_t was_playing = (audio_player.state == AUDIO_STATE_PLAY);
    
    // 停止DMA传输
    I2S_Play_Stop();
    HAL_Delay(10);

    // 跳转文件指针
    f_lseek(audiodev.file, target_pos);

    // 更新当前时间
    wavctrl.cursec = target_time;
    audio_player.current_time = target_time;

    // 重新填充缓冲区
    uint32_t fillnum1 = wav_buffill(audiodev.i2sbuf1, WAV_I2S_TX_DMA_BUFSIZE, wavctrl.bps);
    uint32_t fillnum2 = wav_buffill(audiodev.i2sbuf2, WAV_I2S_TX_DMA_BUFSIZE, wavctrl.bps);

    // 如果文件已结束，停止播放
    if(fillnum1 != WAV_I2S_TX_DMA_BUFSIZE || fillnum2 != WAV_I2S_TX_DMA_BUFSIZE) {
        audio_stop();
        f_close(audiodev.file);
        free(audiodev.file);
        free(audiodev.i2sbuf1);
        free(audiodev.i2sbuf2);
        free(audiodev.tbuf);
        audio_player.state = AUDIO_STATE_STOP;
        audio_player.current_time = 0;
        return 0;
    }

    // 无论之前是播放还是暂停，都需要重新配置DMA（因为DMA已经被停止了）
    // 这样在暂停状态下跳转后，按播放时DMA才能正常工作
    I2S2_TX_DMA_Init(audiodev.i2sbuf1, audiodev.i2sbuf2, WAV_I2S_TX_DMA_BUFSIZE / 2);
    
    // 只有之前是播放状态时才启动DMA
    if(was_playing) {
        // 重新启动播放
        audio_start();
    }
    // 如果之前是暂停状态，保持暂停状态，DMA已配置但未启动

    return 0;
}

/**
  * @brief  获取WAV文件总时长（不播放文件）
  * @param  filename: WAV文件路径
  * @retval 文件时长（秒），失败返回0
  * @note   仅读取文件头，不加载完整文件，用于快速获取时长信息
 */
uint32_t Audio_GetWavDuration(const char* filename)
{
    FIL file;
    WaveHeader wavhead;
    uint8_t res;
    uint32_t duration = 0;

    res = f_open(&file, filename, FA_READ);
    if(res != FR_OK) return 0;

    // 读取WAV头
    UINT br;
    res = f_read(&file, &wavhead, sizeof(WaveHeader), &br);
    if(res != FR_OK || br != sizeof(WaveHeader)) {
        f_close(&file);
        return 0;
    }

    // 检查格式
    if(wavhead.riff.ChunkID != 0x46464952 ||  // "RIFF"
       wavhead.riff.Format != 0x45564157 ||    // "WAVE"
       wavhead.fmt.ChunkID != 0x20746D66) {    // "fmt "
        f_close(&file);
        return 0;
    }

    // 计算时长：数据大小 / (采样率 * 通道数 * 位深/8)
    uint32_t byte_rate = wavhead.fmt.SampleRate * wavhead.fmt.NumOfChannels * (wavhead.fmt.BitsPerSample / 8);
    if(byte_rate > 0) {
        duration = wavhead.data.ChunkSize / byte_rate;
    }

    f_close(&file);
    return duration;
}

/**
  * @brief  获取音频播放状态
  * @retval 播放状态（AUDIO_STATE_STOP/PLAY/PAUSE）
 */
uint8_t Audio_GetState(void)
{
    return audio_player.state;
}

/**
  * @brief  获取当前播放时间
  * @retval 当前播放时间（秒）
 */
uint32_t Audio_GetCurrentTime(void)
{
    return audio_player.current_time;
}

/**
  * @brief  获取音频文件总时长
  * @retval 总时长（秒）
 */
uint32_t Audio_GetTotalTime(void)
{
    return audio_player.total_time;
}

/**
  * @brief  检查是否正在播放
  * @retval 1=正在播放, 0=未播放（停止或暂停）
 */
uint8_t Audio_IsPlaying(void)
{
    return (audio_player.state == AUDIO_STATE_PLAY);
}
