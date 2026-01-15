#ifndef __METRONOME_H
#define __METRONOME_H

#include "main.h"
#include "tim.h"
#include "metronome_audio.h"  // 使用Kivy思路的预存储方案

// 节拍器控制函数
void Metronome_Init(void);
void Metronome_Start(void);
void Metronome_Stop(void);
void Metronome_SetBPM(uint16_t bpm);
uint16_t Metronome_GetBPM(void);
uint8_t Metronome_IsPlaying(void);
uint8_t Metronome_GetCurrentBeat(void);
void Metronome_SetBeatCount(uint8_t beat_count);  // 设置节拍数（2-16）
uint8_t Metronome_GetBeatCount(void);  // 获取节拍数
void Metronome_Update(void);  // 在定时器中断中调用
void Metronome_CheckBeatStrengthChange(void);  // 中优先级改进：检查节拍强度变化（在主循环中调用）

#endif
