#ifndef __DEBUG_UART_H
#define __DEBUG_UART_H

#include "main.h"
#include "usart.h"
#include <stdarg.h>

/* 简单的串口调试模块，复用 USART1 */

typedef enum
{
  DEBUG_LEVEL_OFF = 0,
  DEBUG_LEVEL_ERROR,
  DEBUG_LEVEL_WARN,
  DEBUG_LEVEL_INFO,
  DEBUG_LEVEL_DEBUG,
} debug_level_t;

void DebugUart_Init(void);
void DebugUart_SetLevel(debug_level_t level);
void DebugUart_Printf(debug_level_t level, const char *fmt, ...);

/* 便捷宏 - 已注释掉所有串口调试输出 */
// #define LOGE(fmt, ...) DebugUart_Printf(DEBUG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
// #define LOGW(fmt, ...) DebugUart_Printf(DEBUG_LEVEL_WARN, fmt, ##__VA_ARGS__)
// #define LOGI(fmt, ...) DebugUart_Printf(DEBUG_LEVEL_INFO, fmt, ##__VA_ARGS__)
// #define LOGD(fmt, ...) DebugUart_Printf(DEBUG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...)  // 注释掉串口调试输出
#define LOGW(fmt, ...)  // 注释掉串口调试输出
#define LOGI(fmt, ...)  // 注释掉串口调试输出
#define LOGD(fmt, ...)  // 注释掉串口调试输出

#endif /* __DEBUG_UART_H */
