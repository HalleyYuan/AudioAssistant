/**
  ******************************************************************************
  * @file    playlist.c
  * @brief   播放列表管理模块实现 - SD卡音频文件扫描和管理
  * @author  项目开发者
  * @date    2025-01-xx
  * @note    支持扫描SD卡指定文件夹中的音频文件（WAV/MP3/FLAC），维护播放列表
  *          支持切换上一首/下一首、设置索引、获取文件信息等功能
  ******************************************************************************
  */

#include "playlist.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* 全局变量 ---------------------------------------------------------*/
Playlist_t g_playlist = {0};  ///< 全局播放列表结构

/* 私有函数声明 ---------------------------------------------------------*/
static uint8_t is_audio_file(const char* filename);

/**
  * @brief  初始化播放列表
  * @retval 0=成功, 1=失败
  */
uint8_t Playlist_Init(void)
{
    Playlist_Clear();
    return Playlist_Scan(MUSIC_PATH);
}

/**
  * @brief  扫描指定路径的音频文件
  * @param  path: 文件夹路径
  * @retval 0=成功, 1=失败
  */
uint8_t Playlist_Scan(const char* path)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;

    Playlist_Clear();

    // 根据路径设置播放列表来源
    if(strstr(path, "RECORDER") != NULL) {
        g_playlist.source = PLAYLIST_SOURCE_RECORDINGS;
    } else {
        g_playlist.source = PLAYLIST_SOURCE_MUSIC;
    }

    res = f_opendir(&dir, path);
    if(res != FR_OK) return 1;

    while(g_playlist.total_count < MAX_PLAYLIST_SIZE)
    {
        res = f_readdir(&dir, &fno);
        if(res != FR_OK || fno.fname[0] == 0) break;

        if(fno.fattrib & AM_DIR) continue;
        if(fno.fname[0] == '.') continue;
        if(!is_audio_file(fno.fname)) continue;

        uint16_t idx = g_playlist.total_count;
        strncpy(g_playlist.files[idx].filename, fno.fname, MAX_FILENAME_LEN-1);
        snprintf(g_playlist.files[idx].fullpath, sizeof(g_playlist.files[idx].fullpath),
                 "%s/%s", path, fno.fname);
        g_playlist.files[idx].filesize = fno.fsize;
        g_playlist.files[idx].is_valid = 1;
        g_playlist.total_count++;
    }

    f_closedir(&dir);
    g_playlist.is_loaded = 1;
    g_playlist.current_index = 0;

    return (g_playlist.total_count > 0) ? 0 : 1;
}

/**
  * @brief  获取播放列表歌曲数量
  * @retval 歌曲数量
  */
uint8_t Playlist_GetCount(void)
{
    return g_playlist.total_count;
}

/**
  * @brief  获取当前播放索引
  * @retval 当前索引
  */
uint16_t Playlist_GetCurrentIndex(void)
{
    return g_playlist.current_index;
}

/**
  * @brief  获取当前播放文件的完整路径
  * @retval 文件路径指针
  */
const char* Playlist_GetCurrentFile(void)
{
    if(!g_playlist.is_loaded || g_playlist.total_count == 0)
        return NULL;

    return g_playlist.files[g_playlist.current_index].fullpath;
}

/**
  * @brief  获取指定索引的文件名
  * @param  index: 文件索引
  * @retval 文件名指针
  */
const char* Playlist_GetFileName(uint16_t index)
{
    if(index >= g_playlist.total_count)
        return NULL;
    
    return g_playlist.files[index].filename;
}

/**
  * @brief  切换到下一首
  * @retval 0=成功, 1=已是最后一首
  */
uint8_t Playlist_Next(void)
{
    if(!g_playlist.is_loaded || g_playlist.total_count == 0)
        return 1;
    
    if(g_playlist.current_index < g_playlist.total_count - 1)
    {
        g_playlist.current_index++;
        return 0;
    }
    
    // 循环播放
    g_playlist.current_index = 0;
    return 0;
}

/**
  * @brief  切换到上一首
  * @retval 0=成功, 1=已是第一首
  */
uint8_t Playlist_Prev(void)
{
    if(!g_playlist.is_loaded || g_playlist.total_count == 0)
        return 1;
    
    if(g_playlist.current_index > 0)
    {
        g_playlist.current_index--;
        return 0;
    }
    
    // 循环播放
    g_playlist.current_index = g_playlist.total_count - 1;
    return 0;
}

/**
  * @brief  设置当前播放索引
  * @param  index: 目标索引
  * @retval 0=成功, 1=索引无效
  */
uint8_t Playlist_SetIndex(uint16_t index)
{
    if(index >= g_playlist.total_count)
        return 1;
    
    g_playlist.current_index = index;
    return 0;
}

/**
  * @brief  清空播放列表
  */
void Playlist_Clear(void)
{
    memset(&g_playlist, 0, sizeof(Playlist_t));
}

/**
  * @brief  检查文件是否为支持的音频文件格式
  * @param  filename: 文件名（包含扩展名）
  * @retval 1=是音频文件, 0=不是
  * @note   当前支持：.wav, .mp3, .flac（不区分大小写）
  */
static uint8_t is_audio_file(const char* filename)
{
    const char* ext = strrchr(filename, '.');
    if(!ext) return 0;
    
    // 转换为小写比较
    char ext_lower[8] = {0};
    for(int i = 0; i < 7 && ext[i]; i++)
        ext_lower[i] = tolower(ext[i]);
    
    if(strcmp(ext_lower, ".wav") == 0) return 1;
    if(strcmp(ext_lower, ".mp3") == 0) return 1;
    if(strcmp(ext_lower, ".flac") == 0) return 1;

    return 0;
}

/**
  * @brief  获取播放列表来源
  * @retval 播放列表来源
  */
PlaylistSource_t Playlist_GetSource(void)
{
    return g_playlist.source;
}
