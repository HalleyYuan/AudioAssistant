#ifndef __PASSWORD_STORAGE_H
#define __PASSWORD_STORAGE_H

#include "main.h"

// Flash 存储配置
#define PASSWORD_FLASH_SECTOR      FLASH_SECTOR_11  // STM32F407ZGT6 最后一个扇区
#define PASSWORD_FLASH_ADDRESS     0x080E0000       // Sector 11 起始地址
#define PASSWORD_FLASH_SIZE         128*1024         // 128KB

// 数据魔数（用于验证数据有效性）
// 注意：这里带版本号。升级该值会让旧配置自动失效，从而回到默认密码。
#define PASSWORD_MAGIC             0x50415332        // "PAS2"

// 密码配置数据结构
typedef struct {
    uint32_t magic;          // 魔数 0x50415353 ("PASS")
    uint8_t enabled;         // 密码保护是否启用 (0=禁用, 1=启用)
    uint8_t password[4];     // 四位密码 (每个字节存储一个数字 0-9)
    uint8_t reserved[2];    // 保留字段，用于对齐
    uint32_t checksum;      // 校验和，用于数据完整性验证
} password_config_t;

// 函数声明
uint8_t PasswordStorage_Init(void);
uint8_t PasswordStorage_Read(password_config_t *config);
uint8_t PasswordStorage_Write(const password_config_t *config);
uint8_t PasswordStorage_Erase(void);
uint8_t PasswordStorage_Verify(const password_config_t *config);
uint32_t PasswordStorage_CalculateChecksum(const password_config_t *config);

#endif /* __PASSWORD_STORAGE_H */
