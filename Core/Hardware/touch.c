#include "touch.h"

volatile _m_tp_dev tp_dev =
	{
		TP_Init,
		NULL,
		0,
		0,
		0,
	};
// 默认为touchtype=0的数据.

/* 触摸扫描标志位，中断里设置，主循环里检查 */
volatile uint8_t touch_scan_flag = 0;

/**
 * @brief 初始化触摸屏
 * 初始化触摸屏，并进行相关配置。
 * @return 初始化结果
 *         - 0：初始化成功
 *         - 1：初始化失败
 */
uint8_t TP_Init(void)
{
	if (FT6336_Init())
	{
		return 1;
	}
	tp_dev.init = FT6336_Init; // 初始化函数指向触摸屏初始化
	tp_dev.scan = FT6336_Scan; // 扫描函数指向GT911触摸屏扫描
	return 0;
}

/**
 * @brief GPIO外部中断回调函数。
 * 当指定的 GPIO 引脚发生外部中断事件时，该函数将被调用。
 * 注意：中断里不能调用阻塞的I2C函数，只设置标志位，在主循环里处理
 * @param GPIO_Pin GPIO 引脚号
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == CTP_INT_Pin)
	{
		/* 只设置标志位，不在这里调用扫描函数（避免I2C阻塞导致死锁） */
		touch_scan_flag = 1;
	}
}

/**
 * @brief 触摸扫描任务，在主循环里定期调用
 * 检查触摸中断标志位，如果有则执行扫描
 */
void TP_Scan_Task(void)
{
	if (touch_scan_flag)
	{
		touch_scan_flag = 0; // 清除标志
		tp_dev.scan(); // 在主循环里安全调用I2C扫描
	}
}
