#ifndef __AUDIO_PLAY_H
#define __AUDIO_PLAY_H

#include "main.h"
#include "i2s.h"
#include "wm8978.h"
#include "ff.h"  // FATFS文件系统

// WAV DMA缓冲区大小
#define WAV_I2S_TX_DMA_BUFSIZE    8192

// WAV文件块结构
typedef struct __attribute__((packed)) {
    uint32_t ChunkID;
    uint32_t ChunkSize;
    uint32_t Format;
} ChunkRIFF;

typedef struct __attribute__((packed)) {
    uint32_t ChunkID;
    uint32_t ChunkSize;
    uint16_t AudioFormat;
    uint16_t NumOfChannels;
    uint32_t SampleRate;
    uint32_t ByteRate;
    uint16_t BlockAlign;
    uint16_t BitsPerSample;
} ChunkFMT;

typedef struct __attribute__((packed)) {
    uint32_t ChunkID;
    uint32_t ChunkSize;
    uint32_t NumOfSamples;
} ChunkFACT;

typedef struct __attribute__((packed)) {
    uint32_t ChunkID;
    uint32_t ChunkSize;
} ChunkDATA;

// 完整WAV文件头
typedef struct __attribute__((packed)) {
    ChunkRIFF riff;
    ChunkFMT fmt;
    ChunkDATA data;
} WaveHeader;

// WAV控制结构
typedef struct __attribute__((packed)) {
    uint16_t audioformat;
    uint16_t nchannels;
    uint16_t blockalign;
    uint32_t datasize;
    uint32_t totsec;
    uint32_t cursec;
    uint32_t bitrate;
    uint32_t samplerate;
    uint16_t bps;
    uint32_t datastart;
} __wavctrl;

// 音频设备结构
typedef struct __attribute__((packed)) {
    uint8_t *i2sbuf1;
    uint8_t *i2sbuf2;
    uint8_t *tbuf;
    FIL *file;
    uint8_t status;
} __audiodev;

extern __audiodev audiodev;
extern __wavctrl wavctrl;
extern volatile uint8_t wavtransferend;
extern volatile uint8_t wavwitchbuf;

// 播放状态
typedef enum {
    AUDIO_STATE_STOP = 0,
    AUDIO_STATE_PLAY,
    AUDIO_STATE_PAUSE
} AudioState_t;

// 音频播放器结构
typedef struct {
    AudioState_t state;
    uint8_t volume;
    uint32_t total_time;
    uint32_t current_time;
} AudioPlayer_t;

extern AudioPlayer_t audio_player;

// 基础函数
void Audio_Init(void);
void audio_start(void);
void audio_stop(void);

// WAV播放函数
uint8_t wav_decode_init(uint8_t* fname, __wavctrl* wavx);
uint32_t wav_buffill(uint8_t *buf, uint16_t size, uint8_t bits);
void wav_i2s_dma_tx_callback(void);
void wav_get_curtime(FIL *fx, __wavctrl *wavx);
uint8_t wav_play_song(uint8_t* fname);

// 高级接口
void Audio_Load(const char* filename, uint8_t auto_play);  // 加载音频文件，auto_play=1自动播放，0仅加载
void Audio_Play(const char* filename);  // 播放音频文件（自动播放）
void Audio_Pause(void);
void Audio_Resume(void);
void Audio_Stop(void);
void Audio_SetVolume(uint8_t vol);

// 播放器状态查询
uint8_t Audio_GetState(void);
uint32_t Audio_GetCurrentTime(void);
uint32_t Audio_GetTotalTime(void);
uint8_t Audio_IsPlaying(void);
void Audio_Update(void);

// 获取WAV文件时长（不播放）
uint32_t Audio_GetWavDuration(const char* filename);

// 跳转到指定时间位置
uint8_t Audio_Seek(uint32_t target_time);

#endif
