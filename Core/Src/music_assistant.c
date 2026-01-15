/**
  ******************************************************************************
  * @file    music_assistant.c
  * @brief   音乐助手主界面和页面管理模块
  * @author  项目开发者
  * @date    2025-01-xx
  * @note    实现主菜单、播放器、播放列表等页面的UI创建和事件处理
  ******************************************************************************
  */

#include "music_assistant.h"
#include "lvgl.h"
#include "lvgl/src/widgets/lv_slider.h"  /* 包含slider头文件，使用lv_slider_is_dragged函数 */
#include "debug_uart.h"
#include "usart.h"
#include "playlist.h"
#include "audio_play.h"
#include "sd_init.h"
#include "bluetooth_power.h"
#include "metronome.h"
#include "password_manager.h"
#include "password_storage.h"
#include "recorder.h"
#include "power_management.h"
#include "practice_timer.h"
#include <string.h>
#include <stdio.h>

/* 外部变量声明 */
extern AudioPlayer_t audio_player;  /* 音频播放器状态（来自audio_play.c） */

/* 全局变量 ---------------------------------------------------------*/
page_t current_page = PAGE_MAIN_MENU;  ///< 当前显示的页面
player_t player = {"No Song", 0, 0, 0, 0, 50};  ///< 播放器状态（移植自v1.0）
metronome_t metronome = {120, 4, 0, 0, {3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};  ///< 节拍器状态（默认120 BPM，4拍，第一拍为强拍）

/* 私有函数声明 ---------------------------------------------------------*/

static void btn_metronome_cb(lv_event_t *e);
static void btn_tuner_cb(lv_event_t *e);
static void btn_player_cb(lv_event_t *e);
static void btn_recorder_cb(lv_event_t *e);
static void btn_practice_cb(lv_event_t *e);
static void btn_audio_test_cb(lv_event_t *e);
static void btn_bluetooth_cb(lv_event_t *e);
static void btn_settings_cb(lv_event_t *e);
static void btn_screen_off_cb(lv_event_t *e);
static void btn_bluetooth_switch_cb(lv_event_t *e);
static void btn_bluetooth_update_status(void);
static void create_bluetooth_page(void);
static void create_settings_page(void);
static void create_password_management_page(void);
static void create_password_input_page(void);
static void btn_password_management_cb(lv_event_t *e);
static void btn_password_switch_cb(lv_event_t *e);
static void btn_set_password_cb(lv_event_t *e);
static void btn_password_num_key_cb(lv_event_t *e);
static void btn_password_del_cb(lv_event_t *e);
static void btn_password_confirm_cb(lv_event_t *e);
static void btn_password_cancel_cb(lv_event_t *e);
static void update_password_display(void);
void btn_back_cb(lv_event_t *e);
static void show_message_box(const char *title, const char *message);  ///< 显示可关闭的弹窗
static void btn_playlist_back_cb(lv_event_t *e);  ///< 播放列表返回按钮回调（返回到播放器）
static void btn_player_prev_cb(lv_event_t *e);
static void btn_player_play_cb(lv_event_t *e);
static void btn_player_next_cb(lv_event_t *e);
static void btn_player_playlist_cb(lv_event_t *e);
static void btn_player_playlist_item_cb(lv_event_t *e);
static void player_progress_slider_cb(lv_event_t *e);  ///< 进度条拖动回调

static void create_player_page_stub(void);
static void create_playlist_page_internal(void);
static void refresh_player_page_if_needed(void);  ///< 检查SD卡初始化状态，必要时刷新播放器页面
static void create_metronome_page(void);  ///< 创建节拍器页面
static void metronome_bpm_slider_cb(lv_event_t *e);  ///< BPM滑动条回调
static void metronome_bpm_minus_cb(lv_event_t *e);   ///< BPM -1 按钮回调
static void metronome_bpm_plus_cb(lv_event_t *e);    ///< BPM +1 按钮回调
static void create_recorder_page(void);  ///< 创建录音机页面
static void recorder_play_pause_cb(lv_event_t *e);  ///< 录音播放/暂停按钮回调
static void recorder_stop_cb(lv_event_t *e);  ///< 录音停止按钮回调
static void recorder_file_item_cb(lv_event_t *e);  ///< 录音文件列表项点击回调
static void recorder_refresh_file_list(void);  ///< 刷新录音文件列表
static void metronome_beat_count_minus_cb(lv_event_t *e);  ///< 节拍数 -1 按钮回调
static void metronome_beat_count_plus_cb(lv_event_t *e);   ///< 节拍数 +1 按钮回调
static void metronome_play_pause_cb(lv_event_t *e);  ///< 播放/暂停按钮回调
static void metronome_beat_strength_cb(lv_event_t *e);  ///< 节拍强度切换回调

/* 播放器页面UI控件指针 ---------------------------------------------------------*/
static lv_obj_t *player_play_label = NULL;        ///< 播放/暂停按钮标签
static lv_obj_t *player_song_label = NULL;        ///< 歌曲名称标签
static lv_obj_t *player_time_label = NULL;        ///< 时间显示标签
static lv_obj_t *player_progress_slider = NULL;   ///< 播放进度条
static uint8_t player_is_playing = 0;             ///< 播放状态标志（仅UI显示用）

/* 进度条拖动保护标志（移植自v1.0） */
static uint8_t slider_updating = 0;  ///< 标志：程序正在更新进度条（避免触发跳转）
static uint8_t slider_dragging = 0;  ///< 标志：用户正在拖动进度条（阻止程序更新）

/* 节拍器页面UI控件指针 */
static lv_obj_t *metronome_bpm_label = NULL;        ///< BPM显示标签
static lv_obj_t *metronome_bpm_slider = NULL;        ///< BPM滑动条
static lv_obj_t *metronome_bpm_btn_minus = NULL;     ///< BPM -1 按钮
static lv_obj_t *metronome_bpm_btn_plus = NULL;       ///< BPM +1 按钮
static lv_obj_t *metronome_beat_count_label = NULL;  ///< 节拍数显示标签
static lv_obj_t *metronome_beat_count_btn_minus = NULL;  ///< 节拍数 -1 按钮
static lv_obj_t *metronome_beat_count_btn_plus = NULL;   ///< 节拍数 +1 按钮
static lv_obj_t *metronome_play_btn = NULL;          ///< 播放/暂停按钮
static lv_obj_t *metronome_play_label = NULL;        ///< 播放/暂停按钮标签
static lv_obj_t *metronome_beat_rects[METRONOME_MAX_BEAT_COUNT] = {NULL};  ///< 节拍点矩形（最多16个）
static lv_obj_t *metronome_beat_segments[METRONOME_MAX_BEAT_COUNT][3] = {{NULL}};  ///< 每个节拍点的三段

/**
  * @brief  记录按钮点击事件到串口
  * @param  name: 按钮名称字符串
  * @retval None
  * @note   使用阻塞发送确保输出，然后使用LOGI宏（可能被DMA阻塞）
  */
static void log_button(const char *name)
{
  /* 先使用阻塞发送测试，确保输出 */
  char test_msg[150];
  sprintf(test_msg, "[BLOCKING] UI: %s button clicked\r\n", name);
  /*HAL_UART_Transmit(&huart1, (uint8_t *)test_msg, strlen(test_msg), 1000);*/
  
  /* 然后使用 LOGI（可能被 DMA 阻塞） */
  LOGI("UI: %s button clicked\r\n", name);
}

/**
  * @brief  弹窗关闭按钮回调
  * @param  e: LVGL事件对象
  * @retval None
  */
static void msgbox_close_cb(lv_event_t *e)
{
  lv_obj_t *msgbox = lv_event_get_current_target(e);
  lv_obj_t *parent = lv_obj_get_parent(msgbox);
  if(parent) {
    lv_obj_del(parent);  // 删除背景层
  }
  lv_obj_del(msgbox);  // 删除消息框
}

/**
  * @brief  显示可关闭的弹窗
  * @param  title: 弹窗标题
  * @param  message: 弹窗消息内容
  * @retval None
  */
static void show_message_box(const char *title, const char *message)
{
  // 创建消息框（使用lv_layer_top()作为父对象，确保在最上层显示）
  lv_obj_t *msgbox = lv_msgbox_create(lv_layer_top(), title, message, NULL, true);
  
  // 设置消息框样式
  lv_obj_set_style_bg_color(msgbox, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(msgbox, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(msgbox, 2, 0);
  lv_obj_set_style_border_color(msgbox, lv_palette_main(LV_PALETTE_BLUE), 0);
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
    lv_obj_set_style_text_color(title_obj, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_text_font(title_obj, &lv_font_montserrat_18, 0);
  }
  
  // 设置关闭按钮事件
  lv_obj_t *close_btn = lv_msgbox_get_close_btn(msgbox);
  if(close_btn) {
    lv_obj_add_event_cb(close_btn, msgbox_close_cb, LV_EVENT_CLICKED, NULL);
  }
  
  // 居中显示
  lv_obj_center(msgbox);
}

/**
  * @brief  初始化音乐助手主界面（主菜单）
  * @retval None
  * @note   创建主菜单界面，包含9个功能按钮（3x3布局）：节拍器、调音器、播放器、录音器、练琴、音频测试、蓝牙、设置、息屏
  * @author 项目开发者
  * @date   2025-01-xx
  */
void music_assistant_init(void)
{
  current_page = PAGE_MAIN_MENU;

  /* 清空当前屏幕，设置背景色 */
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), 0);

  /* 创建标题 */
  lv_obj_t *title = lv_label_create(lv_scr_act());
  lv_label_set_text(title, "Music Assistant");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);  /* 使用24号字体，与例程一致 */
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

  /* 9 个功能按钮（3x3布局） */
  const char *btn_texts[] = {"Metronome", "Tuner", "Player", "Recorder", "Clock", "Audio Test", "Bluetooth", "Settings", "Screen Off"};
  /* 说明：
   * - LVGL 当前符号集未内置“时钟/音叉”图标，因此使用最接近的内置符号：
   *   - Clock: LV_SYMBOL_BELL（更接近“时间提醒”）
   *   - Tuner: LV_SYMBOL_AUDIO
   */
  const char *btn_symbols[] = {LV_SYMBOL_AUDIO, LV_SYMBOL_BARS, LV_SYMBOL_PLAY,
                               LV_SYMBOL_STOP, LV_SYMBOL_BELL, LV_SYMBOL_REFRESH, LV_SYMBOL_BLUETOOTH, LV_SYMBOL_SETTINGS, LV_SYMBOL_POWER};
  lv_event_cb_t btn_cbs[] = {btn_metronome_cb, btn_tuner_cb, btn_player_cb,
                             btn_recorder_cb, btn_practice_cb, btn_audio_test_cb, btn_bluetooth_cb, btn_settings_cb, btn_screen_off_cb};

  /* 使用当前分辨率计算居中位置，避免写死 320 */
  lv_coord_t hor_res = lv_disp_get_hor_res(NULL);

  for (int i = 0; i < 9; i++)
  {
    int row = i / 3;
    int col = i % 3;

    int btn_width = 95;
    int btn_height = 100;
    int btn_spacing_x = 10;
    int btn_spacing_y = 10;
    int start_x = (hor_res - (btn_width * 3 + btn_spacing_x * 2)) / 2;
    int start_y = 60;

    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, btn_width, btn_height);
    lv_obj_set_pos(btn, start_x + col * (btn_width + btn_spacing_x), start_y + row * (btn_height + btn_spacing_y));
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    if (btn_cbs[i] != NULL)
      lv_obj_add_event_cb(btn, btn_cbs[i], LV_EVENT_CLICKED, NULL);

    /* 图标 */
    lv_obj_t *symbol = lv_label_create(btn);
    lv_label_set_text(symbol, btn_symbols[i]);
    lv_obj_set_style_text_font(symbol, &lv_font_montserrat_24, 0);  /* 使用24号字体，与例程一致 */
    lv_obj_align(symbol, LV_ALIGN_CENTER, 0, -15);

    /* 文本 */
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, btn_texts[i]);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);  /* 使用12号字体，与例程一致 */
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 25);
  }
}

/**
  * @brief  节拍器按钮点击回调
  * @param  e: LVGL事件对象
  * @retval None
  * @note   当前仅记录日志，功能待实现
  */
static void btn_metronome_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  current_page = PAGE_METRONOME;
  log_button("Metronome");
  create_metronome_page();
}

/**
  * @brief  调音器按钮点击回调
  * @param  e: LVGL事件对象
  * @retval None
  * @note   显示功能尚待开发的弹窗
  */
static void btn_tuner_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  log_button("Tuner");
  show_message_box("Tuner", "Feature under development");
}

/**
  * @brief  播放器按钮点击回调
  * @param  e: LVGL事件对象
  * @retval None
  * @note   打开播放器页面，如果播放列表未初始化则初始化，并加载第一首歌曲
  */
static void btn_player_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  
  /* 按钮防抖：防止重复触发 */
  static uint32_t last_click_time = 0;
  uint32_t now = HAL_GetTick();
  if(now - last_click_time < 300) {  /* 300ms防抖时间 */
    return;
  }
  last_click_time = now;
  
  /* 立即输出，不依赖任何函数 */
  char msg[] = "!!! PLAYER BUTTON CLICKED !!!\r\n";
  /*HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), 1000);*/
  
  current_page = PAGE_AUDIO_PLAYER;
  log_button("Player");
  
  LOGI("[PLAYER] Entering player page\r\n");
  
  /* 检查SD卡初始化状态 */
  uint8_t sd_ready = SD_Init_IsReady();
  SD_InitState_t sd_state = SD_Init_GetState();
  LOGI("[PLAYER] SD card state: %d, ready: %d\r\n", sd_state, sd_ready);
  
  /* 如果SD卡未就绪，显示加载提示 */
  if(!sd_ready) {
    LOGI("[PLAYER] SD card not ready, showing loading message\r\n");
    /* 打开播放器页面（会显示加载提示） */
    create_player_page_stub();
    return;  /* 等待SD卡初始化完成后再加载歌曲 */
  }
  
  /* SD卡已就绪，尝试初始化播放列表 */
  uint8_t playlist_count = Playlist_GetCount();
  LOGI("[PLAYER] Current playlist count: %d\r\n", playlist_count);
  
  if(playlist_count == 0) {
    LOGI("[PLAYER] Playlist empty, initializing...\r\n");
    uint8_t init_result = Playlist_Init();
    if(init_result != 0) {
      LOGI("[PLAYER] Playlist_Init() failed: %d\r\n", init_result);
      /* 初始化失败，显示错误提示 */
      create_player_page_stub();
      if(player_song_label != NULL) {
        lv_label_set_text(player_song_label, "No songs found");
      }
      return;
    }
    playlist_count = Playlist_GetCount();
    LOGI("[PLAYER] Playlist initialized, found %d songs\r\n", playlist_count);
  }
  
  /* 打开播放器页面 */
  create_player_page_stub();
  
  /* 强制刷新显示，确保UI立即显示 */
  lv_refr_now(NULL);
  
  /* 如果有歌曲，加载第一首 */
  if(playlist_count > 0) {
    LOGI("[PLAYER] Loading first song...\r\n");
    /* 确保当前索引为0（第一首） */
    Playlist_SetIndex(0);
    
    /* 获取第一首歌曲的文件路径 */
    const char *filepath = Playlist_GetCurrentFile();
    if(filepath != NULL) {
      LOGI("[PLAYER] Loading file: %s\r\n", filepath);
      /* 先停止并释放之前的资源（如果有） */
      Audio_Stop();
      HAL_Delay(50);  /* 等待资源释放完成 */
      /* 加载歌曲（不自动播放） */
      Audio_Load(filepath, 0);
      LOGI("[PLAYER] Audio_Load() called\r\n");
      
      /* 更新UI显示（移植自v1.0，同时更新player结构体和UI标签） */
      uint16_t current_index = Playlist_GetCurrentIndex();
      const char *filename = Playlist_GetFileName(current_index);
        if(filename != NULL) {
        /* 更新player结构体（移植自v1.0） */
        strncpy(player.song_name, filename, sizeof(player.song_name)-1);
        player.song_name[sizeof(player.song_name)-1] = '\0';
        
        /* 更新UI标签 */
        if(player_song_label != NULL) {
          lv_label_set_text(player_song_label, filename);
          /* 强制刷新显示 */
          lv_obj_invalidate(player_song_label);
        }
        LOGI("[PLAYER] Song name updated: %s (index: %d)\r\n", filename, current_index);
      } else {
        LOGI("[PLAYER] Playlist_GetFileName(%d) returned NULL\r\n", current_index);
      }
      
      /* 更新总时长到player结构体（关键：必须同步更新） */
      player.total_time = Audio_GetTotalTime();
      player.current_time = 0;
      player.progress = 0;
      LOGI("[PLAYER] player.total_time=%lu, player.current_time=%lu\r\n", 
           player.total_time, player.current_time);
      
      /* 更新时间显示 */
      if(player_time_label != NULL) {
        uint32_t min = player.total_time / 60;
        uint32_t sec = player.total_time % 60;
        lv_label_set_text_fmt(player_time_label, "%02lu:%02lu / %02lu:%02lu", 0, 0, min, sec);
        LOGI("[PLAYER] Total time: %lu seconds\r\n", player.total_time);
      }
      
      /* 更新进度条 */
      if(player_progress_slider != NULL) {
        lv_slider_set_value(player_progress_slider, 0, LV_ANIM_OFF);
      }
    } else {
      LOGI("[PLAYER] Playlist_GetCurrentFile() returned NULL\r\n");
    }
  } else {
    LOGI("[PLAYER] No songs in playlist\r\n");
  }
}

/**
  * @brief  录音器按钮点击回调
  * @param  e: LVGL事件对象
  * @retval None
  * @note   显示功能尚待开发的弹窗
  */
static void btn_recorder_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  log_button("Recorder");
  show_message_box("Recorder", "Feature under development");
}

/**
  * @brief  练琴记录按钮点击回调
  * @param  e: LVGL事件对象
  * @retval None
  * @note   当前仅记录日志，功能待实现（需要RTC实时时钟+EEPROM存储）
  */
static void btn_practice_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  current_page = PAGE_PRACTICE;
  log_button("Practice");
  Practice_Timer_Init();
}

/**
  * @brief  音频测试按钮点击回调
  * @param  e: LVGL事件对象
  * @retval None
  * @note   显示未有开发者权限的弹窗
  */
static void btn_audio_test_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  log_button("Audio Test");
  show_message_box("Audio Test", "No developer permission");
}

/* 蓝牙页面UI控件指针 */
static lv_obj_t *bt_status_label = NULL;      ///< 蓝牙状态标签
static lv_obj_t *bt_switch_btn = NULL;        ///< 蓝牙开关按钮
static lv_obj_t *bt_switch_label = NULL;      ///< 蓝牙开关按钮标签

/**
  * @brief  创建设置页面
  * @retval None
  * @note   显示设置选项列表
  */
static void create_settings_page(void)
{
  /* 清空当前屏幕 */
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), 0);
  
  /* 获取屏幕尺寸 */
  lv_coord_t hor_res = lv_disp_get_hor_res(NULL);
  lv_coord_t ver_res = lv_disp_get_ver_res(NULL);
  
  /* 顶部标题栏 */
  lv_obj_t *header = lv_obj_create(lv_scr_act());
  lv_obj_set_size(header, hor_res, 50);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, lv_palette_main(LV_PALETTE_BLUE), 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  
  /* 返回按钮 */
  lv_obj_t *btn_back = lv_btn_create(header);
  lv_obj_set_size(btn_back, 60, 40);
  lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 5, 0);
  lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t *back_label = lv_label_create(btn_back);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT);
  lv_obj_center(back_label);
  
  /* 标题文字 */
  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, "Settings");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_center(title);
  
  /* 创建列表容器 */
  lv_obj_t *list = lv_list_create(lv_scr_act());
  lv_obj_set_size(list, hor_res - 20, ver_res - 100);
  lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 60);
  lv_obj_set_style_border_width(list, 1, 0);
  lv_obj_set_style_border_color(list, lv_palette_main(LV_PALETTE_GREY), 0);
  
  /* 添加密码按钮 */
  lv_obj_t *pwd_btn = lv_list_add_btn(list, LV_SYMBOL_SETTINGS, "Password");
  lv_obj_set_style_text_font(pwd_btn, &lv_font_montserrat_16, 0);
  lv_obj_add_event_cb(pwd_btn, btn_password_management_cb, LV_EVENT_CLICKED, NULL);
  
  /* 未来可在此添加其他设置项 */
}

/**
  * @brief  创建蓝牙控制页面
  * @retval None
  * @note   显示蓝牙电源开关控制和状态信息
  */
static void create_bluetooth_page(void)
{
  /* 清空当前屏幕 */
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), 0);
  
  /* 创建标题 */
  lv_obj_t *title = lv_label_create(lv_scr_act());
  lv_label_set_text(title, "Bluetooth Control");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
  
  /* 创建状态标签 */
  bt_status_label = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_font(bt_status_label, &lv_font_montserrat_18, 0);
  lv_obj_align(bt_status_label, LV_ALIGN_CENTER, 0, -50);
  
  /* 创建开关按钮 */
  bt_switch_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(bt_switch_btn, 200, 60);
  lv_obj_align(bt_switch_btn, LV_ALIGN_CENTER, 0, 30);
  lv_obj_add_event_cb(bt_switch_btn, btn_bluetooth_switch_cb, LV_EVENT_CLICKED, NULL);
  
  /* 创建开关按钮标签 */
  bt_switch_label = lv_label_create(bt_switch_btn);
  lv_obj_set_style_text_font(bt_switch_label, &lv_font_montserrat_18, 0);
  lv_label_set_text(bt_switch_label, "Turn On");
  lv_obj_center(bt_switch_label);
  
  /* 创建返回按钮 */
  lv_obj_t *back_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(back_btn, 100, 40);
  lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
  lv_obj_add_event_cb(back_btn, btn_back_cb, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t *back_label = lv_label_create(back_btn);
  lv_label_set_text(back_label, "Back");
  lv_obj_center(back_label);
  
  /* 更新状态显示 */
  btn_bluetooth_update_status();
}

/**
  * @brief  更新蓝牙状态显示
  * @retval None
  * @note   根据当前电源状态更新UI显示
  */
static void btn_bluetooth_update_status(void)
{
  bt_power_state_t state = BT_Power_GetState();
  
  if(bt_status_label != NULL) {
    if(state == BT_POWER_ON) {
      lv_label_set_text(bt_status_label, "Status: Power On");
      lv_obj_set_style_text_color(bt_status_label, lv_color_hex(0x00AA00), 0);  /* 绿色 */
    } else {
      lv_label_set_text(bt_status_label, "Status: Power Off");
      lv_obj_set_style_text_color(bt_status_label, lv_color_hex(0xAA0000), 0);  /* 红色 */
    }
  }
  
  if(bt_switch_label != NULL) {
    if(state == BT_POWER_ON) {
      lv_label_set_text(bt_switch_label, "Turn Off");
    } else {
      lv_label_set_text(bt_switch_label, "Turn On");
    }
  }
}

/**
  * @brief  蓝牙开关按钮点击回调
  * @param  e: LVGL事件对象
  * @retval None
  * @note   切换蓝牙模块电源状态
  */
static void btn_bluetooth_switch_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  
  /* 按钮防抖 */
  static uint32_t last_click_time = 0;
  uint32_t now = HAL_GetTick();
  if(now - last_click_time < 300) {
    return;
  }
  last_click_time = now;
  
  /* 切换电源状态 */
  BT_Power_Toggle();
  bt_power_state_t state = BT_Power_GetState();
  
  // LOGI("[BLUETOOTH] Power %s\r\n", state == BT_POWER_ON ? "ON" : "OFF");  // 注释掉串口调试
  
  /* 声明WM8978配置函数 */
  extern void WM8978_ADDA_Cfg(uint8_t dacen, uint8_t adcen);
  extern void WM8978_Input_Cfg(uint8_t micen, uint8_t lineinen, uint8_t auxen);
  extern void WM8978_Output_Cfg(uint8_t dacen, uint8_t bypassen);
  extern void WM8978_LINEIN_Gain(uint8_t gain);
  extern void WM8978_HPvol_Set(uint8_t voll, uint8_t volr);
  extern void WM8978_SPKvol_Set(uint8_t volx);
  extern AudioPlayer_t audio_player;
  
  /* 如果蓝牙启动，配置WM8978的LINE IN输入 */
  if(state == BT_POWER_ON) {
    /* 配置WM8978为LINE IN输入模式（蓝牙音频通过LINE IN输入） */
    WM8978_ADDA_Cfg(0, 0);        // 禁用DAC和ADC（蓝牙音频不经过ADC）
    WM8978_Input_Cfg(0, 1, 0);    // 使能LINE IN输入，关闭MIC和AUX
    /* 蓝牙播放偏小声：不要额外衰减LINE IN，先用0dB（必要时可再加到+6dB） */
    WM8978_LINEIN_Gain(5);       // LINE IN 0dB增益
    WM8978_Output_Cfg(0, 1);     // 使能BYPASS输出（LINE IN直接输出到功放）
    
    /* 设置合适的音量（使用播放器的音量设置） */
    uint8_t vol = audio_player.volume;
    if(vol == 0) vol = 63;       // 避免未初始化/为0导致蓝牙几乎无声
    WM8978_HPvol_Set(vol, vol);
    WM8978_SPKvol_Set(vol);
    
    HAL_Delay(10);  // 等待配置稳定
  } else {
    /* 蓝牙关闭时，关闭LINE IN输入和BYPASS输出，避免底噪 */
    WM8978_Input_Cfg(0, 0, 0);    // 关闭所有输入（MIC、LINE IN、AUX）
    WM8978_Output_Cfg(0, 0);      // 关闭DAC和BYPASS输出
  }
  
  /* 更新UI显示 */
  btn_bluetooth_update_status();
}

/**
  * @brief  蓝牙按钮点击回调
  * @param  e: LVGL事件对象
  * @retval None
  * @note   打开蓝牙控制页面
  */
static void btn_bluetooth_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  
  /* 按钮防抖 */
  static uint32_t last_click_time = 0;
  uint32_t now = HAL_GetTick();
  if(now - last_click_time < 300) {
    return;
  }
  last_click_time = now;
  
  /* 立即输出，不依赖任何函数 */
  char msg[] = "!!! BLUETOOTH BUTTON CLICKED !!!\r\n";
  /*HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), 1000);*/
  
  current_page = PAGE_BLUETOOTH;
  log_button("Bluetooth");
  
  /* 创建蓝牙控制页面 */
  create_bluetooth_page();
}

/**
  * @brief  设置按钮点击回调
  * @param  e: LVGL事件对象
  * @retval None
  * @note   打开设置页面
  */
static void btn_settings_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  
  /* 按钮防抖 */
  static uint32_t last_click_time = 0;
  uint32_t now = HAL_GetTick();
  if(now - last_click_time < 300) {
    return;
  }
  last_click_time = now;
  
  log_button("Settings");
  current_page = PAGE_SETTINGS;
  create_settings_page();
}

/**
  * @brief  息屏按钮点击回调
  * @param  e: LVGL事件对象
  * @retval None
  * @note   关闭屏幕显示（息屏功能）
  */
static void btn_screen_off_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  
  /* 按钮防抖 */
  static uint32_t last_click_time = 0;
  uint32_t now = HAL_GetTick();
  if(now - last_click_time < 300) {
    return;
  }
  last_click_time = now;
  
  log_button("Screen Off");
  
  /* 调用电源管理模块进入息屏低功耗模式 */
  Power_EnterScreenOff();
}

/**
  * @brief  播放器页面返回按钮点击回调
  * @param  e: LVGL事件对象
  * @retval None
  * @note   返回主菜单，重新初始化主界面
 */
void btn_back_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  log_button("Back");
  
  LOGI("[UI] Back button clicked, current_page=%d\r\n", current_page);
  
  // 如果从节拍器页面返回，先停止节拍器（避免杂音）
  // 注意：这可能是为什么返回时杂音消失的原因 - 返回时会停止节拍器
  if(current_page == PAGE_METRONOME) {
    // LOGI("[UI] Back: From metronome page, calling Metronome_Stop()...\r\n");
    extern void Metronome_Stop(void);
    Metronome_Stop();
    // LOGI("[UI] Back: Metronome_Stop() returned\r\n");
  }
  
  // 如果从录音机页面返回，先停止录音
  if(current_page == PAGE_RECORDER) {
    LOGI("[UI] Back: From recorder page, calling Recorder_Stop()...\r\n");
    extern void Recorder_Stop(void);
    Recorder_Stop();
    LOGI("[UI] Back: Recorder_Stop() returned\r\n");
    
    // 注意：返回时不需要刷新文件列表，因为页面会被销毁
    // 下次进入页面时会自动刷新
  }
  
  // 如果从蓝牙页面返回，关闭LINE IN输入（避免底噪）
  if(current_page == PAGE_BLUETOOTH) {
    bt_power_state_t bt_state = BT_Power_GetState();
    if(bt_state == BT_POWER_ON) {
      // 蓝牙开启时，关闭LINE IN输入和BYPASS输出，避免底噪
      extern void WM8978_Input_Cfg(uint8_t micen, uint8_t lineinen, uint8_t auxen);
      extern void WM8978_Output_Cfg(uint8_t dacen, uint8_t bypassen);
      WM8978_Input_Cfg(0, 0, 0);    // 关闭所有输入（MIC、LINE IN、AUX）
      WM8978_Output_Cfg(0, 0);      // 关闭DAC和BYPASS输出
    }
  }
  
  // 如果从密码管理相关页面返回
  if(current_page == PAGE_SETTINGS_PASSWORD || current_page == PAGE_SETTINGS_PASSWORD_INPUT) {
    // 返回设置页面
    current_page = PAGE_SETTINGS;
    create_settings_page();
    return;
  }
  
  LOGI("[UI] Back: Setting current_page=PAGE_MAIN_MENU, calling music_assistant_init()...\r\n");
  current_page = PAGE_MAIN_MENU;
  music_assistant_init();
  LOGI("[UI] Back: music_assistant_init() returned\r\n");
}

/**
  * @brief  播放列表页面返回按钮点击回调
  * @param  e: LVGL事件对象
  * @retval None
  * @note   返回到播放器界面，保持当前播放状态
  */
static void btn_playlist_back_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  log_button("Playlist Back");
  current_page = PAGE_AUDIO_PLAYER;
  
  /* 重新创建播放器页面，恢复UI显示 */
  /* create_player_page_stub() 会自动从 player 结构体读取当前状态并初始化UI */
  create_player_page_stub();
}

/**
  * @brief  播放器上一曲按钮点击回调
  * @param  e: LVGL事件对象
  * @retval None
  * @note   切换到上一首歌曲，如果正在播放则继续播放新歌曲
  */
static void btn_player_prev_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  
  /* 按钮防抖：防止重复触发 */
  static uint32_t last_click_time = 0;
  uint32_t now = HAL_GetTick();
  if(now - last_click_time < 300) {  /* 300ms防抖时间 */
    return;
  }
  last_click_time = now;
  
  log_button("Player Prev");
  
  /* 移植自v1.0 */
  if(Playlist_Prev() == 0)
  {
    const char* file = Playlist_GetCurrentFile();
    const char* filename = Playlist_GetFileName(Playlist_GetCurrentIndex());
    if(file && filename)
    {
      /* 保存当前播放状态（在停止之前保存） */
      uint8_t was_playing = (Audio_GetState() == AUDIO_STATE_PLAY);
      
      strncpy(player.song_name, filename, sizeof(player.song_name)-1);
      player.song_name[sizeof(player.song_name)-1] = '\0';
      player.current_time = 0;
      player.progress = 0;

      /* 停止当前播放 */
      Audio_Stop();
      HAL_Delay(150);  /* 增加延时，确保资源完全释放 */
      
      /* 根据之前的状态决定是否自动播放 */
      Audio_Load(file, was_playing ? 1 : 0);
      
      /* 等待加载完成（确保文件打开、缓冲区填充、I2S启动都完成） */
      HAL_Delay(100);
      
      /* 更新UI状态（从audio_player结构体直接读取，确保是最新值） */
      player.is_playing = (Audio_GetState() == AUDIO_STATE_PLAY);
      player.total_time = Audio_GetTotalTime();
      player.current_time = 0;  /* 重置当前时间 */
      player.progress = 0;       /* 重置进度 */
      
      create_player_page_stub();
    }
  }
}

/**
  * @brief  播放器播放/暂停按钮点击回调
  * @param  e: LVGL事件对象
  * @retval None
  * @note   根据当前播放状态执行相应操作：停止→播放、播放→暂停、暂停→恢复
  */
static void btn_player_play_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  
  /* 按钮防抖：防止重复触发 */
  static uint32_t last_click_time = 0;
  uint32_t now = HAL_GetTick();
  if(now - last_click_time < 300) {  /* 300ms防抖时间 */
    return;
  }
  last_click_time = now;
  
  log_button("Player Play/Pause");

  /* 获取当前播放状态 */
  uint8_t current_state = Audio_GetState();
  
  if(current_state == AUDIO_STATE_STOP) {
    /* 停止状态：加载并播放当前歌曲 */
    if(Playlist_GetCount() > 0) {
      const char *filepath = Playlist_GetCurrentFile();
      if(filepath != NULL) {
        LOGI("[BTN_PLAY] Before Audio_Load, audio_player.total_time=%lu\r\n", Audio_GetTotalTime());
        Audio_Load(filepath, 1);  // 加载并自动播放
        
        /* 等待加载完成 */
        HAL_Delay(100);  // 增加延时，确保加载完成
        
        LOGI("[BTN_PLAY] After Audio_Load, audio_player.total_time=%lu\r\n", Audio_GetTotalTime());
        
        /* 更新总时长到player结构体（关键：必须先获取总时长） */
        player.total_time = Audio_GetTotalTime();
        player.current_time = 0;
        LOGI("[BTN_PLAY] player.total_time=%lu, player.current_time=%lu\r\n", 
             player.total_time, player.current_time);
        
        /* 更新player结构体播放状态（必须先更新，Player_Update依赖此标志） */
        player.is_playing = (Audio_GetState() == AUDIO_STATE_PLAY);
        
        /* 更新UI显示 */
        if(player_play_label != NULL) {
          player_is_playing = player.is_playing;
          lv_label_set_text(player_play_label, player.is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
        }
        
        /* 更新歌曲名显示（移植自v1.0，同时更新player结构体和UI标签） */
        uint16_t current_index = Playlist_GetCurrentIndex();
        const char *filename = Playlist_GetFileName(current_index);
        if(filename != NULL) {
          /* 更新player结构体（移植自v1.0） */
          strncpy(player.song_name, filename, sizeof(player.song_name)-1);
          player.song_name[sizeof(player.song_name)-1] = '\0';
          
          /* 更新UI标签 */
          if(player_song_label != NULL) {
            lv_label_set_text(player_song_label, filename);
          }
        }
        
        /* 更新时间显示 */
        if(player_time_label != NULL) {
          uint32_t min = player.total_time / 60;
          uint32_t sec = player.total_time % 60;
          lv_label_set_text_fmt(player_time_label, "%02lu:%02lu / %02lu:%02lu", 0, 0, min, sec);
          LOGI("[BTN_PLAY] Time label updated: %02lu:%02lu / %02lu:%02lu\r\n", 0, 0, min, sec);
        }
      }
    }
  } else if(current_state == AUDIO_STATE_PLAY) {
    /* 播放状态：暂停播放 */
    Audio_Pause();
    
    /* 更新player结构体状态 */
    player.is_playing = 0;
    
    if(player_play_label != NULL) {
      player_is_playing = 0;
      lv_label_set_text(player_play_label, LV_SYMBOL_PLAY);
    }
  } else if(current_state == AUDIO_STATE_PAUSE) {
    /* 暂停状态：恢复播放 */
    LOGI("[BTN_PLAY] Resuming from pause, current total_time=%lu\r\n", player.total_time);
    Audio_Resume();
    
    /* 等待恢复完成 */
    HAL_Delay(50);
    
    /* 更新player结构体状态（关键：确保total_time同步） */
    player.is_playing = 1;
    if(player.total_time == 0) {
      /* 如果total_time为0，重新从audio_player读取 */
      player.total_time = Audio_GetTotalTime();
      LOGI("[BTN_PLAY] Fixed player.total_time=%lu from audio_player\r\n", player.total_time);
    }
    
    if(player_play_label != NULL) {
      player_is_playing = 1;
      lv_label_set_text(player_play_label, LV_SYMBOL_PAUSE);
    }
    
    /* 更新时间显示（确保UI同步） */
    if(player_time_label != NULL && player.total_time > 0) {
      uint32_t min = player.total_time / 60;
      uint32_t sec = player.total_time % 60;
      lv_label_set_text_fmt(player_time_label, "%02lu:%02lu / %02lu:%02lu", 
                            player.current_time / 60, player.current_time % 60, min, sec);
      LOGI("[BTN_PLAY] Time label updated after resume: %02lu:%02lu / %02lu:%02lu\r\n", 
           player.current_time / 60, player.current_time % 60, min, sec);
  }
}
}

/**
  * @brief  播放器下一曲按钮点击回调
  * @param  e: LVGL事件对象
  * @retval None
  * @note   切换到下一首歌曲，如果正在播放则继续播放新歌曲
  */
static void btn_player_next_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  
  /* 按钮防抖：防止重复触发 */
  static uint32_t last_click_time = 0;
  uint32_t now = HAL_GetTick();
  if(now - last_click_time < 300) {  /* 300ms防抖时间 */
    return;
  }
  last_click_time = now;
  
  log_button("Player Next");
  
  /* 检查播放列表是否有效 */
  if(Playlist_GetCount() == 0) {
    return;
  }
  
  /* 保存当前播放状态 */
  uint8_t was_playing = (audio_player.state == AUDIO_STATE_PLAY);
  
  /* 切换到下一首 */
  if(Playlist_Next() != 0) {
    /* 如果切换失败（已是最后一首），循环到第一首 */
    Playlist_SetIndex(0);
  }
  
  /* 获取新歌曲文件路径 */
  const char *filepath = Playlist_GetCurrentFile();
  if(filepath == NULL) {
    return;
  }
  
  /* 加载新歌曲：如果之前正在播放，则自动播放；否则仅加载不播放 */
  Audio_Load(filepath, was_playing ? 1 : 0);
  
  /* 更新UI显示（移植自v1.0，同时更新player结构体和UI标签） */
  uint16_t current_index = Playlist_GetCurrentIndex();
  const char *filename = Playlist_GetFileName(current_index);
  if(filename != NULL) {
    /* 更新player结构体（移植自v1.0） */
    strncpy(player.song_name, filename, sizeof(player.song_name)-1);
    player.song_name[sizeof(player.song_name)-1] = '\0';
    
    /* 更新UI标签 */
    if(player_song_label != NULL) {
      lv_label_set_text(player_song_label, filename);
    }
  }
  
  /* 更新时间显示 */
  if(player_time_label != NULL) {
    uint32_t total_time = Audio_GetTotalTime();
    uint32_t min = total_time / 60;
    uint32_t sec = total_time % 60;
    lv_label_set_text_fmt(player_time_label, "%02lu:%02lu / %02lu:%02lu", 0, 0, min, sec);
  }
  
  /* 更新进度条 */
  if(player_progress_slider != NULL) {
    lv_slider_set_value(player_progress_slider, 0, LV_ANIM_OFF);
  }
  
  /* 更新播放按钮图标 */
  if(player_play_label != NULL) {
    player_is_playing = was_playing;
    lv_label_set_text(player_play_label, was_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
  }
}

/**
  * @brief  播放器播放列表按钮点击回调
  * @param  e: LVGL事件对象
  * @retval None
  * @note   切换到播放列表页面，显示所有歌曲列表
  */
static void btn_player_playlist_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  
  /* 按钮防抖：防止重复触发 */
  static uint32_t last_click_time = 0;
  uint32_t now = HAL_GetTick();
  if(now - last_click_time < 300) {  /* 300ms防抖时间 */
    return;
  }
  last_click_time = now;
  
  log_button("Player Playlist");
  LOGI("[PLAYLIST] Button clicked, switching to playlist page...\r\n");
  
  /* 先切换页面状态 */
  current_page = PAGE_PLAYLIST;
  
  /* 创建播放列表页面 */
  create_playlist_page();
  
  /* 测试：不手动刷新，让主循环自然刷新（模仿主菜单的方式） */
  // lv_refr_now(NULL);  // 暂时注释掉
  
  LOGI("[PLAYLIST] Playlist page created, waiting for main loop refresh\r\n");
}

/**
  * @brief  创建播放器页面UI
  * @retval None
  * @note   创建播放器界面，包含歌曲名、进度条、时间显示、控制按钮（播放/暂停/上一曲/下一曲）
  *          以及播放列表按钮。当前版本仅实现UI，实际播放控制功能待完善
 */
static void create_player_page_stub(void)
{
  /* 清屏并设置背景 */
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), 0);

  lv_coord_t hor_res = lv_disp_get_hor_res(NULL);
  lv_coord_t ver_res = lv_disp_get_ver_res(NULL);

  /* 播放器UI重建时，重置播放状态（移植自v1.0） */
  player_is_playing = 0;
  player_play_label = NULL;
  player_song_label = NULL;
  player_time_label = NULL;
  player_progress_slider = NULL;
  slider_dragging = 0;  /* 清除拖动标志 */

  /* 顶部标题栏 */
  lv_obj_t *header = lv_obj_create(lv_scr_act());
  lv_obj_set_size(header, hor_res, 50);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, lv_palette_main(LV_PALETTE_BLUE), 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  /* 返回按钮 */
  lv_obj_t *btn_back = lv_btn_create(header);
  lv_obj_set_size(btn_back, 60, 40);
  lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 5, 0);
  lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *back_label = lv_label_create(btn_back);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT);
  lv_obj_center(back_label);

  /* 标题文字 */
  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, "Music Player");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_center(title);

  /* 歌曲名（移植自v1.0，从player结构体读取） */
  player_song_label = lv_label_create(lv_scr_act());
  if(player.song_name[0] != '\0') {
    lv_label_set_text(player_song_label, player.song_name);
  } else {
  lv_label_set_text(player_song_label, "No Song");
  }
  lv_obj_set_style_text_font(player_song_label, &lv_font_montserrat_16, 0);
  lv_obj_align(player_song_label, LV_ALIGN_TOP_MID, 0, 70);

  /* 进度条 */
  player_progress_slider = lv_slider_create(lv_scr_act());
  lv_obj_set_size(player_progress_slider, hor_res - 40, 10);  /* 高度10px，与v1.0一致 */
  lv_obj_align(player_progress_slider, LV_ALIGN_CENTER, 0, -40);
  lv_slider_set_range(player_progress_slider, 0, 100);
  slider_updating = 1;  /* 设置标志，避免触发跳转 */
  lv_slider_set_value(player_progress_slider, player.progress, LV_ANIM_OFF);
  slider_updating = 0;  /* 清除标志 */
  /* 添加拖动事件回调（监听按下和释放事件，移植自v1.0） */
  lv_obj_add_event_cb(player_progress_slider, player_progress_slider_cb, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(player_progress_slider, player_progress_slider_cb, LV_EVENT_RELEASED, NULL);

  /* 时间显示（移植自v1.0，从player结构体读取） */
  player_time_label = lv_label_create(lv_scr_act());
  lv_label_set_text_fmt(player_time_label, "%02lu:%02lu / %02lu:%02lu",
                        player.current_time/60, player.current_time%60,
                        player.total_time/60, player.total_time%60);
  lv_obj_set_style_text_font(player_time_label, &lv_font_montserrat_16, 0);
  lv_obj_align(player_time_label, LV_ALIGN_CENTER, 0, -10);

  /* 控制按钮区域：上一曲 / 播放 / 下一曲
   * 将按钮整体上移一些，避免与底部播放列表按钮贴在一起
   * 按钮布局：三个按钮水平居中排列，按钮之间有适当间距
   */
  lv_coord_t btn_y = ver_res > 0 ? (ver_res - 160) : 300;
  lv_coord_t center_x = hor_res / 2;

  /* 按钮尺寸和间距定义 */
  const lv_coord_t btn_width = 80;      ///< 按钮宽度
  const lv_coord_t btn_height = 60;     ///< 按钮高度
  const lv_coord_t btn_spacing = 15;    ///< 按钮之间的间距
  
  /* 计算按钮组总宽度：3个按钮 + 2个间距 */
  lv_coord_t total_width = btn_width * 3 + btn_spacing * 2;
  lv_coord_t start_x = center_x - total_width / 2;  ///< 第一个按钮的左边缘位置

  /* 上一曲按钮 */
  lv_obj_t *btn_prev = lv_btn_create(lv_scr_act());
  lv_obj_set_size(btn_prev, btn_width, btn_height);
  lv_obj_set_pos(btn_prev, start_x, btn_y);
  lv_obj_add_event_cb(btn_prev, btn_player_prev_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *label_prev = lv_label_create(btn_prev);
  lv_label_set_text(label_prev, LV_SYMBOL_PREV);
  lv_obj_set_style_text_font(label_prev, &lv_font_montserrat_24, 0);
  lv_obj_center(label_prev);

  /* 播放/暂停按钮（居中） */
  lv_obj_t *btn_play = lv_btn_create(lv_scr_act());
  lv_obj_set_size(btn_play, btn_width, btn_height);
  lv_obj_set_pos(btn_play, start_x + btn_width + btn_spacing, btn_y);
  lv_obj_add_event_cb(btn_play, btn_player_play_cb, LV_EVENT_CLICKED, NULL);

  player_play_label = lv_label_create(btn_play);
  /* 根据播放状态显示不同图标（移植自v1.0，从player结构体读取） */
  lv_label_set_text(player_play_label, player.is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
  player_is_playing = player.is_playing;  /* 同步到UI标志 */
  lv_obj_set_style_text_font(player_play_label, &lv_font_montserrat_24, 0);
  lv_obj_center(player_play_label);

  /* 下一曲按钮 */
  lv_obj_t *btn_next = lv_btn_create(lv_scr_act());
  lv_obj_set_size(btn_next, btn_width, btn_height);
  lv_obj_set_pos(btn_next, start_x + (btn_width + btn_spacing) * 2, btn_y);
  lv_obj_add_event_cb(btn_next, btn_player_next_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *label_next = lv_label_create(btn_next);
  lv_label_set_text(label_next, LV_SYMBOL_NEXT);
  lv_obj_set_style_text_font(label_next, &lv_font_montserrat_24, 0);
  lv_obj_center(label_next);

  /* 播放列表按钮 */
  lv_obj_t *btn_list = lv_btn_create(lv_scr_act());
  lv_obj_set_size(btn_list, 120, 40);
  /* 稍微向上调整一点，让按钮组与底部保持适当间距 */
  lv_obj_align(btn_list, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_add_event_cb(btn_list, btn_player_playlist_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *label_list = lv_label_create(btn_list);
  lv_label_set_text(label_list, LV_SYMBOL_LIST " Playlist");
  lv_obj_set_style_text_font(label_list, &lv_font_montserrat_16, 0);
  lv_obj_center(label_list);
}

/**
  * @brief  创建播放列表页面（内部实现）
  * @retval None
  * @note   显示SD卡中扫描到的所有音频文件列表，支持点击选择歌曲播放
  *          当前播放的歌曲会高亮显示
 */
static void create_playlist_page_internal(void)
{
  LOGI("[PLAYLIST] Creating playlist page...\r\n");
  
  /* 清空当前屏幕并设置背景（与其他页面保持一致） */
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), 0);
  
  LOGI("[PLAYLIST] Screen cleared\r\n");
  
  /* 重置播放器UI指针，防止野指针访问（关键修复） */
  player_play_label = NULL;
  player_song_label = NULL;
  player_time_label = NULL;
  player_progress_slider = NULL;
  slider_dragging = 0;  /* 清除拖动标志 */
  
  LOGI("[PLAYLIST] Pointers reset\r\n");

  lv_coord_t hor_res = lv_disp_get_hor_res(NULL);
  lv_coord_t ver_res = lv_disp_get_ver_res(NULL);

  LOGI("[PLAYLIST] Resolution: %d x %d\r\n", hor_res, ver_res);

  /* 检查并初始化播放列表（如果未初始化） */
  uint8_t song_count = Playlist_GetCount();
  LOGI("[PLAYLIST] Current song count: %d\r\n", song_count);
  if(song_count == 0) {
    LOGI("[PLAYLIST] Playlist empty, initializing...\r\n");
    uint8_t init_result = Playlist_Init();
    if(init_result == 0) {
      song_count = Playlist_GetCount();
      LOGI("[PLAYLIST] Playlist initialized, found %d songs\r\n", song_count);
    } else {
      LOGI("[PLAYLIST] Playlist_Init() failed: %d\r\n", init_result);
    }
  }

  LOGI("[PLAYLIST] Creating header...\r\n");
  /* 顶部标题栏 */
  lv_obj_t *header = lv_obj_create(lv_scr_act());
  if(header == NULL) {
    LOGI("[PLAYLIST] ERROR: Failed to create header\r\n");
    return;
  }
  lv_obj_set_size(header, hor_res, 50);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, lv_palette_main(LV_PALETTE_BLUE), 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  LOGI("[PLAYLIST] Header created\r\n");

  /* 返回按钮（返回到播放器界面） */
  lv_obj_t *btn_back = lv_btn_create(header);
  if(btn_back == NULL) {
    LOGI("[PLAYLIST] ERROR: Failed to create back button\r\n");
    return;
  }
  lv_obj_set_size(btn_back, 60, 40);
  lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 5, 0);
  lv_obj_add_event_cb(btn_back, btn_playlist_back_cb, LV_EVENT_CLICKED, NULL);
  LOGI("[PLAYLIST] Back button created\r\n");

  lv_obj_t *back_label = lv_label_create(btn_back);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT);
  lv_obj_center(back_label);

  /* 标题文字 */
  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, "Playlist");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_center(title);
  LOGI("[PLAYLIST] Title created\r\n");

  /* 歌曲数量显示 */
  lv_obj_t *count_label = lv_label_create(lv_scr_act());
  lv_label_set_text_fmt(count_label, "Total: %d songs", song_count);
  lv_obj_set_style_text_font(count_label, &lv_font_montserrat_14, 0);
  lv_obj_align(count_label, LV_ALIGN_TOP_MID, 0, 60);

  /* 创建列表容器 */
  LOGI("[PLAYLIST] Creating list container...\r\n");
  lv_obj_t *list = lv_list_create(lv_scr_act());
  lv_obj_set_size(list, hor_res - 20, ver_res - 150);
  lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 90);
  lv_obj_set_style_border_width(list, 1, 0);
  lv_obj_set_style_border_color(list, lv_palette_main(LV_PALETTE_GREY), 0);

  /* 添加歌曲列表项 */
  LOGI("[PLAYLIST] Adding %d songs to list...\r\n", song_count);
  if(song_count > 0)
  {
    for(uint8_t i = 0; i < song_count; i++)
    {
      const char *filename = Playlist_GetFileName(i);
      if(filename != NULL)
      {
        LOGI("[PLAYLIST] Adding song %d: %s\r\n", i, filename);
        lv_obj_t *list_btn = lv_list_add_btn(list, LV_SYMBOL_AUDIO, filename);
        lv_obj_set_style_text_font(list_btn, &lv_font_montserrat_14, 0);
        
        /* 如果是当前播放的歌曲，高亮显示 */
        if(i == Playlist_GetCurrentIndex())
        {
          lv_obj_set_style_bg_color(list_btn, lv_palette_main(LV_PALETTE_BLUE), 0);
          lv_obj_set_style_text_color(list_btn, lv_color_white(), 0);
        }
        
        /* 添加点击事件：选择歌曲 */
        lv_obj_set_user_data(list_btn, (void*)(uintptr_t)i);
        lv_obj_add_event_cb(list_btn, btn_player_playlist_item_cb, LV_EVENT_CLICKED, NULL);
      }
      else
      {
        LOGI("[PLAYLIST] Warning: filename is NULL for index %d\r\n", i);
    }
    }
    LOGI("[PLAYLIST] All songs added to list\r\n");
  }
  else
  {
    LOGI("[PLAYLIST] No songs found, showing empty message\r\n");
    /* 没有歌曲时显示提示 */
    lv_obj_t *empty_label = lv_label_create(list);
    lv_label_set_text(empty_label, "No songs found in SD card");
    lv_obj_set_style_text_font(empty_label, &lv_font_montserrat_16, 0);
    lv_obj_center(empty_label);
  }
  
  LOGI("[PLAYLIST] Playlist page UI creation completed\r\n");
  /* 不在这里调用 lv_refr_now()，由外部调用者统一刷新（与播放器页面保持一致） */
}

/**
  * @brief  播放列表项点击回调
  * @param  e: LVGL事件对象
  * @retval None
  * @note   当用户点击播放列表中的某一首歌曲时，加载该歌曲并切换到播放器页面
 */
static void btn_player_playlist_item_cb(lv_event_t *e)
{
  lv_obj_t *btn = lv_event_get_target(e);
  uint16_t index = (uint16_t)(uintptr_t)lv_obj_get_user_data(btn);
  
  char msg[100];
  sprintf(msg, "!!! PLAYLIST ITEM %d CLICKED !!!\r\n", index);
  /*HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), 1000);*/
  
  /* 设置当前播放索引 */
  Playlist_SetIndex(index);
  
  /* 获取歌曲文件路径 */
  const char *filepath = Playlist_GetCurrentFile();
  if(filepath != NULL) {
    /* 加载歌曲（不自动播放） */
    Audio_Load(filepath, 0);
  }
  
  /* 返回播放器页面 */
  current_page = PAGE_AUDIO_PLAYER;
  create_player_page_stub();
  
  /* 更新UI显示 */
  if(player_song_label != NULL) {
    const char *filename = Playlist_GetFileName(index);
    if(filename != NULL) {
      lv_label_set_text(player_song_label, filename);
    }
  }
  
  /* 更新时间显示 */
  if(player_time_label != NULL) {
    uint32_t total_time = Audio_GetTotalTime();
    uint32_t min = total_time / 60;
    uint32_t sec = total_time % 60;
    lv_label_set_text_fmt(player_time_label, "%02lu:%02lu / %02lu:%02lu", 0, 0, min, sec);
  }
  
  /* 更新进度条 */
  if(player_progress_slider != NULL) {
    lv_slider_set_value(player_progress_slider, 0, LV_ANIM_OFF);
  }
  
  log_button("Playlist Item Selected");
}

/**
  * @brief  播放器进度条拖动回调
  * @param  e: LVGL事件对象
  * @retval None
  * @note   拖动过程中实时显示时间，拖动结束时跳转到对应位置
  */
static void player_progress_slider_cb(lv_event_t *e)
{
  lv_obj_t *slider = lv_event_get_target(e);
  lv_event_code_t code = lv_event_get_code(e);
  
  if(slider != player_progress_slider) {
    return;
  }
  
  /* 如果程序正在更新进度条，不处理 */
  if(slider_updating) {
    return;
  }
  
  /* 用户开始拖动 */
  if(code == LV_EVENT_PRESSED)
  {
    slider_dragging = 1;  /* 设置拖动标志，阻止Player_Update更新进度条 */
  }
  /* 用户释放时跳转 */
  else if(code == LV_EVENT_RELEASED)
  {
    slider_dragging = 0;  /* 清除拖动标志 */
    
    /* 获取进度条值（0-100） */
    int32_t progress = lv_slider_get_value(slider);
    
    /* 只有在播放或暂停状态才能跳转 */
    if(audio_player.state == AUDIO_STATE_PLAY || audio_player.state == AUDIO_STATE_PAUSE)
    {
      /* 计算目标时间 */
      if(player.total_time > 0)
      {
        uint32_t target_time = (progress * player.total_time) / 100;
        
        /* 跳转到目标位置 */
        if(Audio_Seek(target_time) == 0)
        {
          /* 更新显示 */
          player.current_time = target_time;
          player.progress = progress;
        }
      }
    }
  }
}

/**
  * @brief  创建播放列表页面（外部接口）
  * @retval None
  * @note   外部调用接口，内部调用create_playlist_page_internal实现
 */
void create_playlist_page(void)
{
  create_playlist_page_internal();
}

/**
  * @brief  更新播放器页面UI（播放进度、时间显示等）
  * @retval None
  * @note   在主循环中定期调用，更新播放进度条、当前时间显示
  *          只在播放器页面时更新，避免不必要的计算
  */
void music_assistant_update_player_ui(void)
{
  /* 只在播放器页面时更新 */
  if(current_page != PAGE_AUDIO_PLAYER) {
    return;
  }
  
  /* 检查SD卡初始化状态，如果刚完成初始化，自动刷新播放器页面 */
  static SD_InitState_t last_sd_state = SD_INIT_IDLE;
  static uint8_t user_has_entered_player = 0;  /* 标记用户是否已经进入播放器页面 */
  SD_InitState_t current_sd_state = SD_Init_GetState();
  
  /* 如果用户已经进入播放器页面，不再自动刷新（避免重复加载） */
  if(user_has_entered_player) {
    last_sd_state = current_sd_state;
    /* 继续执行UI更新逻辑 */
  } else if(last_sd_state != SD_INIT_SUCCESS && current_sd_state == SD_INIT_SUCCESS) {
    /* SD卡初始化刚完成，且用户还没有进入播放器页面，刷新播放器页面 */
    /* 注意：这里只在用户已经在播放器页面时才刷新，否则不处理 */
    if(current_page == PAGE_AUDIO_PLAYER) {
      LOGI("[PLAYER] SD card initialization completed, refreshing player page\r\n");
      refresh_player_page_if_needed();
      user_has_entered_player = 1;  /* 标记已刷新，避免重复 */
    }
  }
  last_sd_state = current_sd_state;
  
  /* 检查UI控件是否有效 */
  if(player_time_label == NULL || player_progress_slider == NULL) {
    return;
  }
  
  /* 获取播放状态和时间信息 */
  uint8_t state = Audio_GetState();
  uint32_t total_time = Audio_GetTotalTime();
  uint32_t current_time = Audio_GetCurrentTime();
  
  /* 如果歌曲未加载，不更新 */
  if(total_time == 0) {
    return;
  }
  
  /* 计算时间显示 */
  uint32_t total_min = total_time / 60;
  uint32_t total_sec = total_time % 60;
  uint32_t current_min = current_time / 60;
  uint32_t current_sec = current_time % 60;
  
  /* 检查进度条是否正在被拖动，如果正在拖动则不自动更新进度条值 */
  if(lv_slider_is_dragged(player_progress_slider)) {
    /* 用户正在拖动进度条，只更新时间显示（由拖动回调函数更新），不更新进度条值，避免覆盖用户操作 */
    /* 注意：时间显示会在拖动回调函数中更新，这里不需要重复更新 */
    return;
  }
  
  /* 更新播放进度条（0-100%） */
  int32_t progress = 0;
  if(total_time > 0 && current_time > 0) {
    progress = (int32_t)((current_time * 100UL) / total_time);
    if(progress > 100) progress = 100;
    if(progress < 0) progress = 0;
  }
  
  /* 更新UI显示（使用静态变量控制更新频率，避免过于频繁） */
  static uint32_t last_update_time = 0;
  static uint32_t last_progress = 0;
  uint32_t now = HAL_GetTick();
  
  /* 每100ms更新一次UI（10Hz更新频率，平衡流畅性和CPU占用） */
  if(now - last_update_time >= 100) {
    last_update_time = now;
    
    /* 更新时间显示 */
    lv_label_set_text_fmt(player_time_label, "%02lu:%02lu / %02lu:%02lu", 
                          current_min, current_sec, total_min, total_sec);
    
    /* 更新进度条（只要有总时长就更新，不管播放状态） */
    if(total_time > 0) {
      /* 在播放状态下，总是更新进度条（确保同步） */
      /* 在暂停状态下，只有值变化时才更新 */
      if(state == AUDIO_STATE_PLAY || progress != last_progress) {
        lv_slider_set_value(player_progress_slider, (int32_t)progress, LV_ANIM_OFF);
        /* 强制刷新进度条显示 */
        lv_obj_invalidate(player_progress_slider);
        last_progress = progress;
      }
    }
    
    /* 同步播放按钮图标状态（防止状态不同步） */
    if(player_play_label != NULL) {
      uint8_t is_playing = (state == AUDIO_STATE_PLAY);
      if(player_is_playing != is_playing) {
        player_is_playing = is_playing;
        lv_label_set_text(player_play_label, is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
      }
    }
  }
}

/**
  * @brief  更新播放器状态（移植自v1.0）
  * @retval None
  * @note   在主循环中定期调用（每50ms），内部节流为500ms更新一次
  *          只在播放器页面且正在播放时更新UI
  *          检测播放完成，自动切换下一首
  */
void Player_Update(void)
{
  static uint32_t last_update = 0;
  uint32_t now = HAL_GetTick();

  /* 节流控制：每100ms更新一次（10Hz更新频率，更流畅的UI更新） */
  if(now - last_update >= 100)
  {
    last_update = now;

    /* 只在播放器页面且正在播放时更新 */
    if(current_page == PAGE_AUDIO_PLAYER && player.is_playing)
    {
      /* 从底层读取实际播放时间 */
      player.current_time = Audio_GetCurrentTime();
      /* 不要覆盖总时长，保持初始值 */

      /* 调试输出：检查时间更新 */
      static uint8_t debug_cnt = 0;
      if(++debug_cnt >= 4) {  /* 每2秒输出一次（500ms * 4） */
        debug_cnt = 0;
        LOGI("[Player_Update] current=%lu, total=%lu, state=%d, page=%d\r\n", 
             player.current_time, player.total_time, Audio_GetState(), current_page);
      }

      if(player.total_time > 0)
      {
        /* 计算进度百分比 */
        player.progress = (player.current_time * 100) / player.total_time;
        
        /* 只有在用户没有拖动时才更新进度条 */
        if(player_progress_slider != NULL && !slider_dragging)
        {
          slider_updating = 1;  /* 设置保护标志，避免触发回调 */
          lv_slider_set_value(player_progress_slider, player.progress, LV_ANIM_OFF);
          slider_updating = 0;  /* 清除保护标志 */
        }
        
        /* 更新时间显示（拖动时也更新，显示实际播放时间） */
        if(player_time_label != NULL)
        {
          lv_label_set_text_fmt(player_time_label, "%02lu:%02lu / %02lu:%02lu",
                                player.current_time/60, player.current_time%60,
                                player.total_time/60, player.total_time%60);
        }
      }
    }
    else
    {
      /* 调试输出：为什么没更新 */
      static uint8_t debug_cnt2 = 0;
      if(++debug_cnt2 >= 10) {  /* 每5秒输出一次 */
        debug_cnt2 = 0;
        // LOGI("[Player_Update] Skip: page=%d, is_playing=%d, state=%d\r\n", 
        //      current_page, player.is_playing, Audio_GetState());
      }
    }

    /* 检测播放完成（从播放状态变为停止状态） */
    if(player.is_playing && Audio_GetState() == AUDIO_STATE_STOP)
    {
      /* 保存之前是播放状态（自动切换时应该继续播放） */
      uint8_t was_playing = 1;
      
      player.is_playing = 0;
      
      /* 切换到下一首 */
      if(Playlist_Next() == 0)
      {
        const char* file = Playlist_GetCurrentFile();
        if(file)
        {
          /* 停止当前播放（确保资源释放） */
          Audio_Stop();
          HAL_Delay(50);  /* 等待资源释放 */
          
          /* 根据之前的状态决定是否自动播放（自动切换时继续播放） */
          Audio_Load(file, was_playing ? 1 : 0);
          
          /* 等待加载完成 */
          HAL_Delay(100);
          
          /* 更新UI状态 */
          player.current_time = 0;
          player.progress = 0;
          
          const char* filename = Playlist_GetFileName(Playlist_GetCurrentIndex());
          if(filename)
          {
            strncpy(player.song_name, filename, sizeof(player.song_name)-1);
            player.song_name[sizeof(player.song_name)-1] = '\0';
          }
          
          /* 获取总时长 */
          player.total_time = Audio_GetTotalTime();
          player.is_playing = (Audio_GetState() == AUDIO_STATE_PLAY);
          
          /* 如果还在播放器页面，刷新UI */
          if(current_page == PAGE_AUDIO_PLAYER)
          {
            create_player_page_stub();
          }
        }
      }
      else
      {
        /* 没有下一首了，保持停止状态 */
        player.is_playing = 0;
      }
    }
  }
}

/**
  * @brief  检查SD卡初始化状态，必要时刷新播放器页面
  * @retval None
  * @note   当SD卡初始化完成时，自动加载第一首歌曲并更新UI
  */
static void refresh_player_page_if_needed(void)
{
  /* 只在播放器页面时处理 */
  if(current_page != PAGE_AUDIO_PLAYER) {
    return;
  }
  
  /* 检查SD卡是否已就绪 */
  if(!SD_Init_IsReady()) {
    return;  /* SD卡未就绪，不刷新 */
  }
  
  /* 如果UI控件还未创建，不刷新（避免在页面创建过程中刷新） */
  if(player_song_label == NULL || player_time_label == NULL) {
    LOGI("[PLAYER] UI not ready, skip refresh\r\n");
    return;
  }
  
  LOGI("[PLAYER] Refreshing player page after SD init\r\n");
  
  /* 检查播放列表是否已初始化 */
  uint8_t playlist_count = Playlist_GetCount();
  LOGI("[PLAYER] Playlist count: %d\r\n", playlist_count);
  
  if(playlist_count == 0) {
    /* 尝试初始化播放列表 */
    LOGI("[PLAYER] Initializing playlist...\r\n");
    uint8_t init_result = Playlist_Init();
    if(init_result != 0) {
      LOGI("[PLAYER] Playlist_Init() failed: %d\r\n", init_result);
      /* 更新UI显示错误信息（不重新创建页面，只更新标签） */
      if(player_song_label != NULL) {
        lv_label_set_text(player_song_label, "No songs found");
      }
      return;
    }
    playlist_count = Playlist_GetCount();
    LOGI("[PLAYER] Playlist initialized, found %d songs\r\n", playlist_count);
  }
  
  /* 如果有歌曲，且当前没有加载歌曲，才加载第一首（不重新创建页面，只更新数据和UI） */
  if(playlist_count > 0 && Audio_GetTotalTime() == 0) {
    LOGI("[PLAYER] Loading first song after refresh...\r\n");
    /* 确保当前索引为0（第一首） */
    Playlist_SetIndex(0);
    
    /* 获取第一首歌曲的文件路径 */
    const char *filepath = Playlist_GetCurrentFile();
    if(filepath != NULL) {
      LOGI("[PLAYER] Loading file: %s\r\n", filepath);
      /* 先停止并释放之前的资源（如果有） */
      Audio_Stop();
      HAL_Delay(50);  /* 等待资源释放完成 */
      /* 加载歌曲（不自动播放） */
      Audio_Load(filepath, 0);
      LOGI("[PLAYER] Audio_Load() called\r\n");
      
      /* 更新UI显示（移植自v1.0，同时更新player结构体和UI标签） */
      const char *filename = Playlist_GetFileName(0);
      if(filename != NULL) {
        /* 更新player结构体（移植自v1.0） */
        strncpy(player.song_name, filename, sizeof(player.song_name)-1);
        player.song_name[sizeof(player.song_name)-1] = '\0';
        
        /* 更新UI标签 */
        if(player_song_label != NULL) {
          lv_label_set_text(player_song_label, filename);
        }
        LOGI("[PLAYER] Song name updated: %s\r\n", filename);
      }
      
      /* 更新时间显示 */
      if(player_time_label != NULL) {
        uint32_t total_time = Audio_GetTotalTime();
        uint32_t min = total_time / 60;
        uint32_t sec = total_time % 60;
        lv_label_set_text_fmt(player_time_label, "%02lu:%02lu / %02lu:%02lu", 0, 0, min, sec);
        LOGI("[PLAYER] Total time: %lu seconds\r\n", total_time);
      }
      
      /* 更新进度条 */
      if(player_progress_slider != NULL) {
        lv_slider_set_value(player_progress_slider, 0, LV_ANIM_OFF);
      }
    } else {
      LOGI("[PLAYER] Playlist_GetCurrentFile() returned NULL\r\n");
    }
  } else if(playlist_count == 0) {
    LOGI("[PLAYER] No songs in playlist after refresh\r\n");
    /* 更新UI显示 */
    if(player_song_label != NULL) {
      lv_label_set_text(player_song_label, "No songs found");
    }
  } else {
    /* 已经有歌曲加载了，不需要再次加载 */
    LOGI("[PLAYER] Song already loaded, skip refresh\r\n");
  }
}

/* ==================== 节拍器页面实现 ==================== */

/**
 * @brief  创建节拍器页面
 */
static void create_metronome_page(void)
{
  // 先清理屏幕
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), 0);
  
  // 清理之前的对象指针
  metronome_bpm_label = NULL;
  metronome_bpm_slider = NULL;
  metronome_bpm_btn_minus = NULL;
  metronome_bpm_btn_plus = NULL;
  metronome_beat_count_label = NULL;
  metronome_beat_count_btn_minus = NULL;
  metronome_beat_count_btn_plus = NULL;
  metronome_play_btn = NULL;
  metronome_play_label = NULL;
  
  // 清理所有节拍点指针
  for(int i = 0; i < METRONOME_MAX_BEAT_COUNT; i++) {
    metronome_beat_rects[i] = NULL;
    for(int j = 0; j < 3; j++) {
      metronome_beat_segments[i][j] = NULL;
    }
  }
  
  // 同步节拍数和BPM到硬件层
  extern void Metronome_SetBeatCount(uint8_t beat_count);
  extern void Metronome_SetBPM(uint16_t bpm);
  Metronome_SetBeatCount(metronome.beat_count);
  Metronome_SetBPM(metronome.bpm);
  
  /* 标题栏 */
  lv_obj_t *header = lv_obj_create(lv_scr_act());
  lv_obj_set_size(header, 320, 50);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, lv_palette_main(LV_PALETTE_BLUE), 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  
  /* 返回按钮 */
  lv_obj_t *btn_back = lv_btn_create(header);
  lv_obj_set_size(btn_back, 60, 40);
  lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 5, 0);
  lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *back_label = lv_label_create(btn_back);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT);
  lv_obj_center(back_label);
  
  /* 标题 */
  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, "Metronome");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_center(title);
  
  /* BPM显示 */
  metronome_bpm_label = lv_label_create(lv_scr_act());
  lv_label_set_text_fmt(metronome_bpm_label, "%d BPM", metronome.bpm);
  lv_obj_set_style_text_font(metronome_bpm_label, &lv_font_montserrat_48, 0);
  lv_obj_align(metronome_bpm_label, LV_ALIGN_TOP_MID, 0, 70);
  
  /* 动态创建节拍点 (竖着的矩形，分成三段) - 支持2-16拍 */
  int beat_width = 50;
  int beat_height = 120;
  int border_width = 2;
  int inner_width = beat_width - border_width * 2;
  int inner_height = beat_height - border_width * 2;
  int segment_height = inner_height / 3;
  int beat_spacing = 30;
  int total_width = beat_width * metronome.beat_count + beat_spacing * (metronome.beat_count - 1);
  int start_x = (320 - total_width) / 2;
  int start_y = 150;
  
  // 如果节拍数太多，缩小宽度和间距
  if(total_width > 300) {
    beat_width = 40;
    beat_spacing = 15;
    inner_width = beat_width - border_width * 2;
    total_width = beat_width * metronome.beat_count + beat_spacing * (metronome.beat_count - 1);
    start_x = (320 - total_width) / 2;
  }
  if(total_width > 300) {
    beat_width = 35;
    beat_spacing = 10;
    inner_width = beat_width - border_width * 2;
    total_width = beat_width * metronome.beat_count + beat_spacing * (metronome.beat_count - 1);
    start_x = (320 - total_width) / 2;
  }
  if(total_width > 300) {
    beat_width = 30;
    beat_spacing = 8;
    inner_width = beat_width - border_width * 2;
    total_width = beat_width * metronome.beat_count + beat_spacing * (metronome.beat_count - 1);
    start_x = (320 - total_width) / 2;
  }
  
  for(int i = 0; i < metronome.beat_count; i++) {
    int x_pos = start_x + i * (beat_width + beat_spacing);
    
    /* 创建节拍点容器 */
    metronome_beat_rects[i] = lv_obj_create(lv_scr_act());
    lv_obj_set_size(metronome_beat_rects[i], beat_width, beat_height);
    lv_obj_set_pos(metronome_beat_rects[i], x_pos, start_y);
    lv_obj_set_style_bg_color(metronome_beat_rects[i], lv_color_white(), 0);
    lv_obj_set_style_border_color(metronome_beat_rects[i], lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_border_width(metronome_beat_rects[i], border_width, 0);
    lv_obj_set_style_radius(metronome_beat_rects[i], 5, 0);
    lv_obj_set_style_pad_all(metronome_beat_rects[i], 0, 0);
    lv_obj_clear_flag(metronome_beat_rects[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(metronome_beat_rects[i], metronome_beat_strength_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
    
    /* 创建三段矩形表示强度等级 */
    for(int j = 0; j < 3; j++) {
      metronome_beat_segments[i][j] = lv_obj_create(metronome_beat_rects[i]);
      int seg_y = j * segment_height;
      int seg_h = (j == 2) ? (inner_height - seg_y) : segment_height;
      
      lv_obj_set_size(metronome_beat_segments[i][j], inner_width, seg_h);
      lv_obj_set_pos(metronome_beat_segments[i][j], 0, seg_y);
      lv_obj_set_style_bg_color(metronome_beat_segments[i][j], lv_color_white(), 0);
      lv_obj_set_style_border_width(metronome_beat_segments[i][j], 0, 0);
      lv_obj_set_style_radius(metronome_beat_segments[i][j], 0, 0);
      lv_obj_set_style_pad_all(metronome_beat_segments[i][j], 0, 0);
      lv_obj_clear_flag(metronome_beat_segments[i][j], LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_event_cb(metronome_beat_segments[i][j], metronome_beat_strength_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
    }
    
    /* 添加横线分隔三段 */
    for(int k = 1; k < 3; k++) {
      lv_obj_t *divider = lv_obj_create(metronome_beat_rects[i]);
      int line_y = k * segment_height;
      lv_obj_set_size(divider, inner_width, 1);
      lv_obj_set_pos(divider, 0, line_y);
      lv_obj_set_style_bg_color(divider, lv_palette_main(LV_PALETTE_GREY), 0);
      lv_obj_set_style_border_width(divider, 0, 0);
      lv_obj_set_style_radius(divider, 0, 0);
      lv_obj_set_style_pad_all(divider, 0, 0);
      lv_obj_clear_flag(divider, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_clear_flag(divider, LV_OBJ_FLAG_CLICKABLE);
    }
    
    /* 根据当前强度等级设置显示 */
    uint8_t strength = metronome.beat_strength[i];
    lv_color_t yellow = lv_palette_main(LV_PALETTE_YELLOW);
    lv_color_t white = lv_color_white();
    
    for(int j = 0; j < 3; j++) {
      int segment_from_bottom = 2 - j;
      
      if(strength == 0) {
        lv_obj_set_style_bg_color(metronome_beat_segments[i][j], white, 0);
      } else if(segment_from_bottom < strength) {
        lv_obj_set_style_bg_color(metronome_beat_segments[i][j], yellow, 0);
      } else {
        lv_obj_set_style_bg_color(metronome_beat_segments[i][j], white, 0);
      }
    }
  }
  
  /* BPM滑动条和微调按钮 */
  int slider_y = start_y + beat_height + 20;
  metronome_bpm_slider = lv_slider_create(lv_scr_act());
  lv_obj_set_size(metronome_bpm_slider, 200, 20);
  lv_obj_align(metronome_bpm_slider, LV_ALIGN_TOP_MID, 0, slider_y);
  lv_slider_set_range(metronome_bpm_slider, 10, 500);
  if(metronome.bpm < 10) {
    metronome.bpm = 120;
  }
  lv_slider_set_value(metronome_bpm_slider, metronome.bpm, LV_ANIM_OFF);
  lv_obj_add_event_cb(metronome_bpm_slider, metronome_bpm_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
  
  /* BPM -1 按钮 */
  metronome_bpm_btn_minus = lv_btn_create(lv_scr_act());
  lv_obj_set_size(metronome_bpm_btn_minus, 35, 35);
  lv_obj_align(metronome_bpm_btn_minus, LV_ALIGN_TOP_MID, -130, slider_y - 7);
  lv_obj_set_style_bg_color(metronome_bpm_btn_minus, lv_palette_main(LV_PALETTE_BLUE), 0);
  lv_obj_clear_flag(metronome_bpm_btn_minus, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(metronome_bpm_btn_minus, metronome_bpm_minus_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *minus_label = lv_label_create(metronome_bpm_btn_minus);
  lv_label_set_text(minus_label, "-");
  lv_obj_set_style_text_font(minus_label, &lv_font_montserrat_24, 0);
  lv_obj_center(minus_label);
  
  /* BPM +1 按钮 */
  metronome_bpm_btn_plus = lv_btn_create(lv_scr_act());
  lv_obj_set_size(metronome_bpm_btn_plus, 35, 35);
  lv_obj_align(metronome_bpm_btn_plus, LV_ALIGN_TOP_MID, 130, slider_y - 7);
  lv_obj_set_style_bg_color(metronome_bpm_btn_plus, lv_palette_main(LV_PALETTE_BLUE), 0);
  lv_obj_clear_flag(metronome_bpm_btn_plus, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(metronome_bpm_btn_plus, metronome_bpm_plus_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *plus_label = lv_label_create(metronome_bpm_btn_plus);
  lv_label_set_text(plus_label, "+");
  lv_obj_set_style_text_font(plus_label, &lv_font_montserrat_24, 0);
  lv_obj_center(plus_label);
  
  /* 节拍数显示和选择 */
  int beat_count_y = slider_y + 40;
  metronome_beat_count_label = lv_label_create(lv_scr_act());
  lv_label_set_text_fmt(metronome_beat_count_label, "%d Beats", metronome.beat_count);
  lv_obj_set_style_text_font(metronome_beat_count_label, &lv_font_montserrat_16, 0);
  lv_obj_align(metronome_beat_count_label, LV_ALIGN_TOP_MID, 0, beat_count_y);
  
  /* 节拍数 -1 按钮 */
  metronome_beat_count_btn_minus = lv_btn_create(lv_scr_act());
  lv_obj_set_size(metronome_beat_count_btn_minus, 30, 30);
  lv_obj_align(metronome_beat_count_btn_minus, LV_ALIGN_TOP_MID, -60, beat_count_y - 2);
  lv_obj_set_style_bg_color(metronome_beat_count_btn_minus, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_clear_flag(metronome_beat_count_btn_minus, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(metronome_beat_count_btn_minus, metronome_beat_count_minus_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *beat_minus_label = lv_label_create(metronome_beat_count_btn_minus);
  lv_label_set_text(beat_minus_label, "-");
  lv_obj_set_style_text_font(beat_minus_label, &lv_font_montserrat_20, 0);
  lv_obj_center(beat_minus_label);
  
  /* 节拍数 +1 按钮 */
  metronome_beat_count_btn_plus = lv_btn_create(lv_scr_act());
  lv_obj_set_size(metronome_beat_count_btn_plus, 30, 30);
  lv_obj_align(metronome_beat_count_btn_plus, LV_ALIGN_TOP_MID, 60, beat_count_y - 2);
  lv_obj_set_style_bg_color(metronome_beat_count_btn_plus, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_clear_flag(metronome_beat_count_btn_plus, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(metronome_beat_count_btn_plus, metronome_beat_count_plus_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *beat_plus_label = lv_label_create(metronome_beat_count_btn_plus);
  lv_label_set_text(beat_plus_label, "+");
  lv_obj_set_style_text_font(beat_plus_label, &lv_font_montserrat_20, 0);
  lv_obj_center(beat_plus_label);
  
  /* 播放/暂停按钮 */
  metronome_play_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(metronome_play_btn, 100, 60);
  lv_obj_align(metronome_play_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
  lv_obj_set_style_bg_color(metronome_play_btn, lv_palette_main(LV_PALETTE_GREEN), 0);
  lv_obj_clear_flag(metronome_play_btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(metronome_play_btn, metronome_play_pause_cb, LV_EVENT_CLICKED, NULL);
  metronome_play_label = lv_label_create(metronome_play_btn);
  lv_label_set_text(metronome_play_label, metronome.is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
  lv_obj_set_style_text_font(metronome_play_label, &lv_font_montserrat_32, 0);
  lv_obj_center(metronome_play_label);
}

/**
 * @brief  更新节拍器页面显示
 */
void update_metronome_display(void)
{
  if(current_page != PAGE_METRONOME) {
    return;  // 不在节拍器页面，不更新
  }
  
  // 更新BPM显示
  if(metronome_bpm_label != NULL) {
    lv_label_set_text_fmt(metronome_bpm_label, "%d BPM", metronome.bpm);
  }
  
  // 更新滑动条
  if(metronome_bpm_slider != NULL) {
    lv_slider_set_value(metronome_bpm_slider, metronome.bpm, LV_ANIM_OFF);
  }
  
  // 更新节拍数显示
  if(metronome_beat_count_label != NULL) {
    lv_label_set_text_fmt(metronome_beat_count_label, "%d Beats", metronome.beat_count);
  }
  
  // 更新播放/暂停按钮
  if(metronome_play_label != NULL) {
    extern uint8_t Metronome_IsPlaying(void);
    metronome.is_playing = Metronome_IsPlaying();
    lv_label_set_text(metronome_play_label, metronome.is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
  }
  
  // 更新当前节拍的高亮显示
  extern uint8_t Metronome_GetCurrentBeat(void);
  metronome.current_beat = Metronome_GetCurrentBeat();
  
  for(int i = 0; i < metronome.beat_count && i < METRONOME_MAX_BEAT_COUNT; i++) {
    if(metronome_beat_rects[i] != NULL) {
      if(i == metronome.current_beat) {
        // 当前节拍：高亮边框（蓝色，宽度3）
        lv_obj_set_style_border_color(metronome_beat_rects[i], lv_palette_main(LV_PALETTE_BLUE), 0);
        lv_obj_set_style_border_width(metronome_beat_rects[i], 3, 0);
      } else {
        // 非当前节拍：普通边框（灰色，宽度2）
        lv_obj_set_style_border_color(metronome_beat_rects[i], lv_palette_main(LV_PALETTE_GREY), 0);
        lv_obj_set_style_border_width(metronome_beat_rects[i], 2, 0);
      }
    }
  }
}

/**
 * @brief  BPM滑动条回调
 */
static void metronome_bpm_slider_cb(lv_event_t *e)
{
  lv_obj_t *slider = lv_event_get_target(e);
  int32_t value = lv_slider_get_value(slider);
  
  if(value < 10) value = 10;
  if(value > 500) value = 500;
  metronome.bpm = (uint16_t)value;
  
  extern void Metronome_SetBPM(uint16_t bpm);
  Metronome_SetBPM(metronome.bpm);
  
  if(metronome_bpm_label != NULL) {
    lv_label_set_text_fmt(metronome_bpm_label, "%d BPM", metronome.bpm);
  }
}

/**
 * @brief  BPM -1 按钮回调
 */
static void metronome_bpm_minus_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  if(metronome.bpm > 10) {
    metronome.bpm--;
    extern void Metronome_SetBPM(uint16_t bpm);
    Metronome_SetBPM(metronome.bpm);
    
    if(metronome_bpm_slider != NULL) {
      lv_slider_set_value(metronome_bpm_slider, metronome.bpm, LV_ANIM_OFF);
    }
    if(metronome_bpm_label != NULL) {
      lv_label_set_text_fmt(metronome_bpm_label, "%d BPM", metronome.bpm);
    }
  }
}

/**
 * @brief  BPM +1 按钮回调
 */
static void metronome_bpm_plus_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  if(metronome.bpm < 500) {
    metronome.bpm++;
    extern void Metronome_SetBPM(uint16_t bpm);
    Metronome_SetBPM(metronome.bpm);
    
    if(metronome_bpm_slider != NULL) {
      lv_slider_set_value(metronome_bpm_slider, metronome.bpm, LV_ANIM_OFF);
    }
    if(metronome_bpm_label != NULL) {
      lv_label_set_text_fmt(metronome_bpm_label, "%d BPM", metronome.bpm);
    }
  }
}

/**
 * @brief  节拍数 -1 按钮回调
 */
static void metronome_beat_count_minus_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  if(metronome.beat_count > 2) {
    metronome.beat_count--;
    extern void Metronome_SetBeatCount(uint8_t beat_count);
    Metronome_SetBeatCount(metronome.beat_count);
    create_metronome_page();  // 重新创建页面以更新显示
  }
}

/**
 * @brief  节拍数 +1 按钮回调
 */
static void metronome_beat_count_plus_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  if(metronome.beat_count < METRONOME_MAX_BEAT_COUNT) {
    metronome.beat_count++;
    extern void Metronome_SetBeatCount(uint8_t beat_count);
    Metronome_SetBeatCount(metronome.beat_count);
    create_metronome_page();  // 重新创建页面以更新显示
  }
}

/**
 * @brief  播放/暂停按钮回调
 * @note   暂停时执行与返回按钮相同的停止逻辑，确保完全停止并清理状态
 */
static void metronome_play_pause_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  extern void Metronome_Start(void);
  extern void Metronome_Stop(void);
  
  // LOGI("[UI] Metronome play/pause button clicked, current state: %d\r\n", metronome.is_playing);
  
  if(metronome.is_playing) {
    // 暂停：执行与返回按钮相同的停止逻辑
    // 注意：这确保了暂停时也能像返回时一样完全停止并清理状态
    // LOGI("[UI] Pause: Pausing metronome (same logic as back button)...\r\n");
    // LOGI("[UI] Pause: Before Metronome_Stop(), current_page=%d\r\n", current_page);
    Metronome_Stop();  // 这会清理所有标志位和硬件状态
    // LOGI("[UI] Pause: Metronome_Stop() returned\r\n");
    metronome.is_playing = 0;
    // LOGI("[UI] Pause: After stop, current_page=%d (should still be PAGE_METRONOME)\r\n", current_page);
  } else {
    metronome.is_playing = 1;
    // LOGI("[UI] Play: Starting metronome...\r\n");
    Metronome_Start();
    // LOGI("[UI] Play: Metronome_Start() returned\r\n");
  }
  
  if(metronome_play_label != NULL) {
    lv_label_set_text(metronome_play_label, metronome.is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
  }
  
  // LOGI("[UI] Metronome state updated: is_playing=%d\r\n", metronome.is_playing);
}

/**
 * @brief  节拍强度切换回调
 */
static void metronome_beat_strength_cb(lv_event_t *e)
{
  uint8_t beat_index = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  
  if(beat_index >= METRONOME_MAX_BEAT_COUNT) {
    return;
  }
  
  // 循环切换强度：0(不出声) -> 1(弱) -> 2(中) -> 3(强) -> 0
  metronome.beat_strength[beat_index]++;
  if(metronome.beat_strength[beat_index] > 3) {
    metronome.beat_strength[beat_index] = 0;
  }
  
  // 更新显示
  uint8_t strength = metronome.beat_strength[beat_index];
  lv_color_t yellow = lv_palette_main(LV_PALETTE_YELLOW);
  lv_color_t white = lv_color_white();
  
  for(int j = 0; j < 3; j++) {
    if(metronome_beat_segments[beat_index][j] == NULL) {
      continue;
    }
    
    int segment_from_bottom = 2 - j;
    
    if(strength == 0) {
      lv_obj_set_style_bg_color(metronome_beat_segments[beat_index][j], white, 0);
    } else if(segment_from_bottom < strength) {
      lv_obj_set_style_bg_color(metronome_beat_segments[beat_index][j], yellow, 0);
    } else {
      lv_obj_set_style_bg_color(metronome_beat_segments[beat_index][j], white, 0);
    }
  }
}

/**
 * @brief  创建录音机页面
 */
static void create_recorder_page(void)
{
  // 先清理屏幕
  lv_obj_clean(lv_scr_act());
  
  /* 标题栏 */
  lv_obj_t *header = lv_obj_create(lv_scr_act());
  lv_obj_set_size(header, 320, 50);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, lv_palette_main(LV_PALETTE_RED), 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  
  /* 返回按钮 */
  lv_obj_t *btn_back = lv_btn_create(header);
  lv_obj_set_size(btn_back, 60, 40);
  lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 5, 0);
  lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *back_label = lv_label_create(btn_back);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT);
  lv_obj_center(back_label);
  
  /* 标题 */
  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, "Recorder");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_center(title);
  
  /* 录音状态显示 */
  lv_obj_t *recorder_status_label = lv_label_create(lv_scr_act());
  lv_label_set_text(recorder_status_label, "Ready");
  lv_obj_set_style_text_font(recorder_status_label, &lv_font_montserrat_16, 0);
  lv_obj_align(recorder_status_label, LV_ALIGN_TOP_MID, 0, 60);
  lv_obj_set_user_data(recorder_status_label, (void*)"status");  // 标记为状态标签
  
  /* 录音时长显示 */
  lv_obj_t *recorder_time_label = lv_label_create(lv_scr_act());
  lv_label_set_text(recorder_time_label, "00:00");
  lv_obj_set_style_text_font(recorder_time_label, &lv_font_montserrat_32, 0);
  lv_obj_align(recorder_time_label, LV_ALIGN_TOP_MID, 0, 85);
  lv_obj_set_user_data(recorder_time_label, (void*)"time");  // 标记为时间标签
  
  /* 录音按钮（播放/暂停） */
  lv_obj_t *recorder_play_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(recorder_play_btn, 60, 60);
  lv_obj_align(recorder_play_btn, LV_ALIGN_TOP_MID, -50, 130);
  lv_obj_set_style_bg_color(recorder_play_btn, lv_palette_main(LV_PALETTE_GREEN), 0);
  lv_obj_clear_flag(recorder_play_btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(recorder_play_btn, recorder_play_pause_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_set_user_data(recorder_play_btn, (void*)"play");  // 标记为播放按钮
  lv_obj_t *recorder_play_label = lv_label_create(recorder_play_btn);
  lv_label_set_text(recorder_play_label, LV_SYMBOL_PLAY);
  lv_obj_set_style_text_font(recorder_play_label, &lv_font_montserrat_24, 0);
  lv_obj_center(recorder_play_label);
  
  /* 停止按钮 */
  lv_obj_t *recorder_stop_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(recorder_stop_btn, 60, 60);
  lv_obj_align(recorder_stop_btn, LV_ALIGN_TOP_MID, 50, 130);
  lv_obj_set_style_bg_color(recorder_stop_btn, lv_palette_main(LV_PALETTE_RED), 0);
  lv_obj_clear_flag(recorder_stop_btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(recorder_stop_btn, recorder_stop_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *recorder_stop_label = lv_label_create(recorder_stop_btn);
  lv_label_set_text(recorder_stop_label, LV_SYMBOL_STOP);
  lv_obj_set_style_text_font(recorder_stop_label, &lv_font_montserrat_24, 0);
  lv_obj_center(recorder_stop_label);
  
  /* 录音文件列表标题 */
  lv_obj_t *list_title = lv_label_create(lv_scr_act());
  lv_label_set_text(list_title, "Recordings:");
  lv_obj_set_style_text_font(list_title, &lv_font_montserrat_14, 0);
  lv_obj_align(list_title, LV_ALIGN_TOP_LEFT, 10, 200);
  
  /* 创建文件列表容器 */
  lv_obj_t *recorder_list = lv_list_create(lv_scr_act());
  lv_obj_set_size(recorder_list, 300, 120);
  lv_obj_align(recorder_list, LV_ALIGN_TOP_LEFT, 10, 220);
  lv_obj_set_style_border_width(recorder_list, 1, 0);
  lv_obj_set_style_border_color(recorder_list, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_set_user_data(recorder_list, (void*)"filelist");  // 标记为文件列表
  
  /* 刷新文件列表 */
  recorder_refresh_file_list();
  
  LOGI("[UI] Recorder page created\r\n");
}

/**
 * @brief  录音播放/暂停按钮回调
 */
static void recorder_play_pause_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  
  extern uint8_t Recorder_Start(const char* file_path);
  extern void Recorder_Pause(void);
  extern void Recorder_Resume(void);
  extern recorder_state_t Recorder_GetState(void);
  
  recorder_state_t state = Recorder_GetState();
  
  LOGI("[UI] Recorder play/pause button clicked, current state: %d\r\n", state);
  
  if(state == RECORDER_STATE_IDLE) {
    // 开始录音
    LOGI("[UI] Starting recording...\r\n");
    
    // 在开始录音前，先停止并释放音频播放器的内存，避免内存碎片化
    Audio_Stop();
    HAL_Delay(50);  // 等待内存释放完成
    
    if(Recorder_Start(NULL) == 0) {  // NULL表示自动生成文件名
      LOGI("[UI] Recording started successfully\r\n");
    } else {
      LOGI("[UI] Failed to start recording\r\n");
    }
  } else if(state == RECORDER_STATE_RECORDING) {
    // 暂停录音
    LOGI("[UI] Pausing recording...\r\n");
    Recorder_Pause();
  } else if(state == RECORDER_STATE_PAUSED) {
    // 恢复录音
    LOGI("[UI] Resuming recording...\r\n");
    Recorder_Resume();
  }
  
  // 更新按钮显示
  lv_obj_t *btn = lv_event_get_target(e);
  lv_obj_t *label = lv_obj_get_child(btn, 0);
  state = Recorder_GetState();
  if(state == RECORDER_STATE_RECORDING) {
    lv_label_set_text(label, LV_SYMBOL_PAUSE);
  } else {
    lv_label_set_text(label, LV_SYMBOL_PLAY);
  }
  
  // 更新状态显示
  lv_obj_t *screen = lv_scr_act();
  uint32_t child_cnt = lv_obj_get_child_cnt(screen);
  for(uint32_t i = 0; i < child_cnt; i++) {
    lv_obj_t *child = lv_obj_get_child(screen, i);
    void *user_data = lv_obj_get_user_data(child);
    if(user_data && strcmp((char*)user_data, "status") == 0) {
      const char* status_text = "Ready";
      if(state == RECORDER_STATE_RECORDING) {
        status_text = "Recording";
      } else if(state == RECORDER_STATE_PAUSED) {
        status_text = "Paused";
      }
      lv_label_set_text(child, status_text);
      break;
    }
  }
}

/**
 * @brief  录音停止按钮回调
 */
static void recorder_stop_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  
  extern void Recorder_Stop(void);
  extern recorder_state_t Recorder_GetState(void);
  
  LOGI("[UI] Recorder stop button clicked\r\n");
  Recorder_Stop();
  LOGI("[UI] Recording stopped\r\n");
  
  // 停止后刷新文件列表（显示新录音的文件）
  recorder_refresh_file_list();
  
  // 更新播放按钮显示
  lv_obj_t *screen = lv_scr_act();
  uint32_t child_cnt = lv_obj_get_child_cnt(screen);
  for(uint32_t i = 0; i < child_cnt; i++) {
    lv_obj_t *child = lv_obj_get_child(screen, i);
    void *user_data = lv_obj_get_user_data(child);
    if(user_data && strcmp((char*)user_data, "play") == 0) {
      lv_obj_t *play_label = lv_obj_get_child(child, 0);
      if(play_label) {
        lv_label_set_text(play_label, LV_SYMBOL_PLAY);
      }
      break;
    }
  }
  
  // 更新状态显示
  for(uint32_t i = 0; i < child_cnt; i++) {
    lv_obj_t *child = lv_obj_get_child(screen, i);
    void *user_data = lv_obj_get_user_data(child);
    if(user_data && strcmp((char*)user_data, "status") == 0) {
      lv_label_set_text(child, "Ready");
      break;
    }
  }
  
  // 更新时间显示
  for(uint32_t i = 0; i < child_cnt; i++) {
    lv_obj_t *child = lv_obj_get_child(screen, i);
    void *user_data = lv_obj_get_user_data(child);
    if(user_data && strcmp((char*)user_data, "time") == 0) {
      lv_label_set_text(child, "00:00");
      break;
    }
  }
}

/**
 * @brief  刷新录音文件列表
 */
static void recorder_refresh_file_list(void)
{
  LOGI("[UI] Refreshing recorder file list...\r\n");
  
  // 查找文件列表对象
  lv_obj_t *screen = lv_scr_act();
  lv_obj_t *file_list = NULL;
  uint32_t child_cnt = lv_obj_get_child_cnt(screen);
  for(uint32_t i = 0; i < child_cnt; i++) {
    lv_obj_t *child = lv_obj_get_child(screen, i);
    void *user_data = lv_obj_get_user_data(child);
    if(user_data && strcmp((char*)user_data, "filelist") == 0) {
      file_list = child;
      break;
    }
  }
  
  if(file_list == NULL) {
    LOGI("[UI] File list not found\r\n");
    return;
  }
  
  // 清空列表
  lv_obj_clean(file_list);
  
  // 扫描RECORDER目录
  extern uint8_t Playlist_Scan(const char* path);
  extern uint8_t Playlist_GetCount(void);
  extern const char* Playlist_GetFileName(uint16_t index);
  
  uint8_t scan_result = Playlist_Scan("0:/RECORDER");
  uint8_t file_count = Playlist_GetCount();
  
  LOGI("[UI] Scanned RECORDER directory, found %d files\r\n", file_count);
  
  if(file_count > 0) {
    // 只显示最新的20个文件，从后往前显示（最新的在前）
    uint8_t display_count = (file_count > 20) ? 20 : file_count;
    uint8_t start_index = file_count - display_count;  // 从倒数第N个开始
    
    LOGI("[UI] Displaying latest %d files (from index %d to %d)\r\n", 
         display_count, start_index, file_count - 1);
    
    // 倒序添加文件到列表（最新的在前）
    for(int16_t i = file_count - 1; i >= (int16_t)start_index; i--) {
      const char *filename = Playlist_GetFileName(i);
      if(filename != NULL) {
        LOGI("[UI] Adding file %d: %s\r\n", i, filename);
        lv_obj_t *list_btn = lv_list_add_btn(file_list, LV_SYMBOL_FILE, filename);
        lv_obj_set_style_text_font(list_btn, &lv_font_montserrat_12, 0);
        lv_obj_set_user_data(list_btn, (void*)(uintptr_t)i);  // 保存原始索引
        lv_obj_add_event_cb(list_btn, recorder_file_item_cb, LV_EVENT_CLICKED, NULL);
      }
    }
  } else {
    // 没有文件时显示提示
    lv_obj_t *empty_label = lv_label_create(file_list);
    lv_label_set_text(empty_label, "No recordings found");
    lv_obj_set_style_text_font(empty_label, &lv_font_montserrat_12, 0);
    lv_obj_center(empty_label);
  }
  
  LOGI("[UI] File list refreshed\r\n");
}

/**
 * @brief  录音文件列表项点击回调
 */
static void recorder_file_item_cb(lv_event_t *e)
{
  lv_obj_t *btn = lv_event_get_target(e);
  uint16_t file_index = (uint16_t)(uintptr_t)lv_obj_get_user_data(btn);
  
  LOGI("[UI] Recorder file item clicked: index=%d\r\n", file_index);
  
  // 外部函数声明
  extern uint8_t Playlist_SetIndex(uint16_t index);
  extern const char* Playlist_GetCurrentFile(void);
  extern const char* Playlist_GetFileName(uint16_t index);
  
  // 设置播放列表索引
  Playlist_SetIndex(file_index);
  
  // 获取文件完整路径
  const char *filepath = Playlist_GetCurrentFile();
  const char *filename = Playlist_GetFileName(file_index);
  
  if(filepath != NULL && filename != NULL) {
    LOGI("[UI] Loading recording file: %s (path: %s)\r\n", filename, filepath);
    
    // 停止当前播放（如果有）
    Audio_Stop();
    
    // 切换到播放器页面
    current_page = PAGE_AUDIO_PLAYER;
    
    // 创建播放器页面UI
    create_player_page_stub();
    
    // 更新播放器状态：设置歌曲名
    strncpy(player.song_name, filename, sizeof(player.song_name)-1);
    player.song_name[sizeof(player.song_name)-1] = '\0';
    player.current_time = 0;
    player.progress = 0;
    player.is_playing = 0;
    
    // 更新UI标签（先显示默认值）
    if(player_song_label != NULL) {
      lv_label_set_text(player_song_label, player.song_name);
    }
    if(player_time_label != NULL) {
      lv_label_set_text_fmt(player_time_label, "00:00 / 00:00");
    }
    if(player_progress_slider != NULL) {
      slider_updating = 1;
      lv_slider_set_value(player_progress_slider, 0, LV_ANIM_OFF);
      slider_updating = 0;
    }
    
    // 加载文件（不自动播放，先获取总时长）
    Audio_Load(filepath, 0);  // 先加载但不播放
    
    // 立即获取总时长（Audio_Load已经解析了文件头）
    player.total_time = Audio_GetTotalTime();
    LOGI("[UI] Recording file loaded, total_time=%lu seconds\r\n", player.total_time);
    
    // 更新UI显示总时长
    if(player_time_label != NULL && player.total_time > 0) {
      uint32_t min = player.total_time / 60;
      uint32_t sec = player.total_time % 60;
      lv_label_set_text_fmt(player_time_label, "00:00 / %02lu:%02lu", min, sec);
    }
    
    // 现在开始播放
    Audio_Resume();  // 开始播放
    
    LOGI("[UI] Recording file loaded and playing\r\n");
  } else {
    LOGI("[UI] Failed to get file path or filename\r\n");
  }
}

/* ==================== 密码管理相关函数 ==================== */

/* 密码输入模式 */
#define PWD_INPUT_MODE_SET_NEW     0  /* 设置新密码（直接输入新密码） */
#define PWD_INPUT_MODE_VERIFY_OLD  1  /* 先验证旧密码，再设置新密码 */
#define PWD_INPUT_MODE_UNLOCK      2  /* 唤醒后解锁，仅验证，不设置 */

/* 密码管理页面UI控件指针 */
static lv_obj_t *pwd_switch = NULL;              ///< 密码保护开关
static lv_obj_t *pwd_status_label = NULL;        ///< 密码保护状态标签
static lv_obj_t *set_pwd_btn = NULL;             ///< 设置密码按钮

/* 密码输入页面UI控件指针和状态 */
static lv_obj_t *pwd_display_label = NULL;       ///< 密码显示标签
static lv_obj_t *pwd_num_keys[10] = {NULL};     ///< 数字键按钮数组 (0-9)
static lv_obj_t *pwd_del_btn = NULL;            ///< 删除键按钮
static lv_obj_t *pwd_clear_btn = NULL;          ///< 清除键按钮（C）
static lv_obj_t *pwd_confirm_btn = NULL;        ///< 确认按钮
static lv_obj_t *pwd_cancel_btn = NULL;         ///< 取消按钮
static lv_obj_t *pwd_hint_label = NULL;         ///< 输入提示标签（旧/新密码）
static uint8_t pwd_input_digits[4] = {0};       ///< 当前输入的密码
static uint8_t pwd_input_count = 0;             ///< 已输入的数字个数
static uint8_t pwd_input_mode = PWD_INPUT_MODE_SET_NEW; ///< 输入模式
static uint8_t pwd_old_verified = 0;            ///< 旧密码是否已验证通过（仅在启用状态下设置新密码时使用）
static page_t pwd_unlock_target_page = PAGE_MAIN_MENU;  ///< 解锁后要进入的页面

/**
 * @brief  密码管理按钮点击回调
 */
static void btn_password_management_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  log_button("Password");
  current_page = PAGE_SETTINGS_PASSWORD;
  create_password_management_page();
}

/**
 * @brief  密码保护开关回调
 */
static void btn_password_switch_cb(lv_event_t *e)
{
  lv_obj_t *sw = lv_event_get_target(e);
  uint8_t enabled = lv_obj_has_state(sw, LV_STATE_CHECKED) ? 1 : 0;
  
  PasswordManager_SetEnabled(enabled);
  
  // 更新状态标签
  if(pwd_status_label != NULL) {
    if(enabled) {
      lv_label_set_text(pwd_status_label, "Password Protection: ON");
      lv_obj_set_style_text_color(pwd_status_label, lv_color_hex(0x00AA00), 0);
    } else {
      lv_label_set_text(pwd_status_label, "Password Protection: OFF");
      lv_obj_set_style_text_color(pwd_status_label, lv_color_hex(0xAA0000), 0);
    }
  }
  
  // 更新设置密码按钮状态
  if(set_pwd_btn != NULL) {
    if(enabled) {
      lv_obj_clear_state(set_pwd_btn, LV_STATE_DISABLED);
    } else {
      lv_obj_add_state(set_pwd_btn, LV_STATE_DISABLED);
    }
  }
  
  LOGI("[PASSWORD] Protection %s\r\n", enabled ? "enabled" : "disabled");
}

/**
 * @brief  设置密码按钮点击回调
 */
static void btn_set_password_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  log_button("Set Password");
  
  // 如果密码保护已启用，需要先验证旧密码
  if(PasswordManager_IsEnabled()) {
    pwd_input_mode = PWD_INPUT_MODE_VERIFY_OLD;  // 验证模式
  } else {
    pwd_input_mode = PWD_INPUT_MODE_SET_NEW;     // 设置模式
  }
  pwd_old_verified = 0;
  
  // 重置输入状态
  pwd_input_count = 0;
  memset(pwd_input_digits, 0, 4);
  
  current_page = PAGE_SETTINGS_PASSWORD_INPUT;
  create_password_input_page();
}

/**
 * @brief  更新密码显示
 */
static void update_password_display(void)
{
  if(pwd_display_label == NULL) {
    return;
  }
  
  char display[10] = {0};
  for(uint8_t i = 0; i < 4; i++) {
    if(i < pwd_input_count) {
      // 正常模式：显示 *
      display[i * 2] = '*';
      display[i * 2 + 1] = ' ';
    } else {
      display[i * 2] = '_';
      display[i * 2 + 1] = ' ';
    }
  }
  
  lv_label_set_text(pwd_display_label, display);
}

/**
 * @brief  数字键点击回调
 */
static void btn_password_num_key_cb(lv_event_t *e)
{
  if(pwd_input_count >= 4) {
    return;  // 已输入4位，不能再输入
  }
  
  uint8_t digit = (uint8_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_target(e));
  pwd_input_digits[pwd_input_count++] = digit;
  
  update_password_display();
  
  // 如果已输入4位，可以确认
  if(pwd_input_count == 4 && pwd_confirm_btn != NULL) {
    lv_obj_clear_state(pwd_confirm_btn, LV_STATE_DISABLED);
  }
}

/**
 * @brief  删除键点击回调
 */
static void btn_password_del_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  
  if(pwd_input_count > 0) {
    pwd_input_count--;
    pwd_input_digits[pwd_input_count] = 0;
    update_password_display();
    
    // 禁用确认按钮
    if(pwd_confirm_btn != NULL) {
      lv_obj_add_state(pwd_confirm_btn, LV_STATE_DISABLED);
    }
  }
}

/**
 * @brief  清除键点击回调（清空全部输入）
 */
static void btn_password_clear_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  pwd_input_count = 0;
  memset(pwd_input_digits, 0, 4);
  update_password_display();
  if(pwd_confirm_btn != NULL) {
    lv_obj_add_state(pwd_confirm_btn, LV_STATE_DISABLED);
  }
}

/**
 * @brief  确认按钮点击回调
 */
static void btn_password_confirm_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  
  if(pwd_input_count != 4) {
    return;  // 未输入完整密码
  }
  
  if(pwd_input_mode == PWD_INPUT_MODE_VERIFY_OLD) {
    // 验证模式：验证旧密码（用于修改密码）
    if(!PasswordManager_VerifyPassword(pwd_input_digits)) {
      // 密码错误，显示提示
      show_message_box("Error", "Incorrect password!");
      // 清空输入
      pwd_input_count = 0;
      memset(pwd_input_digits, 0, 4);
      update_password_display();
      if(pwd_confirm_btn != NULL) {
        lv_obj_add_state(pwd_confirm_btn, LV_STATE_DISABLED);
      }
      return;
    }
    
    // 密码正确，切换到设置新密码模式
    pwd_input_mode = PWD_INPUT_MODE_SET_NEW;
    pwd_old_verified = 1;
    pwd_input_count = 0;
    memset(pwd_input_digits, 0, 4);
    update_password_display();
    
    // 更新提示文字
    if(pwd_hint_label != NULL) {
      lv_label_set_text(pwd_hint_label, "Enter New Password:");
    }
    
    if(pwd_confirm_btn != NULL) {
      lv_obj_add_state(pwd_confirm_btn, LV_STATE_DISABLED);
    }
    
    LOGI("[PASSWORD] Old password verified, ready to set new password\r\n");
    LOGI("[PASSWORD] Old password verified, ready to set new password\r\n");
  } else if(pwd_input_mode == PWD_INPUT_MODE_UNLOCK) {
    // 唤醒解锁：只校验，不修改密码
    if(!PasswordManager_VerifyPassword(pwd_input_digits)) {
      show_message_box("Error", "Incorrect password!");
      pwd_input_count = 0;
      memset(pwd_input_digits, 0, 4);
      update_password_display();
      if(pwd_confirm_btn != NULL) {
        lv_obj_add_state(pwd_confirm_btn, LV_STATE_DISABLED);
      }
      return;
    }

    // 校验通过，进入目标页（默认主菜单）
    current_page = pwd_unlock_target_page;
    music_assistant_init();
  } else {
    // 设置模式：保存新密码
    // 如果当前是启用状态，必须先通过旧密码验证才允许设置新密码
    if(PasswordManager_IsEnabled() && !pwd_old_verified) {
      show_message_box("Error", "Verify old password first!");
      return;
    }
    
    // 这里传 NULL：避免二次验证；启用状态下我们已经在上一步验证过旧密码
    if(PasswordManager_SetPassword(NULL, pwd_input_digits) == 0) {
      show_message_box("Success", "Password set successfully!");
      // 返回密码管理页面
      current_page = PAGE_SETTINGS_PASSWORD;
      create_password_management_page();
    } else {
      show_message_box("Error", "Failed to set password!");
    }
  }
}

/**
 * @brief  取消按钮点击回调
 */
static void btn_password_cancel_cb(lv_event_t *e)
{
  LV_UNUSED(e);
  
  // 解锁模式下不允许取消，防止绕过密码
  if(pwd_input_mode == PWD_INPUT_MODE_UNLOCK) {
    return;
  }

  // 返回密码管理页面
  current_page = PAGE_SETTINGS_PASSWORD;
  create_password_management_page();
}

/**
 * @brief  创建密码管理详细页面
 */
static void create_password_management_page(void)
{
  /* 清空当前屏幕 */
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), 0);
  
  /* 获取屏幕尺寸 */
  lv_coord_t hor_res = lv_disp_get_hor_res(NULL);
  
  /* 顶部标题栏 */
  lv_obj_t *header = lv_obj_create(lv_scr_act());
  lv_obj_set_size(header, hor_res, 50);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, lv_palette_main(LV_PALETTE_BLUE), 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  
  /* 返回按钮 */
  lv_obj_t *btn_back = lv_btn_create(header);
  lv_obj_set_size(btn_back, 60, 40);
  lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 5, 0);
  lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t *back_label = lv_label_create(btn_back);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT);
  lv_obj_center(back_label);
  
  /* 标题文字 */
  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, "Password");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_center(title);
  
  /* 密码保护状态标签 */
  pwd_status_label = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_font(pwd_status_label, &lv_font_montserrat_18, 0);
  lv_obj_align(pwd_status_label, LV_ALIGN_CENTER, 0, -60);
  
  /* 密码保护开关 */
  pwd_switch = lv_switch_create(lv_scr_act());
  lv_obj_align(pwd_switch, LV_ALIGN_CENTER, 0, -20);
  lv_obj_add_event_cb(pwd_switch, btn_password_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
  
  /* 设置密码按钮（长矩形） */
  set_pwd_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(set_pwd_btn, 200, 50);
  lv_obj_align(set_pwd_btn, LV_ALIGN_CENTER, 0, 40);
  lv_obj_add_event_cb(set_pwd_btn, btn_set_password_cb, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t *set_pwd_label = lv_label_create(set_pwd_btn);
  lv_label_set_text(set_pwd_label, "Set Password");
  lv_obj_set_style_text_font(set_pwd_label, &lv_font_montserrat_16, 0);
  lv_obj_center(set_pwd_label);
  
  /* 更新状态显示 */
  uint8_t enabled = PasswordManager_IsEnabled();
  if(enabled) {
    lv_obj_add_state(pwd_switch, LV_STATE_CHECKED);
    lv_label_set_text(pwd_status_label, "Password Protection: ON");
    lv_obj_set_style_text_color(pwd_status_label, lv_color_hex(0x00AA00), 0);
  } else {
    lv_obj_clear_state(pwd_switch, LV_STATE_CHECKED);
    lv_label_set_text(pwd_status_label, "Password Protection: OFF");
    lv_obj_set_style_text_color(pwd_status_label, lv_color_hex(0xAA0000), 0);
  }
}

/**
 * @brief  创建密码输入页面
 */
static void create_password_input_page(void)
{
  /* 清空当前屏幕 */
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), 0);
  
  /* 获取屏幕尺寸 */
  lv_coord_t hor_res = lv_disp_get_hor_res(NULL);
  
  /* 顶部标题栏 */
  lv_obj_t *header = lv_obj_create(lv_scr_act());
  lv_obj_set_size(header, hor_res, 50);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, lv_palette_main(LV_PALETTE_BLUE), 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  
  /* 返回按钮（解锁模式下不显示，避免绕过） */
  if(pwd_input_mode != PWD_INPUT_MODE_UNLOCK) {
    lv_obj_t *btn_back = lv_btn_create(header);
    lv_obj_set_size(btn_back, 60, 40);
    lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_add_event_cb(btn_back, btn_password_cancel_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(btn_back);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);
  }
  
  /* 标题文字 */
  if(pwd_input_mode != PWD_INPUT_MODE_UNLOCK) {
    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "Set Password");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_center(title);
  }
  
  /* 提示文字 */
  pwd_hint_label = lv_label_create(lv_scr_act());
  if(pwd_input_mode == PWD_INPUT_MODE_VERIFY_OLD) {
    lv_label_set_text(pwd_hint_label, "Enter Old Password:");
  } else if (pwd_input_mode == PWD_INPUT_MODE_SET_NEW) {
    lv_label_set_text(pwd_hint_label, "Enter New Password:");
  } else { // PWD_INPUT_MODE_UNLOCK
    lv_label_set_text(pwd_hint_label, "Enter Password:");
  }
  lv_obj_set_style_text_font(pwd_hint_label, &lv_font_montserrat_16, 0);
  /* 上移一点，给键盘留空间 */
  lv_obj_align(pwd_hint_label, LV_ALIGN_CENTER, 0, -170);
  
  /* 密码显示标签 */
  pwd_display_label = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_font(pwd_display_label, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(pwd_display_label, lv_color_black(), 0);
  lv_obj_set_style_text_align(pwd_display_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(pwd_display_label, 260);
  lv_label_set_long_mode(pwd_display_label, LV_LABEL_LONG_CLIP);
  lv_obj_align(pwd_display_label, LV_ALIGN_CENTER, 0, -140);
  lv_label_set_text(pwd_display_label, "_ _ _ _ ");
  update_password_display();
  
  /* 创建数字键盘容器 */
  lv_obj_t *keyboard_container = lv_obj_create(lv_scr_act());
  /* 恢复足够高度，保证 4 行按键完整显示（不会被裁剪导致可滚动） */
  lv_obj_set_size(keyboard_container, 210, 280);
  lv_obj_align(keyboard_container, LV_ALIGN_CENTER, 0, 45);
  lv_obj_set_style_bg_color(keyboard_container, lv_color_white(), 0);
  lv_obj_set_style_border_width(keyboard_container, 0, 0);
  lv_obj_set_style_pad_all(keyboard_container, 5, 0);
  
  /* 创建数字键 (1-9) */
  const char *num_labels[10] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
  const uint8_t num_values[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0};
  uint8_t key_positions[10][2] = {
    {0, 0}, {70, 0}, {140, 0},   // 1, 2, 3
    {0, 70}, {70, 70}, {140, 70}, // 4, 5, 6
    {0, 140}, {70, 140}, {140, 140}, // 7, 8, 9
    {70, 210}  // 0（放在8下面）
  };
  
  for(uint8_t i = 0; i < 10; i++) {
    pwd_num_keys[i] = lv_btn_create(keyboard_container);
    lv_obj_set_size(pwd_num_keys[i], 60, 60);
    lv_obj_set_pos(pwd_num_keys[i], key_positions[i][0], key_positions[i][1]);
    // 关键：让回调里拿到的 digit 与按键文字一致（1..9,0）
    lv_obj_set_user_data(pwd_num_keys[i], (void*)(uintptr_t)num_values[i]);
    lv_obj_add_event_cb(pwd_num_keys[i], btn_password_num_key_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *key_label = lv_label_create(pwd_num_keys[i]);
    lv_label_set_text(key_label, num_labels[i]);
    lv_obj_set_style_text_font(key_label, &lv_font_montserrat_20, 0);
    lv_obj_center(key_label);
  }
  
  /* 清除键 C（放在7下面） */
  pwd_clear_btn = lv_btn_create(keyboard_container);
  lv_obj_set_size(pwd_clear_btn, 60, 60);
  lv_obj_set_pos(pwd_clear_btn, 0, 210);
  lv_obj_add_event_cb(pwd_clear_btn, btn_password_clear_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_set_style_bg_color(pwd_clear_btn, lv_palette_main(LV_PALETTE_GREY), 0);
  
  lv_obj_t *c_label = lv_label_create(pwd_clear_btn);
  lv_label_set_text(c_label, "C");
  lv_obj_set_style_text_font(c_label, &lv_font_montserrat_20, 0);
  lv_obj_center(c_label);
  
  /* 删除键 */
  pwd_del_btn = lv_btn_create(keyboard_container);
  lv_obj_set_size(pwd_del_btn, 60, 60);
  lv_obj_set_pos(pwd_del_btn, 140, 210);
  lv_obj_add_event_cb(pwd_del_btn, btn_password_del_cb, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t *del_label = lv_label_create(pwd_del_btn);
  lv_label_set_text(del_label, LV_SYMBOL_LEFT);
  lv_obj_set_style_text_font(del_label, &lv_font_montserrat_20, 0);
  lv_obj_center(del_label);
  
  /* 确认按钮 */
  pwd_confirm_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(pwd_confirm_btn, 100, 40);
  if(pwd_input_mode == PWD_INPUT_MODE_UNLOCK) {
    lv_obj_align(pwd_confirm_btn, LV_ALIGN_BOTTOM_MID, 0, -8);
  } else {
    lv_obj_align(pwd_confirm_btn, LV_ALIGN_BOTTOM_MID, -60, -8);
  }
  lv_obj_add_event_cb(pwd_confirm_btn, btn_password_confirm_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_state(pwd_confirm_btn, LV_STATE_DISABLED);  // 初始禁用
  
  lv_obj_t *confirm_label = lv_label_create(pwd_confirm_btn);
  lv_label_set_text(confirm_label, "Confirm");
  lv_obj_center(confirm_label);
  
  /* 取消按钮 */
  pwd_cancel_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(pwd_cancel_btn, 100, 40);
  lv_obj_align(pwd_cancel_btn, LV_ALIGN_BOTTOM_MID, 60, -8);
  lv_obj_add_event_cb(pwd_cancel_btn, btn_password_cancel_cb, LV_EVENT_CLICKED, NULL);
  if(pwd_input_mode == PWD_INPUT_MODE_UNLOCK) {
    lv_obj_add_flag(pwd_cancel_btn, LV_OBJ_FLAG_HIDDEN);
  }
  
  lv_obj_t *cancel_label = lv_label_create(pwd_cancel_btn);
  lv_label_set_text(cancel_label, "Cancel");
  lv_obj_center(cancel_label);
}

/**
 * @brief  息屏唤醒后，如开启密码，则弹出解锁输入页
 */
void music_assistant_show_password_unlock(void)
{
  if(!PasswordManager_IsEnabled()) {
    return;
  }

  // 恢复到主菜单再锁屏解锁（保持与唤醒后的默认行为一致）
  pwd_unlock_target_page = PAGE_MAIN_MENU;

  pwd_input_mode = PWD_INPUT_MODE_UNLOCK;
  pwd_old_verified = 0;
  pwd_input_count = 0;
  memset(pwd_input_digits, 0, 4);

  current_page = PAGE_SETTINGS_PASSWORD_INPUT;
  create_password_input_page();
}
