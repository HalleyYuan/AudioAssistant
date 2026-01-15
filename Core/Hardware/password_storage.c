/**
  ******************************************************************************
  * @file    password_storage.c
  * @brief   Flash 存储模块实现
  * @author  项目开发者
  * @date    2025-01-xx
  * @note    使用 STM32F407 的 Flash Sector 11 存储密码配置
  ******************************************************************************
  */

#include "password_storage.h"
#include "stm32f4xx_hal_flash.h"
#include "debug_uart.h"
#include <string.h>

// 默认配置（密码保护禁用，密码为 8888）
static password_config_t default_config = {
    .magic = PASSWORD_MAGIC,
    .enabled = 0,
    .password = {8, 8, 8, 8},
    .reserved = {0, 0},
    .checksum = 0
};

/**
 * @brief  计算校验和
 * @param  config: 密码配置结构体指针
 * @return 校验和值
 */
uint32_t PasswordStorage_CalculateChecksum(const password_config_t *config)
{
    uint32_t sum = 0;
    const uint8_t *data = (const uint8_t *)config;
    
    // 计算除 checksum 字段外的所有数据的校验和
    for(uint32_t i = 0; i < offsetof(password_config_t, checksum); i++) {
        sum += data[i];
    }
    
    return sum;
}

/**
 * @brief  验证数据有效性
 * @param  config: 密码配置结构体指针
 * @return 0=有效, 1=无效
 */
uint8_t PasswordStorage_Verify(const password_config_t *config)
{
    if(config == NULL) {
        return 1;
    }
    
    // 检查魔数
    if(config->magic != PASSWORD_MAGIC) {
        LOGI("[PASSWORD_STORAGE] Invalid magic: 0x%08lX\r\n", config->magic);
        return 1;
    }
    
    // 检查校验和
    uint32_t calculated_checksum = PasswordStorage_CalculateChecksum(config);
    if(config->checksum != calculated_checksum) {
        LOGI("[PASSWORD_STORAGE] Checksum mismatch: stored=0x%08lX, calculated=0x%08lX\r\n", 
             config->checksum, calculated_checksum);
        return 1;
    }
    
    // 检查密码值是否有效（每个数字应在 0-9 范围内）
    for(uint8_t i = 0; i < 4; i++) {
        if(config->password[i] > 9) {
            LOGI("[PASSWORD_STORAGE] Invalid password digit[%d]: %d\r\n", i, config->password[i]);
            return 1;
        }
    }
    
    return 0;
}

/**
 * @brief  初始化 Flash 存储模块
 * @return 0=成功, 1=失败
 */
uint8_t PasswordStorage_Init(void)
{
    LOGI("[PASSWORD_STORAGE] Initializing...\r\n");
    
    // 读取 Flash 中的数据
    password_config_t config;
    if(PasswordStorage_Read(&config) == 0) {
        // 数据有效
        LOGI("[PASSWORD_STORAGE] Valid config found: enabled=%d, password=%d%d%d%d\r\n",
             config.enabled, config.password[0], config.password[1], 
             config.password[2], config.password[3]);
        return 0;
    } else {
        // 数据无效，写入默认配置
        LOGI("[PASSWORD_STORAGE] Invalid config, writing default...\r\n");
        default_config.checksum = PasswordStorage_CalculateChecksum(&default_config);
        return PasswordStorage_Write(&default_config);
    }
}

/**
 * @brief  从 Flash 读取密码配置
 * @param  config: 输出参数，读取的配置数据
 * @return 0=成功, 1=失败
 */
uint8_t PasswordStorage_Read(password_config_t *config)
{
    if(config == NULL) {
        return 1;
    }
    
    // 从 Flash 地址读取数据
    const password_config_t *flash_config = (const password_config_t *)PASSWORD_FLASH_ADDRESS;
    
    // 复制数据到输出参数
    memcpy(config, flash_config, sizeof(password_config_t));
    
    // 验证数据有效性
    if(PasswordStorage_Verify(config) != 0) {
        return 1;
    }
    
    return 0;
}

/**
 * @brief  擦除 Flash 扇区
 * @return 0=成功, 1=失败
 */
uint8_t PasswordStorage_Erase(void)
{
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;
    HAL_StatusTypeDef status;
    
    // 解锁 Flash
    HAL_FLASH_Unlock();
    
    // 配置擦除参数
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;  // 2.7V - 3.6V
    EraseInitStruct.Sector = PASSWORD_FLASH_SECTOR;
    EraseInitStruct.NbSectors = 1;
    
    // 执行擦除
    status = HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);
    
    // 锁定 Flash
    HAL_FLASH_Lock();
    
    if(status != HAL_OK) {
        LOGI("[PASSWORD_STORAGE] Erase failed: status=%d, SectorError=%lu\r\n", status, SectorError);
        return 1;
    }
    
    LOGI("[PASSWORD_STORAGE] Sector erased successfully\r\n");
    return 0;
}

/**
 * @brief  写入密码配置到 Flash
 * @param  config: 要写入的配置数据
 * @return 0=成功, 1=失败
 */
uint8_t PasswordStorage_Write(const password_config_t *config)
{
    if(config == NULL) {
        return 1;
    }
    
    // 验证输入数据
    password_config_t temp_config = *config;
    temp_config.checksum = PasswordStorage_CalculateChecksum(&temp_config);
    
    if(PasswordStorage_Verify(&temp_config) != 0) {
        LOGI("[PASSWORD_STORAGE] Invalid config data\r\n");
        return 1;
    }
    
    // 擦除扇区
    if(PasswordStorage_Erase() != 0) {
        return 1;
    }
    
    // 解锁 Flash
    HAL_FLASH_Unlock();
    
    // 写入数据（按 32 位写入）
    const uint32_t *src = (const uint32_t *)&temp_config;
    uint32_t *dst = (uint32_t *)PASSWORD_FLASH_ADDRESS;
    uint32_t word_count = sizeof(password_config_t) / 4;
    
    HAL_StatusTypeDef status = HAL_OK;
    for(uint32_t i = 0; i < word_count; i++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, 
                                   (uint32_t)(dst + i), 
                                   src[i]);
        if(status != HAL_OK) {
            LOGI("[PASSWORD_STORAGE] Write failed at word %lu: status=%d\r\n", i, status);
            break;
        }
    }
    
    // 锁定 Flash
    HAL_FLASH_Lock();
    
    if(status != HAL_OK) {
        return 1;
    }
    
    // 验证写入的数据
    password_config_t verify_config;
    if(PasswordStorage_Read(&verify_config) != 0) {
        LOGI("[PASSWORD_STORAGE] Write verification failed\r\n");
        return 1;
    }
    
    LOGI("[PASSWORD_STORAGE] Config written successfully: enabled=%d, password=%d%d%d%d\r\n",
         verify_config.enabled, verify_config.password[0], verify_config.password[1],
         verify_config.password[2], verify_config.password[3]);
    
    return 0;
}
