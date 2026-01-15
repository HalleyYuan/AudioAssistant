/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    : main.c
 * @brief   : Main program body
 * @author  : Zeruns
 * @brief   : 基于STM32F407的LVGL工程模板（MSP3526屏幕）
 * @version : 0.1
 * @date    : 2024-05-21
 * @作者个人博客  : https://blog.zeruns.tech
 * @B站空间       : https://space.bilibili.com/8320520
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 *    硬件接线：
 *    LCD               MCU           功能描述
 * ----------------------------------------------------------------------------
 *    GND       ->      GND     ->    LCD屏电源地     
 *    VCC       ->      5V      ->    LCD屏电源正极
 *    LCD_CS    ->      PE6     ->    LCD屏SPI片选
 *    LCD_RST   ->      PC1     ->    LCD屏复位
 *    LCD_RS    ->      PC0     ->    LCD屏命令/数据选择
 *    SDI(MOSI) ->      PB5     ->    LCD屏SPI数据线，主机输出从机输入
 *    SCK       ->      PB3     ->    LCD屏SPI时钟线
 *    LED       ->      3.3V（可以自己接IO口，用软件控制背光）
 *    SDO(MISO) ->      PB4     ->    LCD屏SPI数据线，主机输入从机输出
 *    CTP_SCL   ->      PB8     ->    电容触摸屏控制器I2C时钟线
 *    CTP_RST   ->      PB7     ->    电容触摸屏控制器复位
 *    CTP_SDA   ->      PB9     ->    电容触摸屏控制器I2C数据线
 *    CTP_INT   ->      PB6     ->    电容触摸屏控制器中断信号
 ****************************************************************************** 
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "crc.h"
#include "dma.h"
#include "fatfs.h"
#include "i2c.h"
#include "i2s.h"
#include "rtc.h"
#include "sdio.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "LCD.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "touch.h"
#include "FT6336.h"
#include "debug_uart.h"
#include "music_assistant.h"
#include "playlist.h"
#include "audio_play.h"
#include "sd_init.h"
#include "wm8978.h"
#include "fatfs.h"
#include "ff.h"
#include "bluetooth_power.h"
#include "recorder.h"
#include "power_management.h"
#include <string.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile uint16_t ms_cnt_1 = 0;      ///< 毫秒计时变量1（用于500ms LED闪烁）
volatile uint16_t ms_cnt_2 = 0;      ///< 毫秒计时变量2（用于5ms LVGL任务处理）
volatile uint16_t heartbeat_ms = 0;  ///< 系统心跳计时，用于心跳灯（PF9，1Hz闪烁）
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  MX_TIM3_Init();
  MX_CRC_Init();
  MX_SPI1_Init();
  MX_SDIO_SD_Init();
  MX_I2S2_Init();
  MX_FATFS_Init();
  MX_I2C2_Init();
  MX_RTC_Init();
  /* USER CODE BEGIN 2 */
  /* 先输出最基本的启动信息，确认系统能运行 */
  /*HAL_UART_Transmit(&huart1, (uint8_t *)"System Starting...\r\n", 20, 1000);
  HAL_Delay(10);*/
  
  HAL_TIM_Base_Start_IT(&htim3);  ///< 启动定时器3，1ms中断（用于系统时基和LVGL tick）
  /*HAL_UART_Transmit(&huart1, (uint8_t *)"Timer started\r\n", 16, 1000);
  HAL_Delay(10);*/
  
  /* 初始化WM8978音频芯片 */
  if(WM8978_Init() == 0) {
    /*HAL_UART_Transmit(&huart1, (uint8_t *)"WM8978 initialized\r\n", 21, 1000);*/
    /* 初始设置：耳机音量50%，扬声器音量30% */
    WM8978_HPvol_Set(50, 50);
    WM8978_SPKvol_Set(30);
  } else {
    /*HAL_UART_Transmit(&huart1, (uint8_t *)"WM8978 init failed!\r\n", 22, 1000);*/
  }
  /*HAL_Delay(10);*/
  
  /* 初始化蓝牙电源控制（默认断电状态） */
  BT_Power_Init();
  /*HAL_UART_Transmit(&huart1, (uint8_t *)"Bluetooth power control initialized\r\n", 40, 1000);
  HAL_Delay(10);*/
  
  /* 初始化密码管理模块 */
  extern void PasswordManager_Init(void);
  PasswordManager_Init();
  
  /* 初始化节拍器 */
  extern void Metronome_Init(void);
  Metronome_Init();
  /*HAL_UART_Transmit(&huart1, (uint8_t *)"Metronome initialized\r\n", 24, 1000);
  HAL_Delay(10);*/
  
  /* 初始化录音机 */
  extern void Recorder_Init(void);
  Recorder_Init();
  /*HAL_UART_Transmit(&huart1, (uint8_t *)"Recorder initialized\r\n", 24, 1000);
  HAL_Delay(10);*/
  
  /* 暂时注释掉SD卡和播放列表初始化，避免阻塞 */
  /*
  FRESULT res;
  res = f_mount(&SDFatFS, SDPath, 1);
  if(res == FR_OK)
  {
    LOGI("SD Card mounted successfully\r\n");
    
    if(Playlist_Init() == 0)
    {
      LOGI("Playlist initialized, found %d songs\r\n", Playlist_GetCount());
    }
    else
    {
      LOGI("Playlist init failed or no songs found\r\n");
    }
  }
  else
  {
    LOGI("SD Card mount failed: %d\r\n", res);
  }
  */
  
  /*HAL_UART_Transmit(&huart1, (uint8_t *)"Initializing LVGL...\r\n", 23, 1000);
  HAL_Delay(10);*/
  
  /*LOGI("System boot.\r\n");
  LOGI("Hello World!\r\n");*/       // 上电串口输出
  lv_init();                     // LVGL初始化
  lv_port_disp_init();           // LVGL显示初始化
  lv_port_indev_init();          // LVGL输入设备初始化

  /*HAL_UART_Transmit(&huart1, (uint8_t *)"LVGL initialized\r\n", 19, 1000);
  HAL_Delay(10);*/

  /* 初始化主菜单界面 */
  music_assistant_init();
  /* 如果开启了密码保护：上电也先解锁，再进入主菜单 */
  extern void music_assistant_show_password_unlock(void);
  music_assistant_show_password_unlock();
  
  /*HAL_UART_Transmit(&huart1, (uint8_t *)"UI initialized\r\n", 17, 1000);
  HAL_Delay(10);*/
  
  /* 初始化电源管理模块 */
  Power_Init();
  
  /* 启动SD卡异步初始化（延迟初始化，不阻塞启动） */
  SD_Init_Start();
  /*HAL_UART_Transmit(&huart1, (uint8_t *)"SD init started (async)\r\n", 26, 1000);*/
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  /* 使用阻塞发送代替 USART1_Printf，避免 DMA 阻塞问题 */
  /*char lcd_msg[50];
  sprintf(lcd_msg, "LCD ID:%d\r\n", LCD_Read_ID());
  HAL_UART_Transmit(&huart1, (uint8_t *)lcd_msg, strlen(lcd_msg), 1000);
  
  char entry_msg[] = "[MAIN] Entering main loop...\r\n";
  HAL_UART_Transmit(&huart1, (uint8_t *)entry_msg, strlen(entry_msg), 1000);*/

  while (1)
  {
    /* [MAIN] Loop running 输出已注释，避免刷屏 */
    /*
    static uint32_t loop_entry_count = 0;
    if (++loop_entry_count % 10000 == 0)
    {
      char quick_msg[80];
      sprintf(quick_msg, "[MAIN] Loop running, entry=%lu\r\n", loop_entry_count);
      HAL_UART_Transmit(&huart1, (uint8_t *)quick_msg, strlen(quick_msg), 1000);
    }
    */

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* SD卡异步初始化更新（在主循环中执行，不阻塞） */
    SD_Init_Update();
    
    /* 音频播放状态更新（需要在主循环中定期调用，每次循环都执行） */
    Audio_Update();  ///< 更新音频播放状态，填充DMA缓冲区，检测播放结束
    
    /* 触摸扫描任务：检查中断标志位，在主循环里安全执行I2C扫描 */
    TP_Scan_Task();  ///< 触摸屏扫描任务（非阻塞方式，检查中断标志后执行I2C读取）
    
    /* 电源管理触摸扫描任务：检测触摸长按3秒唤醒 */
    Power_TouchScanTask();  ///< 电源管理触摸扫描任务（检测长按3秒）
    
    /* LVGL 刷新：50ms（移植自v1.0，与Player_Update同步） */
    if (ms_cnt_2 >= 50)
    {
      ms_cnt_2 = 0;      // 计时清零
      lv_task_handler(); // LVGL任务处理
      
      /* 更新播放器状态（移植自v1.0，在LVGL刷新块内调用，确保UI同步） */
      Player_Update();   ///< 500ms更新频率（内部节流），但每50ms检查一次
      
      /* 节拍器处理 */
      extern void Metronome_Audio_Process(void);
      extern void Metronome_CheckBeatStrengthChange(void);
      Metronome_Audio_Process();              // 处理音频播放请求和完成标志
      Metronome_CheckBeatStrengthChange();    // 检查节拍强度变化
      
      /* 更新节拍器UI显示 */
      extern void update_metronome_display(void);
      update_metronome_display();
      
      // 更新Practice计时器
      if(current_page == PAGE_PRACTICE)
      {
          extern void Practice_Timer_Update(void);
          Practice_Timer_Update();
      }
      
      /* 录音机处理 */
      extern void Recorder_Process(void);
      Recorder_Process();  // 处理录音数据写入
      
      /* 更新录音机UI显示 */
      extern page_t current_page;
      if(current_page == 5) {  // PAGE_RECORDER
        extern recorder_state_t Recorder_GetState(void);
        extern uint32_t Recorder_GetRecordTime(void);
        
        recorder_state_t state = Recorder_GetState();
        uint32_t record_time = Recorder_GetRecordTime();
        
        // 更新UI显示
        lv_obj_t *screen = lv_scr_act();
        uint32_t child_cnt = lv_obj_get_child_cnt(screen);
        for(uint32_t i = 0; i < child_cnt; i++) {
          lv_obj_t *child = lv_obj_get_child(screen, i);
          void *user_data = lv_obj_get_user_data(child);
          if(user_data) {
            if(strcmp((char*)user_data, "status") == 0) {
              const char* status_text = "Ready";
              if(state == RECORDER_STATE_RECORDING) {
                status_text = "Recording";
              } else if(state == RECORDER_STATE_PAUSED) {
                status_text = "Paused";
              }
              lv_label_set_text(child, status_text);
            } else if(strcmp((char*)user_data, "time") == 0) {
              uint32_t minutes = record_time / 60;
              uint32_t seconds = record_time % 60;
              lv_label_set_text_fmt(child, "%02lu:%02lu", minutes, seconds);
            } else if(strcmp((char*)user_data, "play") == 0) {
              lv_obj_t *play_label = lv_obj_get_child(child, 0);
              if(play_label) {
                if(state == RECORDER_STATE_RECORDING) {
                  lv_label_set_text(play_label, LV_SYMBOL_PAUSE);
                } else {
                  lv_label_set_text(play_label, LV_SYMBOL_PLAY);
                }
              }
            }
          }
        }
      }
    }
    
    if (ms_cnt_1 >= 500) // 判断是否计时到500ms
    {
      ms_cnt_1 = 0;                                 // 计时清零
      HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin); // LED2电平翻转
      
      /* [LOOP] 输出已注释，避免刷屏 */
      /*
      static uint32_t loop_count = 0;
      loop_count++;
      char status_msg[150];
      sprintf(status_msg, "[LOOP] count=%lu, USART1 gState=0x%02X, RxState=0x%02X\r\n", 
              loop_count, huart1.gState, huart1.RxState);
      HAL_UART_Transmit(&huart1, (uint8_t *)status_msg, strlen(status_msg), 1000);
      */
    }
    if (heartbeat_ms >= 500) // 心跳灯 1Hz 闪烁（PF9，低电平点亮）
    {
      heartbeat_ms = 0;
      HAL_GPIO_TogglePin(LED_RUN_GPIO_Port, LED_RUN_Pin);
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enables the Clock Security System
  */
  HAL_RCC_EnableCSS();
}

/* USER CODE BEGIN 4 */
/**
  * @brief  定时器周期中断回调函数
  * @param  htim: 定时器句柄指针
  * @retval None
  * @note   TIM3定时器1ms中断回调，更新系统计时变量和LVGL时基
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM3)  ///< 定时器TIM3，中断周期1ms
  {
    ms_cnt_1++;        ///< 500ms计时变量（用于LED闪烁）
    ms_cnt_2++;        ///< 5ms计时变量（用于LVGL任务处理）
    heartbeat_ms++;    ///< 心跳灯计时变量（1Hz闪烁）
    lv_tick_inc(1);    ///< LVGL时基更新（1ms）
  }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
