#ifndef __RECORDER_H
#define __RECORDER_H

#include "main.h"
#include "ff.h"
#include "audio_play.h"  // 使用audio_play.h中已定义的WAV结构体

// 录音DMA缓冲区大小（字节）
// 与例程保持一致：4096字节（4KB）
// 4096字节 = 64ms @ 16kHz双声道16bit
// 内存需求：2×4096 + sizeof(FIL) ≈ 8.2KB（远小于32KB堆内存）
#define RECORDER_RX_DMA_BUF_SIZE    4096  // 4KB，与例程一致

// 注意：WAV文件头结构体（ChunkRIFF, ChunkFMT, ChunkDATA, WaveHeader）已在audio_play.h中定义
// 这里不再重复定义，直接使用audio_play.h中的定义

// 录音状态
typedef enum {
    RECORDER_STATE_IDLE = 0,      // 空闲
    RECORDER_STATE_RECORDING = 1, // 录音中
    RECORDER_STATE_PAUSED = 2      // 暂停
} recorder_state_t;

// 录音机结构体
typedef struct {
    recorder_state_t state;       // 录音状态
    FIL* file;                    // 文件句柄
    WaveHeader wav_header;        // WAV文件头
    uint32_t wav_data_size;       // WAV数据大小（字节，不包括文件头）
    char file_path[64];           // 文件路径
    uint8_t* rx_buf1;             // DMA接收缓冲区1
    uint8_t* rx_buf2;             // DMA接收缓冲区2
} recorder_t;

// 全局录音机实例
extern recorder_t recorder;

// 函数声明
void Recorder_Init(void);
uint8_t Recorder_Start(const char* file_path);
void Recorder_Stop(void);
void Recorder_Pause(void);
void Recorder_Resume(void);
recorder_state_t Recorder_GetState(void);
uint32_t Recorder_GetRecordTime(void);  // 获取录音时长（秒）
void Recorder_DMA_Callback(void);       // DMA中断回调（在中断中调用，只设置标志位）
void Recorder_Process(void);            // 主循环中调用，处理录音数据和文件写入

#endif /* __RECORDER_H */
