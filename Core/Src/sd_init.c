/**
  ******************************************************************************
  * @file    sd_init.c
  * @brief   SD卡异步初始化模块实现
  * @author  项目开发者
  * @date    2025-01-xx
  ******************************************************************************
  */

#include "sd_init.h"
#include "../Hardware/playlist.h"
#include "debug_uart.h"
#include <string.h>

/* 私有变量 */
static SD_InitState_t sd_init_state = SD_INIT_IDLE;
static uint32_t sd_init_start_time = 0;
static uint32_t sd_init_timeout = 0;
static uint8_t sd_init_delay_done = 0;

/* 私有函数声明 */
static uint32_t SD_Init_GetTick(void);
static void SD_Init_StateMachine(void);

/**
  * @brief  获取当前系统tick（毫秒）
  * @retval 当前tick值
  */
static uint32_t SD_Init_GetTick(void)
{
    return HAL_GetTick();
}

/**
  * @brief  SD卡初始化状态机
  * @retval None
  */
static void SD_Init_StateMachine(void)
{
    uint32_t current_time = SD_Init_GetTick();
    FRESULT res;
    
    switch(sd_init_state)
    {
        case SD_INIT_IDLE:
            /* 等待延迟时间，确保系统稳定 */
            if(!sd_init_delay_done) {
                sd_init_start_time = current_time;
                sd_init_delay_done = 1;
            }
            
            if(current_time - sd_init_start_time >= SD_INIT_DELAY_MS) {
                LOGI("[SD_INIT] Starting SD card mount...\r\n");
                sd_init_state = SD_INIT_MOUNTING;
                sd_init_start_time = current_time;
                sd_init_timeout = SD_MOUNT_TIMEOUT_MS;
            }
            break;
            
        case SD_INIT_MOUNTING:
            /* 检查超时 */
            if(current_time - sd_init_start_time >= sd_init_timeout) {
                LOGI("[SD_INIT] Mount timeout after %lu ms\r\n", sd_init_timeout);
                sd_init_state = SD_INIT_TIMEOUT;
                break;
            }
            
            /* 尝试挂载SD卡（非阻塞，快速检查） */
            res = f_mount(&SDFatFS, SDPath, 1);
            if(res == FR_OK) {
                LOGI("[SD_INIT] SD card mounted successfully\r\n");
                sd_init_state = SD_INIT_SCANNING;
                sd_init_start_time = current_time;
                sd_init_timeout = SD_SCAN_TIMEOUT_MS;
            } else if(res == FR_DISK_ERR || res == FR_NOT_READY) {
                /* SD卡硬件错误或未就绪，继续重试（在超时范围内） */
                /* 每500ms输出一次状态，避免刷屏 */
                static uint32_t last_log_time = 0;
                if(current_time - last_log_time >= 500) {
                    LOGI("[SD_INIT] Retrying mount... (err: %d)\r\n", res);
                    last_log_time = current_time;
                }
            } else {
                /* 其他错误（如FR_NO_FILESYSTEM），立即失败 */
                LOGI("[SD_INIT] Mount failed: %d\r\n", res);
                sd_init_state = SD_INIT_FAILED;
            }
            break;
            
        case SD_INIT_SCANNING:
            /* 检查超时 */
            if(current_time - sd_init_start_time >= sd_init_timeout) {
                LOGI("[SD_INIT] Scan timeout!\r\n");
                sd_init_state = SD_INIT_TIMEOUT;
                break;
            }
            
            /* 扫描播放列表 */
            if(Playlist_Init() == 0) {
                uint8_t count = Playlist_GetCount();
                LOGI("[SD_INIT] Playlist initialized, found %d songs\r\n", count);
                sd_init_state = SD_INIT_SUCCESS;
            } else {
                /* 扫描失败，可能是没有文件或文件系统错误 */
                LOGI("[SD_INIT] Playlist scan failed or no songs found\r\n");
                /* 即使没有歌曲，也认为初始化成功（SD卡已挂载） */
                sd_init_state = SD_INIT_SUCCESS;
            }
            break;
            
        case SD_INIT_SUCCESS:
        case SD_INIT_FAILED:
        case SD_INIT_TIMEOUT:
            /* 最终状态，不再处理 */
            break;
    }
}

/**
  * @brief  启动SD卡异步初始化
  * @retval None
  * @note   在LVGL和UI初始化完成后调用
  */
void SD_Init_Start(void)
{
    /* 如果已经初始化完成或正在初始化，不重复启动 */
    if(sd_init_state == SD_INIT_IDLE || 
       sd_init_state == SD_INIT_SUCCESS ||
       sd_init_state == SD_INIT_FAILED ||
       sd_init_state == SD_INIT_TIMEOUT) {
        /* 重置状态，重新开始初始化 */
        sd_init_state = SD_INIT_IDLE;
        sd_init_delay_done = 0;
        sd_init_start_time = 0;
        LOGI("[SD_INIT] Async initialization started\r\n");
    }
    /* 如果正在初始化中（MOUNTING或SCANNING），不重复启动 */
}

/**
  * @brief  更新SD卡初始化状态（在主循环中调用）
  * @retval None
  * @note   每次调用只执行少量工作，避免阻塞主循环
  */
void SD_Init_Update(void)
{
    /* 只在非最终状态时更新 */
    if(sd_init_state != SD_INIT_SUCCESS && 
       sd_init_state != SD_INIT_FAILED && 
       sd_init_state != SD_INIT_TIMEOUT) {
        SD_Init_StateMachine();
    }
}

/**
  * @brief  获取当前初始化状态
  * @retval 当前状态
  */
SD_InitState_t SD_Init_GetState(void)
{
    return sd_init_state;
}

/**
  * @brief  检查SD卡是否已就绪
  * @retval 1=就绪, 0=未就绪
  */
uint8_t SD_Init_IsReady(void)
{
    return (sd_init_state == SD_INIT_SUCCESS);
}

/**
  * @brief  获取播放列表歌曲数量
  * @retval 歌曲数量（仅在就绪时有效，否则返回0）
  */
uint8_t SD_Init_GetPlaylistCount(void)
{
    if(SD_Init_IsReady()) {
        return Playlist_GetCount();
    }
    return 0;
}
