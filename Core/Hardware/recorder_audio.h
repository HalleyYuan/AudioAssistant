#ifndef __RECORDER_AUDIO_H
#define __RECORDER_AUDIO_H

#include "main.h"
#include "i2s.h"

// 录音音频参数
#define RECORDER_SAMPLE_RATE      16000  // 采样率：16kHz
#define RECORDER_BITS_PER_SAMPLE  16     // 位深度：16bit
#define RECORDER_NUM_CHANNELS     2      // 声道数：双声道

// 函数声明
void Recorder_Audio_Init(void);
void Recorder_Audio_EnterRecordMode(void);  // 进入录音模式（配置WM8978和I2S）
void Recorder_Audio_ExitRecordMode(void);   // 退出录音模式
void Recorder_Audio_Start(void);            // 启动录音（启动I2S和DMA）
void Recorder_Audio_Stop(void);              // 停止录音（停止I2S和DMA）
uint8_t Recorder_Audio_IsRecording(void);   // 检查是否正在录音

#endif /* __RECORDER_AUDIO_H */
