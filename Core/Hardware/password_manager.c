/**
  ******************************************************************************
  * @file    password_manager.c
  * @brief   密码管理模块实现
  * @author  项目开发者
  * @date    2025-01-xx
  * @note    提供密码管理的上层接口
  ******************************************************************************
  */

#include "password_manager.h"
#include "password_storage.h"
#include "debug_uart.h"
#include <string.h>

// 内存中的配置缓存
static password_config_t cached_config;
static uint8_t initialized = 0;

/**
 * @brief  初始化密码管理模块
 */
void PasswordManager_Init(void)
{
    if(initialized) {
        return;
    }
    
    LOGI("[PASSWORD_MANAGER] Initializing...\r\n");
    
    // 初始化 Flash 存储模块
    if(PasswordStorage_Init() != 0) {
        LOGI("[PASSWORD_MANAGER] Storage init failed, using default config\r\n");
        // 使用默认配置
        cached_config.magic = PASSWORD_MAGIC;
        cached_config.enabled = 0;
        cached_config.password[0] = 8;
        cached_config.password[1] = 8;
        cached_config.password[2] = 8;
        cached_config.password[3] = 8;
        cached_config.reserved[0] = 0;
        cached_config.reserved[1] = 0;
        cached_config.checksum = PasswordStorage_CalculateChecksum(&cached_config);
    } else {
        // 读取配置到缓存
        if(PasswordStorage_Read(&cached_config) != 0) {
            LOGI("[PASSWORD_MANAGER] Read failed, using default config\r\n");
            // 使用默认配置
            cached_config.magic = PASSWORD_MAGIC;
            cached_config.enabled = 0;
            cached_config.password[0] = 8;
            cached_config.password[1] = 8;
            cached_config.password[2] = 8;
            cached_config.password[3] = 8;
            cached_config.reserved[0] = 0;
            cached_config.reserved[1] = 0;
            cached_config.checksum = PasswordStorage_CalculateChecksum(&cached_config);
        }
    }
    
    initialized = 1;
    LOGI("[PASSWORD_MANAGER] Initialized: enabled=%d\r\n", cached_config.enabled);
}

/**
 * @brief  检查密码保护是否启用
 * @return 1=启用, 0=禁用
 */
uint8_t PasswordManager_IsEnabled(void)
{
    if(!initialized) {
        PasswordManager_Init();
    }
    
    return cached_config.enabled;
}

/**
 * @brief  设置密码保护启用/禁用状态
 * @param  enabled: 1=启用, 0=禁用
 * @return 0=成功, 1=失败
 */
uint8_t PasswordManager_SetEnabled(uint8_t enabled)
{
    if(!initialized) {
        PasswordManager_Init();
    }
    
    cached_config.enabled = (enabled != 0) ? 1 : 0;
    
    // 写入 Flash
    if(PasswordStorage_Write(&cached_config) != 0) {
        LOGI("[PASSWORD_MANAGER] Failed to save enabled state\r\n");
        return 1;
    }
    
    LOGI("[PASSWORD_MANAGER] Enabled state set to %d\r\n", cached_config.enabled);
    return 0;
}

/**
 * @brief  验证密码是否正确
 * @param  password: 四位密码数组
 * @return 1=正确, 0=错误
 */
uint8_t PasswordManager_VerifyPassword(const uint8_t *password)
{
    if(password == NULL) {
        return 0;
    }
    
    if(!initialized) {
        PasswordManager_Init();
    }
    
    // 如果密码保护未启用，直接返回正确
    if(!cached_config.enabled) {
        return 1;
    }
    
    // 比较密码
    for(uint8_t i = 0; i < 4; i++) {
        if(password[i] != cached_config.password[i]) {
            LOGI("[PASSWORD_MANAGER] Password verification failed at digit %d\r\n", i);
            return 0;
        }
    }
    
    LOGI("[PASSWORD_MANAGER] Password verified successfully\r\n");
    return 1;
}

/**
 * @brief  设置新密码
 * @param  old_password: 旧密码（如果已设置密码，需要验证）
 * @param  new_password: 新密码（四位数字数组）
 * @return 0=成功, 1=失败
 */
uint8_t PasswordManager_SetPassword(const uint8_t *old_password, const uint8_t *new_password)
{
    if(new_password == NULL) {
        return 1;
    }
    
    if(!initialized) {
        PasswordManager_Init();
    }
    
    // 如果已启用密码保护，需要验证旧密码
    if(cached_config.enabled && old_password != NULL) {
        if(!PasswordManager_VerifyPassword(old_password)) {
            LOGI("[PASSWORD_MANAGER] Old password verification failed\r\n");
            return 1;
        }
    }
    
    // 验证新密码有效性（每个数字应在 0-9 范围内）
    for(uint8_t i = 0; i < 4; i++) {
        if(new_password[i] > 9) {
            LOGI("[PASSWORD_MANAGER] Invalid new password digit[%d]: %d\r\n", i, new_password[i]);
            return 1;
        }
    }
    
    // 更新密码
    memcpy(cached_config.password, new_password, 4);
    
    // 写入 Flash
    if(PasswordStorage_Write(&cached_config) != 0) {
        LOGI("[PASSWORD_MANAGER] Failed to save new password\r\n");
        return 1;
    }
    
    LOGI("[PASSWORD_MANAGER] Password set successfully: %d%d%d%d\r\n",
         cached_config.password[0], cached_config.password[1],
         cached_config.password[2], cached_config.password[3]);
    
    return 0;
}

/**
 * @brief  获取当前密码（用于显示，需要验证）
 * @param  password: 输出参数，四位密码数组
 */
void PasswordManager_GetPassword(uint8_t *password)
{
    if(password == NULL) {
        return;
    }
    
    if(!initialized) {
        PasswordManager_Init();
    }
    
    memcpy(password, cached_config.password, 4);
}
