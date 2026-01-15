#include "recorder.h"
#include "recorder_audio.h"
#include "ff.h"
#include "debug_uart.h"
#include "lvgl.h"
#include "sd_init.h"
#include "fatfs.h"
#include "bsp_driver_sd.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// 错误弹窗回调函数（关闭按钮）
static void recorder_error_msgbox_close_cb(lv_event_t *e)
{
    lv_obj_t *msgbox = lv_event_get_current_target(e);
    lv_obj_t *parent = lv_obj_get_parent(msgbox);
    if(parent) {
        lv_obj_del(parent);  // 删除背景层
    }
    lv_obj_del(msgbox);  // 删除消息框
}

// 显示错误弹窗
static void recorder_show_error(const char *title, const char *message)
{
    // 创建消息框（使用lv_layer_top()作为父对象，确保在最上层显示）
    lv_obj_t *msgbox = lv_msgbox_create(lv_layer_top(), title, message, NULL, true);
    
    // 设置消息框样式
    lv_obj_set_style_bg_color(msgbox, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(msgbox, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(msgbox, 2, 0);
    lv_obj_set_style_border_color(msgbox, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_pad_all(msgbox, 10, 0);
    
    // 设置文本样式
    lv_obj_t *text = lv_msgbox_get_text(msgbox);
    if(text) {
        lv_obj_set_style_text_color(text, lv_color_black(), 0);
        lv_obj_set_style_text_font(text, &lv_font_montserrat_16, 0);
    }
    
    // 设置标题样式
    lv_obj_t *title_obj = lv_msgbox_get_title(msgbox);
    if(title_obj) {
        lv_obj_set_style_text_color(title_obj, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_style_text_font(title_obj, &lv_font_montserrat_18, 0);
    }
    
    // 设置关闭按钮事件
    lv_obj_t *close_btn = lv_msgbox_get_close_btn(msgbox);
    if(close_btn) {
        lv_obj_add_event_cb(close_btn, recorder_error_msgbox_close_cb, LV_EVENT_CLICKED, NULL);
    }
    
    // 居中显示
    lv_obj_center(msgbox);
}

// 全局录音机实例
recorder_t recorder = {
    .state = RECORDER_STATE_IDLE,
    .file = NULL,
    .wav_data_size = 0,
    .file_path = {0},
    .rx_buf1 = NULL,
    .rx_buf2 = NULL
};

// 外部DMA句柄
extern DMA_HandleTypeDef hdma_i2s2_ext_rx;

// DMA中断标志位（在中断中设置，在主循环中处理）
static volatile uint8_t recorder_dma_ready_flag = 0;
static volatile uint8_t* recorder_dma_ready_buf = NULL;

/**
 * @brief 初始化录音机
 */
void Recorder_Init(void)
{
    memset(&recorder, 0, sizeof(recorder_t));
    recorder.state = RECORDER_STATE_IDLE;
    Recorder_Audio_Init();
    // LOGI("[RECORDER] Initialized\r\n");
}

/**
 * @brief 初始化WAV文件头
 * 注意：使用audio_play.h中定义的WaveHeader结构体
 */
static void Recorder_InitWAVHeader(WaveHeader* header)
{
    // RIFF块
    header->riff.ChunkID = 0x46464952;      // "RIFF"
    header->riff.ChunkSize = 0;             // 待更新
    header->riff.Format = 0x45564157;       // "WAVE"
    
    // fmt块
    header->fmt.ChunkID = 0x20746D66;       // "fmt "
    header->fmt.ChunkSize = 16;             // fmt块大小
    header->fmt.AudioFormat = 0x01;         // PCM格式
    header->fmt.NumOfChannels = RECORDER_NUM_CHANNELS;  // 双声道
    header->fmt.SampleRate = RECORDER_SAMPLE_RATE;      // 16kHz
    header->fmt.ByteRate = RECORDER_SAMPLE_RATE * RECORDER_NUM_CHANNELS * (RECORDER_BITS_PER_SAMPLE / 8);  // 64000
    header->fmt.BlockAlign = RECORDER_NUM_CHANNELS * (RECORDER_BITS_PER_SAMPLE / 8);  // 4
    header->fmt.BitsPerSample = RECORDER_BITS_PER_SAMPLE;  // 16bit
    
    // data块
    header->data.ChunkID = 0x61746164;      // "data"
    header->data.ChunkSize = 0;             // 待更新
}

/**
 * @brief 生成新的录音文件名
 * @param file_path: 输出文件路径缓冲区
 * @param max_len: 缓冲区最大长度
 */
static void Recorder_GenerateFileName(char* file_path, uint32_t max_len)
{
    FRESULT res;
    FILINFO fno;
    uint16_t index = 0;
    
    // 确保RECORDER目录存在
    f_mkdir("0:/RECORDER");
    
    // 查找可用的文件名
    while(index < 0xFFFF) {
        snprintf(file_path, max_len, "0:/RECORDER/REC%05d.wav", index);
        res = f_stat(file_path, &fno);
        if(res == FR_NO_FILE) {
            break;  // 文件不存在，可以使用
        }
        index++;
    }
    
    // LOGI("[RECORDER] Generated filename: %s\r\n", file_path);
}

/**
 * @brief 启动录音
 * @param file_path: 文件路径（如果为NULL，自动生成）
 * @return 0=成功，其他=失败
 */
uint8_t Recorder_Start(const char* file_path)
{
    FRESULT res;
    UINT bw;
    
    if(recorder.state != RECORDER_STATE_IDLE) {
        // LOGI("[RECORDER] Cannot start: not idle (state=%d)\r\n", recorder.state);
        return 1;
    }
    
    // 检查SD卡是否就绪
    if(!SD_Init_IsReady()) {
        recorder_show_error("Recording Error", "SD card not ready\nPlease wait for SD card initialization");
        return 2;
    }
    
    // 检查文件系统是否已挂载（重新挂载以确保状态正确）
    res = f_mount(&SDFatFS, SDPath, 1);
    if(res != FR_OK) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Failed to mount SD card\nError code: %d", res);
        recorder_show_error("Recording Error", error_msg);
        return 3;
    }
    
    // LOGI("[RECORDER] Starting recording...\r\n");
    
    // 1. 分配内存
    recorder.rx_buf1 = (uint8_t*)malloc(RECORDER_RX_DMA_BUF_SIZE);
    recorder.rx_buf2 = (uint8_t*)malloc(RECORDER_RX_DMA_BUF_SIZE);
    recorder.file = (FIL*)malloc(sizeof(FIL));
    
        if(!recorder.rx_buf1 || !recorder.rx_buf2 || !recorder.file) {
        // LOGI("[RECORDER] Memory allocation failed\r\n");
        if(recorder.rx_buf1) free(recorder.rx_buf1);
        if(recorder.rx_buf2) free(recorder.rx_buf2);
        if(recorder.file) free(recorder.file);
        recorder_show_error("Recording Error", "Memory allocation failed");
        return 2;
    }
    
    // 2. 生成文件名
    if(file_path == NULL) {
        Recorder_GenerateFileName(recorder.file_path, sizeof(recorder.file_path));
    } else {
        strncpy(recorder.file_path, file_path, sizeof(recorder.file_path) - 1);
        recorder.file_path[sizeof(recorder.file_path) - 1] = '\0';
    }
    
    // 3. 初始化WAV文件头
    Recorder_InitWAVHeader(&recorder.wav_header);
    recorder.wav_data_size = 0;
    
    // 4. 创建并打开文件
    res = f_open(recorder.file, recorder.file_path, FA_CREATE_ALWAYS | FA_WRITE);
    if(res != FR_OK) {
        // LOGI("[RECORDER] Failed to create file: res=%d\r\n", res);
        free(recorder.rx_buf1);
        free(recorder.rx_buf2);
        free(recorder.file);
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Failed to create file\nError code: %d", res);
        recorder_show_error("Recording Error", error_msg);
        return 3;
    }
    
    // 5. 写入WAV文件头（占位，后续会更新）
    // LOGI("[RECORDER] Writing WAV header: size=%d\r\n", sizeof(WaveHeader));
    // LOGI("[RECORDER] Header: SampleRate=%lu, Channels=%d, BitsPerSample=%d\r\n", 
    //      recorder.wav_header.fmt.SampleRate, 
    //      recorder.wav_header.fmt.NumOfChannels,
    //      recorder.wav_header.fmt.BitsPerSample);
    
    res = f_write(recorder.file, &recorder.wav_header, sizeof(WaveHeader), &bw);
    if(res != FR_OK || bw != sizeof(WaveHeader)) {
        // LOGI("[RECORDER] Failed to write header: res=%d, written=%d, expected=%d\r\n", 
        //      res, bw, sizeof(WaveHeader));
        f_close(recorder.file);
        free(recorder.rx_buf1);
        free(recorder.rx_buf2);
        free(recorder.file);
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Failed to write header\nError code: %d", res);
        recorder_show_error("Recording Error", error_msg);
        return 4;
    }
    
    // LOGI("[RECORDER] WAV header written successfully: %d bytes\r\n", bw);
    
    // 6. 配置I2S2ext RX DMA（双缓冲模式）
    I2S2ext_RX_DMA_Init(recorder.rx_buf1, recorder.rx_buf2, RECORDER_RX_DMA_BUF_SIZE / 2);
    
    // 7. 进入录音模式
    Recorder_Audio_EnterRecordMode();
    
    // 8. 启动录音
    Recorder_Audio_Start();
    
    // 9. 更新状态
    recorder.state = RECORDER_STATE_RECORDING;
    
    // LOGI("[RECORDER] Recording started: %s\r\n", recorder.file_path);
    return 0;
}

/**
 * @brief 停止录音
 */
void Recorder_Stop(void)
{
    FRESULT res;
    UINT bw;
    
    if(recorder.state == RECORDER_STATE_IDLE) {
        return;
    }
    
    // LOGI("[RECORDER] Stopping recording...\r\n");
    
    // 1. 停止录音
    Recorder_Audio_Stop();
    Recorder_Audio_ExitRecordMode();
    
    // 2. 更新WAV文件头
    if(recorder.file && recorder.file->obj.fs != NULL) {  // 检查文件是否打开
        // 先声明变量，避免goto跳过初始化
        FSIZE_t actual_file_size = 0;
        uint32_t actual_data_size = 0;
        uint32_t data_size = 0;
        uint32_t file_size = 0;
        uint32_t record_time = 0;
        
        // 先同步文件系统，确保所有数据都已写入
        // LOGI("[RECORDER] Stop: syncing file before updating header...\r\n");
        res = f_sync(recorder.file);
        if(res != FR_OK) {
            // LOGI("[RECORDER] Stop: sync error before header update: res=%d\r\n", res);
            // 如果是文件系统内部错误，尝试关闭文件后重新打开
            if(res == FR_INT_ERR) {
                // LOGI("[RECORDER] Stop: File system error detected, closing and reopening file...\r\n");
                f_close(recorder.file);
                HAL_Delay(100);
                // 尝试重新打开文件
                res = f_open(recorder.file, recorder.file_path, FA_OPEN_EXISTING | FA_WRITE | FA_READ);
                if(res != FR_OK) {
                    // LOGI("[RECORDER] Stop: Failed to reopen file after error: res=%d\r\n", res);
                    // 无法恢复，直接关闭文件
                    if(recorder.file->obj.fs != NULL) {
                        f_close(recorder.file);
                    }
                    goto cleanup;
                }
            }
        }
        
        // 优先使用 recorder.wav_data_size（实际写入的数据大小）
        // 因为 actual_file_size 可能包含 FAT 簇对齐空间，导致文件头不正确
        data_size = recorder.wav_data_size;
        file_size = data_size + sizeof(WaveHeader);
        
        // 验证文件大小是否小于预期（说明有写入失败）
        actual_file_size = f_size(recorder.file);
        if(actual_file_size < file_size) {
            // 文件大小小于预期，说明有写入失败，使用实际文件大小
            actual_data_size = (actual_file_size > sizeof(WaveHeader)) ? 
                                        (actual_file_size - sizeof(WaveHeader)) : 0;
            if(actual_data_size > 0) {
                data_size = actual_data_size;
                file_size = data_size + sizeof(WaveHeader);
            }
        }
        record_time = Recorder_GetRecordTime();
        
        // LOGI("[RECORDER] Stop: recorded_size=%lu, actual_file_size=%lu, actual_data_size=%lu\r\n", 
        //      recorder.wav_data_size, actual_file_size, actual_data_size);
        // LOGI("[RECORDER] Stop: using data_size=%lu bytes (%lu KB), file_size=%lu, time=%lu s\r\n", 
        //      data_size, data_size/1024, file_size, record_time);
        
        // 计算WAV文件头大小
        // RIFF.ChunkSize = 文件总大小 - 8 (不包括RIFF和ChunkSize本身)
        recorder.wav_header.riff.ChunkSize = file_size - 8;
        // data.ChunkSize = 数据大小
        recorder.wav_header.data.ChunkSize = data_size;
        
        // LOGI("[RECORDER] Stop: updating header, riff.ChunkSize=%lu, data.ChunkSize=%lu\r\n", 
        //      recorder.wav_header.riff.ChunkSize, recorder.wav_header.data.ChunkSize);
        
        // 关闭文件，然后重新打开来更新文件头（更可靠的方法）
        f_close(recorder.file);
        HAL_Delay(100);  // 增加延迟，等待文件系统完成关闭操作
        
        // 重新打开文件（读写模式）
        res = f_open(recorder.file, recorder.file_path, FA_OPEN_EXISTING | FA_WRITE | FA_READ);
        if(res == FR_OK) {
            // 回到文件开头，更新文件头
            f_lseek(recorder.file, 0);
            res = f_write(recorder.file, &recorder.wav_header, sizeof(WaveHeader), &bw);
            
            if(res == FR_OK && bw == sizeof(WaveHeader)) {
                // LOGI("[RECORDER] Header updated successfully\r\n");
            } else {
                // LOGI("[RECORDER] Header update failed: res=%d, written=%d, expected=%d\r\n", 
                //      res, bw, sizeof(WaveHeader));
            }
            
            // 最终同步文件系统（确保文件头写入SD卡）
            f_sync(recorder.file);
            
            // 关闭文件
            f_close(recorder.file);
            // LOGI("[RECORDER] File closed successfully\r\n");
        } else {
            // LOGI("[RECORDER] Failed to reopen file for header update: res=%d\r\n", res);
            // 如果重新打开失败，尝试关闭原文件句柄
            if(recorder.file && recorder.file->obj.fs != NULL) {
                f_close(recorder.file);
            }
        }
        
cleanup:
    } else {
        // LOGI("[RECORDER] Stop: file not open, cannot update header\r\n");
        if(recorder.file == NULL) {
            // LOGI("[RECORDER] Stop: file handle is NULL\r\n");
        } else {
            // LOGI("[RECORDER] Stop: file->obj.fs is NULL\r\n");
        }
    }
    
    // 3. 释放内存
    if(recorder.rx_buf1) {
        free(recorder.rx_buf1);
        recorder.rx_buf1 = NULL;
    }
    if(recorder.rx_buf2) {
        free(recorder.rx_buf2);
        recorder.rx_buf2 = NULL;
    }
    if(recorder.file) {
        free(recorder.file);
        recorder.file = NULL;
    }
    
    // 4. 更新状态
    recorder.state = RECORDER_STATE_IDLE;
    recorder.wav_data_size = 0;
    
    // LOGI("[RECORDER] Recording stopped, file saved: %s\r\n", recorder.file_path);
}

/**
 * @brief 暂停录音
 */
void Recorder_Pause(void)
{
    if(recorder.state != RECORDER_STATE_RECORDING) {
        return;
    }
    
    // LOGI("[RECORDER] Pausing recording...\r\n");
    Recorder_Audio_Stop();
    recorder.state = RECORDER_STATE_PAUSED;
}

/**
 * @brief 恢复录音
 */
void Recorder_Resume(void)
{
    if(recorder.state != RECORDER_STATE_PAUSED) {
        return;
    }
    
    // LOGI("[RECORDER] Resuming recording...\r\n");
    Recorder_Audio_Start();
    recorder.state = RECORDER_STATE_RECORDING;
}

/**
 * @brief 获取录音状态
 */
recorder_state_t Recorder_GetState(void)
{
    return recorder.state;
}

/**
 * @brief 获取录音时长（秒）
 */
uint32_t Recorder_GetRecordTime(void)
{
    if(recorder.state == RECORDER_STATE_IDLE) {
        return 0;
    }
    
    // 计算录音时长 = 数据大小 / 字节率
    uint32_t byte_rate = RECORDER_SAMPLE_RATE * RECORDER_NUM_CHANNELS * (RECORDER_BITS_PER_SAMPLE / 8);
    return recorder.wav_data_size / byte_rate;
}

/**
 * @brief DMA中断回调（在中断中调用）
 * 参考例程：rec_i2s_dma_rx_callback()
 * 注意：在中断中只设置标志位，不执行文件写入操作
 */
void Recorder_DMA_Callback(void)
{
    // 只在录音状态下处理
    if(recorder.state != RECORDER_STATE_RECORDING) {
        return;
    }
    
    // 检查DMA当前使用的缓冲区（通过DMA1_Stream3->CR的位19判断）
    // 位19=0：使用M0AR（buf1），位19=1：使用M1AR（buf2）
    if(DMA1_Stream3->CR & (1 << 19)) {
        // 当前使用M1AR（buf2），需要写入buf1
        recorder_dma_ready_buf = recorder.rx_buf1;
    } else {
        // 当前使用M0AR（buf1），需要写入buf2
        recorder_dma_ready_buf = recorder.rx_buf2;
    }
    
    // 设置标志位，让主循环处理文件写入
    recorder_dma_ready_flag = 1;
}

/**
 * @brief 主循环中调用，处理录音数据
 * 处理DMA中断设置的标志位，执行文件写入操作
 */
void Recorder_Process(void)
{
    FRESULT res;
    UINT bw;
    static uint32_t write_count = 0;
    static uint32_t last_sync_time = 0;
    
    // 检查是否有DMA数据就绪标志
    if(recorder_dma_ready_flag && recorder_dma_ready_buf) {
        recorder_dma_ready_flag = 0;  // 清除标志位
        
        // 只在录音状态下写入文件
        if(recorder.state == RECORDER_STATE_RECORDING && 
           recorder.file && recorder.file->obj.fs != NULL) {  // 检查文件是否打开
            // 将volatile指针转换为const指针（f_write需要const void*）
            const uint8_t* buf = (const uint8_t*)recorder_dma_ready_buf;
            
            // 写入策略：直接一次性写入整个缓冲区（4KB）
            // 如果失败，再分块写入（512字节块，SD卡扇区大小）
            uint32_t total_size = RECORDER_RX_DMA_BUF_SIZE;  // 4096字节（4KB）
            uint8_t retry_count = 0;
            const uint8_t max_retries = 3;
            static uint32_t consecutive_errors = 0;
            static uint32_t write_fail_count = 0;  // 写入失败计数器（在函数开始处声明，两个分支都能访问）
            uint32_t remaining = total_size;
            uint8_t write_success = 0;
            
            // 首先尝试一次性写入整个缓冲区（4KB）
            res = f_write(recorder.file, buf, total_size, &bw);
            
            if(res == FR_OK && bw == total_size) {
                // 一次性写入成功
                consecutive_errors = 0;
                recorder.wav_data_size += bw;
                write_success = 1;
            } else {
                // 一次性写入失败，尝试分块写入
                remaining = total_size;
                uint32_t offset = 0;
                uint32_t chunk_size = 512;  // 512字节块（SD卡扇区大小）
                
                while(remaining > 0) {
                    uint32_t to_write = (remaining > chunk_size) ? chunk_size : remaining;
                    res = f_write(recorder.file, buf + offset, to_write, &bw);
                    
                    if(res != FR_OK) {
                        // 写入失败，检查错误类型和SD卡状态
                        // 首先检查SD卡硬件状态
                        uint8_t sd_state = BSP_SD_GetCardState();
                        
                        // res=2 (FR_INT_ERR): 文件系统内部错误，尝试恢复而不是立即停止
                        if(res == FR_INT_ERR) {
                            consecutive_errors++;
                            
                            // 如果SD卡硬件状态异常，尝试重新挂载文件系统
                            if(sd_state != SD_TRANSFER_OK) {
                                // SD卡硬件错误，尝试重新挂载
                                f_close(recorder.file);
                                HAL_Delay(200);
                                FRESULT remount_res = f_mount(&SDFatFS, SDPath, 1);
                                if(remount_res == FR_OK) {
                                    // 重新打开文件（追加模式）
                                    remount_res = f_open(recorder.file, recorder.file_path, FA_OPEN_APPEND | FA_WRITE);
                                    if(remount_res != FR_OK) {
                                        // 重新打开失败，停止录音
                                        char error_msg[128];
                                        snprintf(error_msg, sizeof(error_msg), "SD card error\nFailed to reopen file\nError: %d", remount_res);
                                        recorder_show_error("Recording Error", error_msg);
                                        Recorder_Stop();
                                        break;
                                    }
                                    // 确保文件指针在文件末尾（FA_OPEN_APPEND应该自动定位，但显式确保更可靠）
                                    f_lseek(recorder.file, f_size(recorder.file));
                                    consecutive_errors = 0;  // 恢复成功，重置错误计数
                                }
                            } else {
                                // SD卡硬件正常，只是文件系统错误，尝试同步
                                f_sync(recorder.file);
                                HAL_Delay(100);  // 给文件系统恢复时间
                            }
                            
                            // 如果连续错误太多（超过10次），才停止录音
                            if(consecutive_errors > 10) {
                                char error_msg[128];
                                snprintf(error_msg, sizeof(error_msg), "File system error too many times (%lu)\nSD card state: %d\nRecording stopped", consecutive_errors, sd_state);
                                recorder_show_error("Recording Error", error_msg);
                                Recorder_Stop();
                                break;
                            }
                            
                            // 继续重试，不立即停止
                            if(retry_count < max_retries) {
                                retry_count++;
                                continue;
                            } else {
                                // 重试次数用完，跳过这个缓冲区
                                break;
                            }
                        }
                        
                        // res=1 (FR_DISK_ERR): SD卡硬件错误，需要更长的恢复时间
                        // 其他错误也按相同方式处理
                        if(res == FR_DISK_ERR) {
                            // SD卡硬件错误，检查SD卡状态
                            if(sd_state != SD_TRANSFER_OK) {
                                // SD卡硬件状态异常，尝试重新挂载
                                f_close(recorder.file);
                                HAL_Delay(300);
                                FRESULT remount_res = f_mount(&SDFatFS, SDPath, 1);
                                if(remount_res == FR_OK) {
                                    // 重新打开文件（追加模式）
                                    remount_res = f_open(recorder.file, recorder.file_path, FA_OPEN_APPEND | FA_WRITE);
                                    if(remount_res != FR_OK) {
                                        // 重新打开失败，停止录音
                                        char error_msg[128];
                                        snprintf(error_msg, sizeof(error_msg), "SD card hardware error\nFailed to reopen file\nError: %d", remount_res);
                                        recorder_show_error("Recording Error", error_msg);
                                        Recorder_Stop();
                                        break;
                                    }
                                    // 确保文件指针在文件末尾（FA_OPEN_APPEND应该自动定位，但显式确保更可靠）
                                    f_lseek(recorder.file, f_size(recorder.file));
                                    consecutive_errors = 0;  // 恢复成功，重置错误计数
                                } else {
                                    consecutive_errors++;
                                }
                            } else {
                                // SD卡硬件正常，可能是临时错误，尝试同步
                                FRESULT sync_res = f_sync(recorder.file);
                                HAL_Delay(200);  // SD卡错误，延迟200ms
                                if(sync_res != FR_OK) {
                                    // 同步失败，增加错误计数
                                    consecutive_errors++;
                                } else {
                                    // 同步成功，重置错误计数（因为文件系统正常）
                                    consecutive_errors = 0;
                                }
                            }
                        } else {
                            // 其他错误
                            consecutive_errors++;
                            f_sync(recorder.file);
                            HAL_Delay(50);   // 其他错误，延迟50ms
                        }
                        
                        // 如果连续错误太多（超过20次），停止录音并显示错误
                        if(consecutive_errors > 20) {
                            char error_msg[128];
                            if(res == FR_DISK_ERR) {
                                snprintf(error_msg, sizeof(error_msg), "SD card error too many times (%lu)\nSD state: %d\nRecording stopped", consecutive_errors, sd_state);
                            } else {
                                snprintf(error_msg, sizeof(error_msg), "Write error too many times (%lu)\nError code: %d\nSD state: %d\nRecording stopped", consecutive_errors, res, sd_state);
                            }
                            recorder_show_error("Recording Error", error_msg);
                            Recorder_Stop();
                            break;
                        }
                        
                        // 继续重试
                        if(retry_count < max_retries) {
                            retry_count++;
                            continue;
                        } else {
                            // 重试次数用完，跳过这个缓冲区
                            break;
                        }
                    } else {
                        // 写入成功，重置计数
                        consecutive_errors = 0;
                        retry_count = 0;
                        
                        if(bw != to_write) {
                            remaining -= bw;
                            offset += bw;
                            recorder.wav_data_size += bw;
                        } else {
                            remaining -= bw;
                            offset += bw;
                            recorder.wav_data_size += bw;
                        }
                    }
                }
                
                // 检查是否完全写入
                if(res == FR_OK && remaining == 0) {
                    write_success = 1;
                }
            }
            
            if(write_success) {
                // 写入成功，更新计数
                write_count++;
                consecutive_errors = 0;  // 重置连续错误计数
                write_fail_count = 0;  // 写入成功，重置失败计数
                
                // 减少同步频率，避免频繁同步导致SD卡响应慢
                // 每写入10次或每1000ms同步一次
                uint32_t now = HAL_GetTick();
                if((write_count % 10 == 0) || (now - last_sync_time >= 1000)) {
                    res = f_sync(recorder.file);
                    if(res != FR_OK) {
                        // 同步错误也计入连续错误
                        consecutive_errors++;
                        if(consecutive_errors > 20) {
                            char error_msg[128];
                            snprintf(error_msg, sizeof(error_msg), "File sync error too many times (%lu)\nRecording stopped", consecutive_errors);
                            recorder_show_error("Recording Error", error_msg);
                            Recorder_Stop();
                        }
                    } else {
                        consecutive_errors = 0;  // 同步成功，重置错误计数
                    }
                    last_sync_time = now;
                }
            } else {
                // 写入失败，检查是否应该停止录音
                write_fail_count++;
                
                // 如果连续失败太多次，停止录音
                // 注意：这个计数器会在写入成功时重置，所以只统计连续失败次数
                if(write_fail_count > 50) {  // 连续50次写入失败，停止录音
                    char error_msg[128];
                    snprintf(error_msg, sizeof(error_msg), "Write failed too many times (%lu)\nRecording stopped", write_fail_count);
                    recorder_show_error("Recording Error", error_msg);
                    write_fail_count = 0;  // 重置计数
                    Recorder_Stop();
                }
            }
        } else {
            // 状态异常，但如果是IDLE状态（录音已停止），这是正常的，不输出错误
            if(recorder.state == RECORDER_STATE_IDLE) {
                // 录音已停止，这是正常状态，直接返回
                recorder_dma_ready_buf = NULL;
                return;
            }
        }
        
        recorder_dma_ready_buf = NULL;
    }
}
