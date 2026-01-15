#ifndef __METRONOME_AUDIO_H
#define __METRONOME_AUDIO_H

#include "main.h"
#include "i2s.h"

// 节拍强度等级（与metronome_audio.h保持一致）
#define METRONOME_STRENGTH_SILENT  0  // 不出声
#define METRONOME_STRENGTH_WEAK    1  // 弱拍
#define METRONOME_STRENGTH_MEDIUM  2  // 中拍
#define METRONOME_STRENGTH_STRONG  3  // 强拍

// 节拍音频参数
#define METRONOME_SAMPLE_RATE      44100  // 采样率
#define METRONOME_BEAT_DURATION_MS 50     // 节拍持续时间（毫秒）
#define METRONOME_BEAT_SAMPLES     (METRONOME_SAMPLE_RATE * METRONOME_BEAT_DURATION_MS / 1000)  // 2205样本

// BPM限制（Kivy思路：样本不依赖BPM，但为了兼容性保留限制）
#define METRONOME_MIN_BPM           10    // 最小BPM（支持10-500范围）
#define METRONOME_MAX_BPM           500   // 最大BPM
#define METRONOME_DEFAULT_BPM       120   // 默认BPM

// 函数声明
void Metronome_Audio_Init(void);
void Metronome_Audio_PlayBeat(uint8_t strength);  // 播放指定强度的节拍（Kivy思路：定时触发播放）
void Metronome_Audio_Stop(void);
uint8_t Metronome_Audio_IsPlaying(void);
void Metronome_Audio_DMA_Callback(void);  // DMA传输完成回调（在中断中调用）
void Metronome_Audio_Process(void);  // 主循环中调用，处理播放请求和完成标志

#endif /* __METRONOME_AUDIO_H */
