#ifndef __MUSIC_ASSISTANT_H
#define __MUSIC_ASSISTANT_H

#include "lvgl.h"

/* 页面枚举，仅保留主菜单层级 */
typedef enum
{
  PAGE_MAIN_MENU = 0,
  PAGE_METRONOME,
  PAGE_TUNER,
  PAGE_AUDIO_PLAYER,
  PAGE_PLAYLIST,
  PAGE_RECORDER,
  PAGE_PRACTICE,
  PAGE_AUDIO_TEST,
  PAGE_BLUETOOTH,
  PAGE_SETTINGS,
  PAGE_SETTINGS_PASSWORD,
  PAGE_SETTINGS_PASSWORD_INPUT,
  PAGE_SCREEN_OFF,
} page_t;

extern page_t current_page;

/* 节拍器状态结构体（移植自v1.0） */
#define METRONOME_MAX_BEAT_COUNT     16  // 最大节拍数（兼容性：支持2-16拍）
typedef struct {
    uint16_t bpm;                    // 节拍速度 (10-500)
    uint8_t beat_count;              // 节拍数 (2-16)
    uint8_t current_beat;            // 当前节拍 (0-15)
    uint8_t is_playing;              // 是否播放中
    uint8_t beat_strength[METRONOME_MAX_BEAT_COUNT];  // 节拍点的强度等级 (0=不出声, 1=弱, 2=中, 3=强)
} metronome_t;

extern metronome_t metronome;  // 全局节拍器状态

/* 音乐播放器状态结构体（移植自v1.0） */
typedef struct {
    char song_name[64];      // 歌曲名称
    uint32_t current_time;   // 当前播放时间（秒）
    uint32_t total_time;     // 总时长（秒）
    uint8_t progress;        // 播放进度（0-100%）
    uint8_t is_playing;      // 是否正在播放
    uint8_t volume;          // 音量（0-100）
} player_t;

extern player_t player;  // 全局播放器状态

/* 初始化音乐助手主界面（主菜单） */
void music_assistant_init(void);

/* 创建播放列表页面 */
void create_playlist_page(void);

/* 更新播放器状态（移植自v1.0，在主循环中调用） */
void Player_Update(void);

/* 更新播放器页面UI（播放进度、时间显示等）- 保留兼容性 */
void music_assistant_update_player_ui(void);

/* 更新节拍器页面显示 */
void update_metronome_display(void);

/* 返回按钮回调（供其他模块使用） */
void btn_back_cb(lv_event_t *e);

/* 息屏唤醒后，如开启密码，则弹出解锁输入页 */
void music_assistant_show_password_unlock(void);

#endif /* __MUSIC_ASSISTANT_H */

