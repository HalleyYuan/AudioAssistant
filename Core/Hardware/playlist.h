/**
  ******************************************************************************
  * @file    playlist.h
  * @brief   播放列表管理模块 - 扫描SD卡音频文件
  ******************************************************************************
  */

#ifndef __PLAYLIST_H
#define __PLAYLIST_H

#include "main.h"
#include "ff.h"

/* 配置参数 */
#define MAX_PLAYLIST_SIZE   50      // 最大歌曲数量
#define MAX_FILENAME_LEN    64      // 文件名最大长度
#define MUSIC_PATH          "0:/music"  // 音乐文件夹路径

/* 音频文件信息 */
typedef struct {
    char filename[MAX_FILENAME_LEN];    // 文件名
    char fullpath[MAX_FILENAME_LEN+16]; // 完整路径
    uint32_t filesize;                  // 文件大小(字节)
    uint8_t is_valid;                   // 是否有效
} AudioFileInfo_t;

/* 播放列表来源 */
typedef enum {
    PLAYLIST_SOURCE_MUSIC = 0,      // 来自music文件夹
    PLAYLIST_SOURCE_RECORDINGS = 1  // 来自录音文件夹
} PlaylistSource_t;

/* 播放列表结构 */
typedef struct {
    AudioFileInfo_t files[MAX_PLAYLIST_SIZE];  // 文件列表
    uint16_t total_count;                      // 总文件数
    uint16_t current_index;                    // 当前播放索引
    uint8_t is_loaded;                         // 是否已加载
    PlaylistSource_t source;                   // 播放列表来源
} Playlist_t;

/* 全局播放列表 */
extern Playlist_t g_playlist;

/* 函数声明 */
uint8_t Playlist_Init(void);
uint8_t Playlist_Scan(const char* path);
uint8_t Playlist_GetCount(void);
uint16_t Playlist_GetCurrentIndex(void);
const char* Playlist_GetCurrentFile(void);
const char* Playlist_GetFileName(uint16_t index);
uint8_t Playlist_Next(void);
uint8_t Playlist_Prev(void);
uint8_t Playlist_SetIndex(uint16_t index);
void Playlist_Clear(void);
PlaylistSource_t Playlist_GetSource(void);  // 获取播放列表来源

#endif /* __PLAYLIST_H */
