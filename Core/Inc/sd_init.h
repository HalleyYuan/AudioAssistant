/**
  ******************************************************************************
  * @file    sd_init.h
  * @brief   SD卡异步初始化模块 - 带超时保护和错误处理
  * @author  项目开发者
  * @date    2025-01-xx
  * @note    提供SD卡挂载、播放列表扫描的异步初始化功能
  *          包含超时保护、错误处理，确保SD卡失败不影响系统启动
  ******************************************************************************
  */

#ifndef __SD_INIT_H
#define __SD_INIT_H

#include "main.h"
#include "ff.h"
#include "fatfs.h"

/* SD卡初始化状态 */
typedef enum {
    SD_INIT_IDLE = 0,        ///< 未开始初始化
    SD_INIT_MOUNTING,        ///< 正在挂载SD卡
    SD_INIT_SCANNING,        ///< 正在扫描播放列表
    SD_INIT_SUCCESS,         ///< 初始化成功
    SD_INIT_FAILED,          ///< 初始化失败
    SD_INIT_TIMEOUT          ///< 初始化超时
} SD_InitState_t;

/* SD卡初始化配置 */
#define SD_MOUNT_TIMEOUT_MS      3000    ///< SD卡挂载超时时间（毫秒）
#define SD_SCAN_TIMEOUT_MS       5000    ///< 播放列表扫描超时时间（毫秒）
#define SD_INIT_DELAY_MS         100     ///< 初始化延迟时间（毫秒，等待系统稳定）

/* 函数声明 */
void SD_Init_Start(void);                ///< 启动SD卡异步初始化
void SD_Init_Update(void);               ///< 更新SD卡初始化状态（在主循环中调用）
SD_InitState_t SD_Init_GetState(void);   ///< 获取当前初始化状态
uint8_t SD_Init_IsReady(void);           ///< 检查SD卡是否已就绪（挂载成功且播放列表已加载）
uint8_t SD_Init_GetPlaylistCount(void);  ///< 获取播放列表歌曲数量（仅在就绪时有效）

#endif /* __SD_INIT_H */
