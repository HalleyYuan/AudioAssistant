#include "debug_uart.h"
#include <stdio.h>

/* 串口调试模块：使用 USART1，阻塞发送，避免打断 CubeMX 生成的 usart.c */

/* 默认关闭调试输出，避免量产时串口刷屏 */
static debug_level_t s_log_level = DEBUG_LEVEL_INFO;  // 临时启用调试输出

void DebugUart_Init(void)
{
  /* USART1 已由 MX_USART1_UART_Init() 完成，这里仅保留接口占位 */
  s_log_level = DEBUG_LEVEL_INFO;
}

void DebugUart_SetLevel(debug_level_t level)
{
  s_log_level = level;
}

void DebugUart_Printf(debug_level_t level, const char *fmt, ...)
{
  if (level > s_log_level)
  {
    return;
  }

  char buf[256];
  int len;
  va_list args;
  va_start(args, fmt);
  len = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  if (len <= 0)
  {
    return;
  }

  /* 限制长度，确保不会越界 */
  if (len > (int)sizeof(buf))
  {
    len = sizeof(buf);
  }

  HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)len, 100);
}
