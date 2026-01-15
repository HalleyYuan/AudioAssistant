#ifndef __PASSWORD_MANAGER_H
#define __PASSWORD_MANAGER_H

#include "main.h"
#include "password_storage.h"

// 函数声明
void PasswordManager_Init(void);
uint8_t PasswordManager_IsEnabled(void);
uint8_t PasswordManager_SetEnabled(uint8_t enabled);
uint8_t PasswordManager_SetPassword(const uint8_t *old_password, const uint8_t *new_password);
uint8_t PasswordManager_VerifyPassword(const uint8_t *password);
void PasswordManager_GetPassword(uint8_t *password);

#endif /* __PASSWORD_MANAGER_H */
