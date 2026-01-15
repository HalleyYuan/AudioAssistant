/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    i2s.c
  * @brief   This file provides code for the configuration
  *          of the I2S instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "i2s.h"

/* USER CODE BEGIN 0 */
#include "debug_uart.h"

/**
 * @brief 打印I2SCFGR寄存器的详细状态
 */
static void I2S2_PrintI2SCFGR(const char* location, uint32_t i2scfgr)
{
    // 正确的位定义（参考 RM0090）:
    // bit11: I2SMOD, bit10: I2SE, bit9:8 I2SCFG, bit7: PCMSYNC,
    // bit5:4 I2SSTD, bit3: CKPOL, bit2:1 DATLEN, bit0: CHLEN
    uint8_t chlen  = (i2scfgr >> 0) & 0x1;
    uint8_t datlen = (i2scfgr >> 1) & 0x3;
    uint8_t ckpol  = (i2scfgr >> 3) & 0x1;
    uint8_t i2sstd = (i2scfgr >> 4) & 0x3;
    uint8_t pcmsync= (i2scfgr >> 7) & 0x1;
    uint8_t i2scfg = (i2scfgr >> 8) & 0x3;
    uint8_t i2se   = (i2scfgr >> 10) & 0x1;
    uint8_t i2smod = (i2scfgr >> 11) & 0x1;
    
    const char* mode_str[]   = {"SlaveTX", "SlaveRX", "MasterTX", "MasterRX"};
    const char* datlen_str[] = {"16bit", "24bit", "32bit", "reserved"};
    const char* std_str[]    = {"Philips", "MSB", "LSB", "PCM"};

    LOGI("[I2S2_DEBUG] %s: I2SCFGR=0x%08lX\r\n", location, i2scfgr);
    LOGI("[I2S2_DEBUG]   I2SMOD=%d (1=I2S), I2SE=%d, I2SCFG=%d(%s)\r\n",
         i2smod, i2se, i2scfg, mode_str[i2scfg]);
    LOGI("[I2S2_DEBUG]   I2SSTD=%d(%s), DATLEN=%d(%s), CHLEN=%d, CKPOL=%d, PCMSYNC=%d\r\n",
         i2sstd, std_str[i2sstd], datlen, datlen_str[datlen], chlen, ckpol, pcmsync);
}

/**
 * @brief 打印I2S2状态寄存器
 */
static void I2S2_PrintSR(const char* location)
{
    uint32_t sr = SPI2->SR;
    LOGI("[I2S2_DEBUG] %s: SR=0x%08lX (TXE=%d, RXNE=%d, BSY=%d, UDR=%d, CHSIDE=%d)\r\n",
         location, sr,
         (sr & SPI_SR_TXE) ? 1 : 0,
         (sr & SPI_SR_RXNE) ? 1 : 0,
         (sr & SPI_SR_BSY) ? 1 : 0,
         (sr & SPI_SR_UDR) ? 1 : 0,
         (sr & SPI_SR_CHSIDE) ? 1 : 0);
}

/* USER CODE END 0 */

I2S_HandleTypeDef hi2s2;
DMA_HandleTypeDef hdma_spi2_tx;
DMA_HandleTypeDef hdma_i2s2_ext_rx;

/* I2S2 init function */
void MX_I2S2_Init(void)
{

  /* USER CODE BEGIN I2S2_Init 0 */

  /* USER CODE END I2S2_Init 0 */

  /* USER CODE BEGIN I2S2_Init 1 */

  /* USER CODE END I2S2_Init 1 */
  hi2s2.Instance = SPI2;
  hi2s2.Init.Mode = I2S_MODE_MASTER_TX;
  hi2s2.Init.Standard = I2S_STANDARD_PHILIPS;
  hi2s2.Init.DataFormat = I2S_DATAFORMAT_16B;
  hi2s2.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
  hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_44K;
  hi2s2.Init.CPOL = I2S_CPOL_LOW;
  hi2s2.Init.ClockSource = I2S_CLOCK_PLL;
  hi2s2.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_ENABLE;
  if (HAL_I2S_Init(&hi2s2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2S2_Init 2 */

  /* USER CODE END I2S2_Init 2 */

}

void HAL_I2S_MspInit(I2S_HandleTypeDef* i2sHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(i2sHandle->Instance==SPI2)
  {
  /* USER CODE BEGIN SPI2_MspInit 0 */

  /* USER CODE END SPI2_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2S;
    PeriphClkInitStruct.PLLI2S.PLLI2SN = 192;
    PeriphClkInitStruct.PLLI2S.PLLI2SR = 2;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* I2S2 clock enable */
    __HAL_RCC_SPI2_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**I2S2 GPIO Configuration
    PC2     ------> I2S2_ext_SD
    PC3     ------> I2S2_SD
    PB10     ------> I2S2_CK
    PB12     ------> I2S2_WS
    PC6     ------> I2S2_MCK
    */
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF6_I2S2ext;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* I2S2 DMA Init */
    /* SPI2_TX Init */
    hdma_spi2_tx.Instance = DMA1_Stream4;
    hdma_spi2_tx.Init.Channel = DMA_CHANNEL_0;
    hdma_spi2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_spi2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_spi2_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_spi2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_spi2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_spi2_tx.Init.Mode = DMA_CIRCULAR;
    hdma_spi2_tx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_spi2_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_spi2_tx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(i2sHandle,hdmatx,hdma_spi2_tx);

    /* I2S2_EXT_RX Init */
    hdma_i2s2_ext_rx.Instance = DMA1_Stream3;
    hdma_i2s2_ext_rx.Init.Channel = DMA_CHANNEL_3;
    hdma_i2s2_ext_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_i2s2_ext_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_i2s2_ext_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_i2s2_ext_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_i2s2_ext_rx.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_i2s2_ext_rx.Init.Mode = DMA_CIRCULAR;
    hdma_i2s2_ext_rx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_i2s2_ext_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_i2s2_ext_rx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(i2sHandle,hdmarx,hdma_i2s2_ext_rx);

    /* I2S2 interrupt Init */
    HAL_NVIC_SetPriority(SPI2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(SPI2_IRQn);
  /* USER CODE BEGIN SPI2_MspInit 1 */

  /* USER CODE END SPI2_MspInit 1 */
  }
}

void HAL_I2S_MspDeInit(I2S_HandleTypeDef* i2sHandle)
{

  if(i2sHandle->Instance==SPI2)
  {
  /* USER CODE BEGIN SPI2_MspDeInit 0 */

  /* USER CODE END SPI2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_SPI2_CLK_DISABLE();

    /**I2S2 GPIO Configuration
    PC2     ------> I2S2_ext_SD
    PC3     ------> I2S2_SD
    PB10     ------> I2S2_CK
    PB12     ------> I2S2_WS
    PC6     ------> I2S2_MCK
    */
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_6);

    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10|GPIO_PIN_12);

    /* I2S2 DMA DeInit */
    HAL_DMA_DeInit(i2sHandle->hdmatx);
    HAL_DMA_DeInit(i2sHandle->hdmarx);

    /* I2S2 interrupt Deinit */
    HAL_NVIC_DisableIRQ(SPI2_IRQn);
  /* USER CODE BEGIN SPI2_MspDeInit 1 */

  /* USER CODE END SPI2_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

// I2S DMA回调函数指针
void (*i2s_tx_callback)(void) = NULL;
void (*i2s_rx_callback)(void) = NULL;

// I2S采样率配置表
// 注意：原配置表基于PLLM=8，但我们的系统时钟配置中PLLM=4
// 因此需要将PLLI2SN值减半，以保持相同的I2S时钟频率
// 公式：I2SxCLK = (HSE / PLLM) * PLLI2SN / PLLI2SR
// 当PLLM从8变为4时，PLLI2SN需要减半才能得到相同的I2S时钟
const uint16_t I2S_PSC_TBL[][5] = {
    {800,  128, 5, 12, 1},  // 8KHz (原256/2=128)
    {1102, 215, 4, 19, 0},  // 11.025KHz (原429/2=214.5，取215)
    {1600, 107, 2, 13, 0},  // 16KHz (原213/2=106.5，取107)
    {2205, 215, 4,  9, 1},  // 22.05KHz (原429/2=214.5，取215)
    {3200, 107, 2,  6, 1},  // 32KHz (原213/2=106.5，取107)
    {4410, 136, 2,  6, 0},  // 44.1KHz (原271/2=135.5，取136)
    {4800, 129, 3,  3, 1},  // 48KHz (原258/2=129)
    {8820, 158, 2,  3, 1},  // 88.2KHz (原316/2=158)
    {9600, 172, 2,  3, 1},  // 96KHz (原344/2=172)
    {17640, 181, 2, 2, 0},  // 176.4KHz (原361/2=180.5，取181)
    {19200, 197, 2, 2, 0}   // 192KHz (原393/2=196.5，取197)
};

/**
 * @brief I2S2初始化（完全按照参考例程，不使用HAL库）
 * 修改位置：Core/Src/i2s.c - I2S2_Init()函数
 * 修改内容：移除HAL_I2S_Init()调用，直接操作寄存器配置I2S2
 */
void I2S2_Init(uint16_t I2S_Standard, uint16_t I2S_Mode, uint16_t I2S_Clock_Polarity, uint16_t I2S_DataFormat)
{
    LOGI("[I2S2_DEBUG] ===== I2S2_Init() START =====\r\n");
    LOGI("[I2S2_DEBUG] Params: Standard=%d, Mode=%d, CPOL=%d, DataFormat=%d\r\n",
         I2S_Standard, I2S_Mode, I2S_Clock_Polarity, I2S_DataFormat);
    
    // 读取初始状态
    uint32_t i2scfgr_before = SPI2->I2SCFGR;
    I2S2_PrintI2SCFGR("Before reset", i2scfgr_before);
    I2S2_PrintSR("Before reset");
    
    // 参考例程：先复位SPI2，然后配置I2S2
    // RCC_APB1PeriphResetCmd(RCC_APB1Periph_SPI2,ENABLE);
    // RCC_APB1PeriphResetCmd(RCC_APB1Periph_SPI2,DISABLE);
    LOGI("[I2S2_DEBUG] Resetting SPI2...\r\n");
    __HAL_RCC_SPI2_FORCE_RESET();
    __HAL_RCC_SPI2_RELEASE_RESET();
    
    // 读取复位后的状态
    uint32_t i2scfgr_after_reset = SPI2->I2SCFGR;
    I2S2_PrintI2SCFGR("After reset", i2scfgr_after_reset);
    
    // 先禁用I2S2
    LOGI("[I2S2_DEBUG] Disabling I2S2 (clearing I2SE bit)...\r\n");
    SPI2->I2SCFGR &= ~SPI_I2SCFGR_I2SE;
    uint32_t i2scfgr_after_disable = SPI2->I2SCFGR;
    I2S2_PrintI2SCFGR("After disable", i2scfgr_after_disable);
    
    // 配置I2SCFGR寄存器
    // bit0: I2SE (I2S使能) - 稍后设置
    // bit8-9: I2SCFG[1:0] (I2S配置模式)
    //   00: Slave transmit
    //   01: Slave receive
    //   10: Master transmit  <- 我们需要这个
    //   11: Master receive
    // bit11: I2SMOD (I2S模式，不是SPI模式) = 1
    // bit10: I2SSTD (I2S标准) = 0 (Philips)
    // bit4-5: DATLEN[1:0] (数据长度)
    //   00: 16bit
    //   01: 24bit
    //   10: 32bit
    // bit1: CKPOL (时钟极性)
    //   0: 低电平有效
    //   1: 高电平有效
    
    uint32_t i2scfgr = 0;
    
    // I2SMOD = 1 (I2S模式)
    i2scfgr |= SPI_I2SCFGR_I2SMOD;
    
    // I2SCFG[1:0] - 根据I2S_Mode设置
    if(I2S_Mode == I2S_Mode_MasterTx) {
        i2scfgr |= (2 << 8);  // Master TX (10)
    } else if(I2S_Mode == I2S_Mode_SlaveTx) {
        i2scfgr |= (0 << 8);  // Slave TX (00)
    } else if(I2S_Mode == I2S_Mode_MasterRx) {
        i2scfgr |= (3 << 8);  // Master RX (11)
    } else if(I2S_Mode == I2S_Mode_SlaveRx) {
        i2scfgr |= (1 << 8);  // Slave RX (01)
    } else {
        i2scfgr |= (2 << 8);  // 默认Master TX
    }
    
    // I2SSTD - 根据I2S_Standard设置
    if(I2S_Standard == I2S_Standard_Phillips) {
        i2scfgr &= ~SPI_I2SCFGR_I2SSTD;  // Philips标准 (0)
    } else {
        i2scfgr |= SPI_I2SCFGR_I2SSTD;   // 其他标准 (1)
    }
    
    // DATLEN[1:0] - 根据I2S_DataFormat设置
    if(I2S_DataFormat == I2S_DataFormat_16b) {
        i2scfgr &= ~(3 << 4);  // 16bit (00)
    } else if(I2S_DataFormat == I2S_DataFormat_24b) {
        i2scfgr |= (1 << 4);   // 24bit (01)
        i2scfgr &= ~(1 << 5);
    } else if(I2S_DataFormat == I2S_DataFormat_32b) {
        i2scfgr |= (2 << 4);   // 32bit (10)
    } else {
        i2scfgr &= ~(3 << 4);  // 默认16bit
    }
    
    // CKPOL - 根据I2S_Clock_Polarity设置
    if(I2S_Clock_Polarity == I2S_CPOL_Low) {
        i2scfgr &= ~SPI_I2SCFGR_CKPOL;  // 低电平有效 (0)
    } else {
        i2scfgr |= SPI_I2SCFGR_CKPOL;   // 高电平有效 (1)
    }
    
    // 写入配置（不启用I2S2，稍后启用）
    LOGI("[I2S2_DEBUG] Writing I2SCFGR config (without I2SE): 0x%08lX\r\n", i2scfgr);
    SPI2->I2SCFGR = i2scfgr;
    uint32_t i2scfgr_after_write = SPI2->I2SCFGR;
    I2S2_PrintI2SCFGR("After write config", i2scfgr_after_write);
    
    // 参考例程：启用TX DMA请求
    // SPI_I2S_DMACmd(SPI2,SPI_I2S_DMAReq_Tx,ENABLE);
    LOGI("[I2S2_DEBUG] Enabling TX DMA request (CR2.TXDMAEN)...\r\n");
    SPI2->CR2 |= SPI_CR2_TXDMAEN;
    LOGI("[I2S2_DEBUG] CR2 after TXDMAEN: 0x%08lX\r\n", SPI2->CR2);
    
    // 参考例程：启用I2S2
    // I2S_Cmd(SPI2,ENABLE);
    // 注意：根据STM32参考手册，I2SE位只能在I2S禁用时修改
    // 所以先确保I2S2被禁用，然后设置配置，最后启用
    LOGI("[I2S2_DEBUG] Enabling I2S2 (setting I2SE bit)...\r\n");
    I2S2_PrintSR("Before enable");
    
    SPI2->I2SCFGR &= ~SPI_I2SCFGR_I2SE;  // 确保禁用
    uint32_t i2scfgr_ensure_disable = SPI2->I2SCFGR;
    I2S2_PrintI2SCFGR("After ensure disable", i2scfgr_ensure_disable);
    
    SPI2->I2SCFGR = i2scfgr;              // 重新写入配置（确保配置正确）
    uint32_t i2scfgr_reconfig = SPI2->I2SCFGR;
    I2S2_PrintI2SCFGR("After reconfig", i2scfgr_reconfig);
    
    SPI2->I2SCFGR |= SPI_I2SCFGR_I2SE;    // 启用I2S2
    uint32_t i2scfgr_after_enable = SPI2->I2SCFGR;
    I2S2_PrintI2SCFGR("After enable (1st try)", i2scfgr_after_enable);
    I2S2_PrintSR("After enable (1st try)");
    
    // 验证I2S2已启用
    uint32_t verify = SPI2->I2SCFGR;
    if((verify & SPI_I2SCFGR_I2SE) == 0) {
        LOGI("[I2S2_DEBUG] WARNING: I2SE still 0 after first enable attempt! Retrying...\r\n");
        // 如果仍然未启用，再次尝试启用
        SPI2->I2SCFGR |= SPI_I2SCFGR_I2SE;
        verify = SPI2->I2SCFGR;
        I2S2_PrintI2SCFGR("After enable (2nd try)", verify);
        I2S2_PrintSR("After enable (2nd try)");
    }
    
    // 最终验证
    uint32_t final = SPI2->I2SCFGR;
    I2S2_PrintI2SCFGR("FINAL STATE", final);
    I2S2_PrintSR("FINAL STATE");
    LOGI("[I2S2_DEBUG] ===== I2S2_Init() END =====\r\n");
}

/**
 * @brief I2S2ext初始化（完全按照参考例程，不使用HAL库）
 * 修改位置：Core/Src/i2s.c - I2S2ext_Init()函数
 * 修改内容：移除HAL库依赖，直接操作I2S2ext寄存器
 * 
 * 注意：参考例程中I2S2ext_Init使用了I2S_FullDuplexConfig()，这里直接操作寄存器实现
 * 关键：I2S2ext_BASE = APB1PERIPH_BASE + 0x3400 = SPI2_BASE - 0x400
 *      I2S2ext的I2SCFGR寄存器地址 = I2S2ext_BASE + 0x1C
 */
void I2S2ext_Init(uint16_t I2S_Standard, uint16_t I2S_Mode, uint16_t I2S_Clock_Polarity, uint16_t I2S_DataFormat)
{
    LOGI("[I2S2ext_DEBUG] ===== I2S2ext_Init() START =====\r\n");
    LOGI("[I2S2ext_DEBUG] Params: Standard=%d, Mode=%d, CPOL=%d, DataFormat=%d\r\n",
         I2S_Standard, I2S_Mode, I2S_Clock_Polarity, I2S_DataFormat);
    
    // I2S2ext寄存器组基地址：I2S2ext_BASE = SPI2_BASE - 0x400
    // 在STM32F407中，I2S2ext使用独立的寄存器组
    // I2S2ext的I2SCFGR寄存器地址 = I2S2ext_BASE + 0x1C = (SPI2_BASE - 0x400) + 0x1C
    // 直接使用地址计算，避免类型问题
    volatile uint32_t *I2S2ext_I2SCFGR = (volatile uint32_t *)((uint32_t)SPI2 - 0x400 + 0x1C);
    
    // 读取I2S2和I2S2ext的初始状态
    uint32_t i2s2_cfg_before = SPI2->I2SCFGR;
    uint32_t i2s2ext_cfg_before = *I2S2ext_I2SCFGR;
    LOGI("[I2S2ext_DEBUG] Before init: I2S2 I2SCFGR=0x%08lX, I2S2ext I2SCFGR=0x%08lX\r\n",
         i2s2_cfg_before, i2s2ext_cfg_before);
    I2S2_PrintI2SCFGR("I2S2 before I2S2ext_Init", i2s2_cfg_before);
    
    // 调试：验证地址计算
    // SPI2_BASE = 0x40003800, I2S2ext_BASE = 0x40003400
    // I2S2ext_I2SCFGR = 0x4000341C, SPI2_I2SCFGR = 0x4000381C
    // 确保地址不同（注释掉调试日志，避免编译错误）
    // 地址计算：I2S2ext_I2SCFGR = SPI2_BASE - 0x400 + 0x1C
    
    // 参考例程：I2S2ext_InitStructure.I2S_Mode=I2S_Mode^(1<<8);
    // 这个异或操作是为了区分I2S2和I2S2ext，但我们直接操作寄存器，不需要这个操作
    
    // 先禁用I2S2ext
    LOGI("[I2S2ext_DEBUG] Disabling I2S2ext (clearing I2SE bit @bit10)...\r\n");
    *I2S2ext_I2SCFGR &= ~(1 << 10);  // 清除I2SE位（bit10，不是bit0！）
    uint32_t i2s2ext_after_disable = *I2S2ext_I2SCFGR;
    LOGI("[I2S2ext_DEBUG] I2S2ext after disable: I2SCFGR=0x%08lX (I2SE=%d @bit10)\r\n",
         i2s2ext_after_disable, (i2s2ext_after_disable >> 10) & 1);
    
    // 配置I2SCFGR寄存器（与I2S2类似，但模式不同）
    uint32_t i2scfgr_val = 0;
    
    // I2SMOD = 1 (I2S模式)
    i2scfgr_val |= (1 << 11);
    
    // I2SCFG[1:0] - 根据I2S_Mode设置（参考例程传入的是SlaveRx）
    if(I2S_Mode == I2S_Mode_MasterTx) {
        i2scfgr_val |= (2 << 8);  // Master TX (10)
    } else if(I2S_Mode == I2S_Mode_SlaveTx) {
        i2scfgr_val |= (0 << 8);  // Slave TX (00)
    } else if(I2S_Mode == I2S_Mode_MasterRx) {
        i2scfgr_val |= (3 << 8);  // Master RX (11)
    } else if(I2S_Mode == I2S_Mode_SlaveRx) {
        i2scfgr_val |= (1 << 8);  // Slave RX (01) <- 录音时使用这个
    } else {
        i2scfgr_val |= (1 << 8);  // 默认Slave RX
    }
    
    // I2SSTD - 根据I2S_Standard设置
    if(I2S_Standard == I2S_Standard_Phillips) {
        i2scfgr_val &= ~(1 << 10);  // Philips标准 (0)
    } else {
        i2scfgr_val |= (1 << 10);   // 其他标准 (1)
    }
    
    // DATLEN[1:0] - 根据I2S_DataFormat设置
    if(I2S_DataFormat == I2S_DataFormat_16b) {
        i2scfgr_val &= ~(3 << 4);  // 16bit (00)
    } else if(I2S_DataFormat == I2S_DataFormat_24b) {
        i2scfgr_val |= (1 << 4);   // 24bit (01)
        i2scfgr_val &= ~(1 << 5);
    } else if(I2S_DataFormat == I2S_DataFormat_32b) {
        i2scfgr_val |= (2 << 4);   // 32bit (10)
    } else {
        i2scfgr_val &= ~(3 << 4);  // 默认16bit
    }
    
    // CKPOL - 根据I2S_Clock_Polarity设置
    if(I2S_Clock_Polarity == I2S_CPOL_Low) {
        i2scfgr_val &= ~(1 << 1);  // 低电平有效 (0)
    } else {
        i2scfgr_val |= (1 << 1);   // 高电平有效 (1)
    }
    
    // 写入配置（不启用I2S2ext，稍后启用）
    // 重要：确保不会影响I2S2的配置
    LOGI("[I2S2ext_DEBUG] Writing I2S2ext I2SCFGR config (without I2SE): 0x%08lX\r\n", i2scfgr_val);
    *I2S2ext_I2SCFGR = i2scfgr_val;
    uint32_t i2s2ext_after_write = *I2S2ext_I2SCFGR;
    LOGI("[I2S2ext_DEBUG] I2S2ext after write config: I2SCFGR=0x%08lX (I2SE=%d, I2SCFG=%d)\r\n",
         i2s2ext_after_write, (i2s2ext_after_write >> 10) & 1, (i2s2ext_after_write >> 8) & 3);
    
    // 检查I2S2是否被影响
    uint32_t i2s2_check = SPI2->I2SCFGR;
    if(i2s2_check != i2s2_cfg_before) {
        LOGI("[I2S2ext_DEBUG] WARNING: I2S2 I2SCFGR changed from 0x%08lX to 0x%08lX!\r\n",
             i2s2_cfg_before, i2s2_check);
        I2S2_PrintI2SCFGR("I2S2 after I2S2ext write", i2s2_check);
    }
    
    // 参考例程：在I2S2ext_Init()中启用RX DMA请求
    // SPI_I2S_DMACmd(I2S2ext, SPI_I2S_DMAReq_Rx, ENABLE);
    // I2S2ext使用SPI2的CR2寄存器（共享）
    LOGI("[I2S2ext_DEBUG] Enabling RX DMA request (CR2.RXDMAEN)...\r\n");
    SPI2->CR2 |= SPI_CR2_RXDMAEN;
    LOGI("[I2S2ext_DEBUG] CR2 after RXDMAEN: 0x%08lX\r\n", SPI2->CR2);
    
    // 参考例程：在I2S2ext_Init()中启用I2S2ext
    // I2S_Cmd(I2S2ext, ENABLE);
    LOGI("[I2S2ext_DEBUG] Enabling I2S2ext (setting I2SE bit @bit10)...\r\n");
    *I2S2ext_I2SCFGR |= (1 << 10);  // I2SE位在bit10
    uint32_t i2s2ext_after_enable = *I2S2ext_I2SCFGR;
    LOGI("[I2S2ext_DEBUG] I2S2ext after enable: I2SCFGR=0x%08lX (I2SE=%d, I2SCFG=%d)\r\n",
         i2s2ext_after_enable, (i2s2ext_after_enable >> 10) & 1, (i2s2ext_after_enable >> 8) & 3);
    
    // 调试：配置值已在recorder_audio.c中输出
    
    // 重要：确保I2S2的配置没有被I2S2ext_Init影响
    // 检查I2S2的I2SCFGR寄存器
    uint32_t i2s2_cfg_after = SPI2->I2SCFGR;
    LOGI("[I2S2ext_DEBUG] Checking I2S2 after I2S2ext init...\r\n");
    I2S2_PrintI2SCFGR("I2S2 after I2S2ext init", i2s2_cfg_after);
    
    uint8_t i2s2_need_correct = 0;
    
    // 检查I2SCFG模式
    if(((i2s2_cfg_after >> 8) & 3) != 2) {  // 如果不是Master TX模式
        LOGI("[I2S2ext_DEBUG] ERROR: I2S2 I2SCFG is not Master TX! (is %d)\r\n", (i2s2_cfg_after >> 8) & 3);
        i2s2_need_correct = 1;
    }
    
    // 检查I2SE使能位（关键！）
    if((i2s2_cfg_after & SPI_I2SCFGR_I2SE) == 0) {  // 如果I2S2被禁用
        LOGI("[I2S2ext_DEBUG] ERROR: I2S2 I2SE is disabled!\r\n");
        i2s2_need_correct = 1;
    }
    
    if(i2s2_need_correct) {
        LOGI("[I2S2ext_DEBUG] Correcting I2S2 configuration...\r\n");
        // 禁用I2S2（如果已启用）
        SPI2->I2SCFGR &= ~SPI_I2SCFGR_I2SE;
        uint32_t i2s2_after_disable = SPI2->I2SCFGR;
        I2S2_PrintI2SCFGR("I2S2 after disable (correction)", i2s2_after_disable);
        
        // 修正I2SCFG为Master TX (10)
        i2s2_cfg_after &= ~(3 << 8);
        i2s2_cfg_after |= (2 << 8);   // Master TX
        i2s2_cfg_after |= (1 << 11);  // I2SMOD
        i2s2_cfg_after &= ~SPI_I2SCFGR_I2SSTD;  // Philips标准
        SPI2->I2SCFGR = i2s2_cfg_after;
        uint32_t i2s2_after_reconfig = SPI2->I2SCFGR;
        I2S2_PrintI2SCFGR("I2S2 after reconfig (correction)", i2s2_after_reconfig);
        
        // 重新启用I2S2（关键：必须启用才能输出时钟）
        SPI2->I2SCFGR |= SPI_I2SCFGR_I2SE;
        uint32_t i2s2_after_enable = SPI2->I2SCFGR;
        I2S2_PrintI2SCFGR("I2S2 after enable (correction)", i2s2_after_enable);
        
        // 验证I2S2已启用
        uint32_t verify = SPI2->I2SCFGR;
        if((verify & SPI_I2SCFGR_I2SE) == 0) {
            LOGI("[I2S2ext_DEBUG] WARNING: I2S2 I2SE still 0 after correction! Retrying...\r\n");
            // 如果仍然未启用，再次尝试启用
            SPI2->I2SCFGR |= SPI_I2SCFGR_I2SE;
            verify = SPI2->I2SCFGR;
            I2S2_PrintI2SCFGR("I2S2 after 2nd enable (correction)", verify);
    }
    }
    
    // 最终状态
    uint32_t i2s2_final = SPI2->I2SCFGR;
    uint32_t i2s2ext_final = *I2S2ext_I2SCFGR;
    LOGI("[I2S2ext_DEBUG] FINAL: I2S2 I2SCFGR=0x%08lX, I2S2ext I2SCFGR=0x%08lX\r\n",
         i2s2_final, i2s2ext_final);
    I2S2_PrintI2SCFGR("I2S2 FINAL", i2s2_final);
    LOGI("[I2S2ext_DEBUG] ===== I2S2ext_Init() END =====\r\n");
}

/**
 * @brief 设置I2S采样率
 */
uint8_t I2S2_SampleRate_Set(uint32_t samplerate)
{
    uint8_t i = 0;
    uint32_t tempreg = 0;
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    samplerate /= 10;

    for(i = 0; i < (sizeof(I2S_PSC_TBL) / 10); i++) {
        if(samplerate == I2S_PSC_TBL[i][0])
            break;
    }

    if(i == (sizeof(I2S_PSC_TBL) / 10))
        return 1;

    // 先禁用I2S（参考例程的做法，避免配置时I2S正在运行）
    __HAL_I2S_DISABLE(&hi2s2);
    HAL_Delay(1);  // 短暂延时，确保I2S完全停止

    // 先禁用PLLI2S（参考例程的做法，确保配置生效）
    RCC->CR &= ~RCC_CR_PLLI2SON;
    while((RCC->CR & RCC_CR_PLLI2SRDY) != 0);  // 等待PLLI2S关闭

    // 配置PLLI2S
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2S;
    PeriphClkInitStruct.PLLI2S.PLLI2SN = I2S_PSC_TBL[i][1];
    PeriphClkInitStruct.PLLI2S.PLLI2SR = I2S_PSC_TBL[i][2];

    if(HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
        return 1;
    }

    // 使能PLLI2S并等待锁定（参考例程的做法）
    RCC->CR |= RCC_CR_PLLI2SON;
    while((RCC->CR & RCC_CR_PLLI2SRDY) == 0);  // 等待PLLI2S锁定

    // 配置I2SPR寄存器
    tempreg = I2S_PSC_TBL[i][3] << 0;  // I2SDIV
    tempreg |= I2S_PSC_TBL[i][4] << 8;  // ODD
    tempreg |= 1 << 9;                  // MCKOE (使能MCLK输出)
    SPI2->I2SPR = tempreg;

    return 0;
}

/**
 * @brief I2S2 TX DMA初始化(双缓冲模式)
 */
void I2S2_TX_DMA_Init(uint8_t* buf0, uint8_t *buf1, uint16_t num)
{
    DMA1_Stream4->CR = 0;
    while(DMA1_Stream4->CR & DMA_SxCR_EN);

    DMA1_Stream4->PAR = (uint32_t)&(SPI2->DR);
    DMA1_Stream4->M0AR = (uint32_t)buf0;
    DMA1_Stream4->M1AR = (uint32_t)buf1;
    DMA1_Stream4->NDTR = num;

    DMA1_Stream4->CR = DMA_CHANNEL_0 |
                       DMA_PRIORITY_HIGH |
                       DMA_MDATAALIGN_HALFWORD |
                       DMA_PDATAALIGN_HALFWORD |
                       DMA_MINC_ENABLE |
                       DMA_CIRCULAR |
                       DMA_SxCR_DIR_0 |
                       DMA_SxCR_DBM |
                       DMA_SxCR_TCIE;

    HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
}

/**
 * @brief 开始I2S播放
 * 参考例程：I2S_Play_Start()只是启用DMA，I2S2在Init时已启用
 */
void I2S_Play_Start(void)
{
    LOGI("[I2S_Play_Start_DEBUG] ===== I2S_Play_Start() START =====\r\n");
    
    // 读取初始状态
    uint32_t i2s2_cfg_initial = SPI2->I2SCFGR;
    I2S2_PrintI2SCFGR("I2S_Play_Start initial", i2s2_cfg_initial);
    I2S2_PrintSR("I2S_Play_Start initial");
    
    // 参考例程：I2S_Play_Start()只是启用DMA
    // DMA_Cmd(DMA1_Stream4,ENABLE);
    
    // 确保DMA被禁用（如果之前被启用）
    LOGI("[I2S_Play_Start_DEBUG] Disabling DMA...\r\n");
    DMA1_Stream4->CR &= ~DMA_SxCR_EN;
    while(DMA1_Stream4->CR & DMA_SxCR_EN);  // 等待DMA完全停止
    LOGI("[I2S_Play_Start_DEBUG] DMA disabled, CR=0x%08lX\r\n", DMA1_Stream4->CR);
    
    // 确保I2S2配置为Master TX模式并启用（关键：必须启用I2S2才能输出时钟）
    uint32_t i2s2_cfg = SPI2->I2SCFGR;
    uint8_t need_correct = 0;
    
    // 检查I2SCFG模式
    if(((i2s2_cfg >> 8) & 3) != 2) {  // 如果不是Master TX模式
        LOGI("[I2S_Play_Start_DEBUG] ERROR: I2S2 I2SCFG is not Master TX! (is %d)\r\n", (i2s2_cfg >> 8) & 3);
        need_correct = 1;
    }
    
    // 检查I2SE使能位（关键！）
    if((i2s2_cfg & SPI_I2SCFGR_I2SE) == 0) {  // 如果I2S2被禁用
        LOGI("[I2S_Play_Start_DEBUG] ERROR: I2S2 I2SE is disabled!\r\n");
        need_correct = 1;
    }
    
    if(need_correct) {
        LOGI("[I2S_Play_Start_DEBUG] Correcting I2S2 configuration...\r\n");
        // 禁用I2S2（如果已启用）
        SPI2->I2SCFGR &= ~SPI_I2SCFGR_I2SE;
        uint32_t i2s2_after_disable = SPI2->I2SCFGR;
        I2S2_PrintI2SCFGR("I2S2 after disable (correction)", i2s2_after_disable);
        I2S2_PrintSR("I2S2 after disable (correction)");
    
        // 修正I2SCFG为Master TX (10)
        i2s2_cfg &= ~(3 << 8);
        i2s2_cfg |= (2 << 8);   // Master TX
        i2s2_cfg |= (1 << 11);  // I2SMOD
        i2s2_cfg &= ~SPI_I2SCFGR_I2SSTD;  // Philips标准
        SPI2->I2SCFGR = i2s2_cfg;
        uint32_t i2s2_after_reconfig = SPI2->I2SCFGR;
        I2S2_PrintI2SCFGR("I2S2 after reconfig (correction)", i2s2_after_reconfig);
        
        // 重新启用I2S2（关键：必须启用才能输出时钟）
        SPI2->I2SCFGR |= SPI_I2SCFGR_I2SE;
        uint32_t i2s2_after_enable = SPI2->I2SCFGR;
        I2S2_PrintI2SCFGR("I2S2 after enable (correction)", i2s2_after_enable);
        I2S2_PrintSR("I2S2 after enable (correction)");
        
        // 验证I2S2已启用
        uint32_t verify_cfg = SPI2->I2SCFGR;
        if((verify_cfg & SPI_I2SCFGR_I2SE) == 0) {
            LOGI("[I2S_Play_Start_DEBUG] WARNING: I2S2 I2SE still 0 after correction! Retrying...\r\n");
            // 如果仍然未启用，强制启用
            SPI2->I2SCFGR |= SPI_I2SCFGR_I2SE;
            verify_cfg = SPI2->I2SCFGR;
            I2S2_PrintI2SCFGR("I2S2 after 2nd enable (correction)", verify_cfg);
        }
    }
    
    // 最终验证：确保I2S2已启用（关键！）
    uint32_t final_cfg = SPI2->I2SCFGR;
    if((final_cfg & SPI_I2SCFGR_I2SE) == 0) {
        LOGI("[I2S_Play_Start_DEBUG] WARNING: I2S2 I2SE still 0 in final check! Force enabling...\r\n");
        // 强制启用I2S2
    SPI2->I2SCFGR |= SPI_I2SCFGR_I2SE;
        final_cfg = SPI2->I2SCFGR;
        I2S2_PrintI2SCFGR("I2S2 after force enable", final_cfg);
    }
    
    I2S2_PrintI2SCFGR("I2S2 before DMA enable", final_cfg);
    I2S2_PrintSR("I2S2 before DMA enable");
    
    // 启用DMA（I2S2现在已启用）
    LOGI("[I2S_Play_Start_DEBUG] Enabling DMA...\r\n");
    DMA1_Stream4->CR |= DMA_SxCR_EN;
    LOGI("[I2S_Play_Start_DEBUG] DMA enabled, CR=0x%08lX\r\n", DMA1_Stream4->CR);
    
    // 最终状态
    uint32_t final_after_dma = SPI2->I2SCFGR;
    I2S2_PrintI2SCFGR("I2S2 FINAL (after DMA enable)", final_after_dma);
    I2S2_PrintSR("I2S2 FINAL (after DMA enable)");
    LOGI("[I2S_Play_Start_DEBUG] ===== I2S_Play_Start() END =====\r\n");
}

/**
 * @brief 停止I2S播放
 */
void I2S_Play_Stop(void)
{
    __HAL_I2S_DISABLE(&hi2s2);
    __HAL_DMA_DISABLE(&hdma_spi2_tx);
}

/**
 * @brief I2S2ext RX DMA初始化(双缓冲模式)
 * 参考例程：I2S2ext_RX_DMA_Init()
 */
void I2S2ext_RX_DMA_Init(uint8_t* buf0, uint8_t *buf1, uint16_t num)
{
    extern DMA_HandleTypeDef hdma_i2s2_ext_rx;
    
    // 停止DMA
    DMA1_Stream3->CR = 0;
    while(DMA1_Stream3->CR & DMA_SxCR_EN);
    
    // 清除中断标志
    DMA1_Stream3->FCR = 0;
    DMA1_Stream3->NDTR = 0;
    
    // 参考例程：DMA_PeripheralBaseAddr = (u32)&I2S2ext->DR
    // 在STM32F407中，I2S2ext_BASE = APB1PERIPH_BASE + 0x3400
    // SPI2_BASE = APB1PERIPH_BASE + 0x3800
    // 所以 I2S2ext_BASE = SPI2_BASE - 0x400
    // I2S2ext的DR寄存器地址 = I2S2ext_BASE + 0x0C = (SPI2_BASE - 0x400) + 0x0C
    // 注意：I2S2ext和SPI2共享DR寄存器，所以实际上&I2S2ext->DR == &SPI2->DR
    // 为了与参考例程保持一致，我们直接使用SPI2->DR（因为它们是共享的）
    // 配置DMA寄存器
    DMA1_Stream3->PAR = (uint32_t)&(SPI2->DR);  // 外设地址（I2S2ext和SPI2共享DR寄存器，参考例程使用&I2S2ext->DR）
    DMA1_Stream3->M0AR = (uint32_t)buf0;        // 内存地址0
    DMA1_Stream3->M1AR = (uint32_t)buf1;        // 内存地址1
    DMA1_Stream3->NDTR = num;                   // 传输数量
    LOGI("[I2S2ext_DMA_DEBUG] RX DMA init: PAR=0x%08lX, M0AR=0x%08lX, M1AR=0x%08lX, NDTR=%u\r\n",
         DMA1_Stream3->PAR, DMA1_Stream3->M0AR, DMA1_Stream3->M1AR, DMA1_Stream3->NDTR);
    
    // 配置DMA控制寄存器：双缓冲模式
    DMA1_Stream3->CR = DMA_CHANNEL_3 |          // Channel 3
                       DMA_PRIORITY_HIGH |       // 高优先级
                       DMA_MDATAALIGN_HALFWORD | // 内存数据对齐：16bit
                       DMA_PDATAALIGN_HALFWORD | // 外设数据对齐：16bit
                       DMA_MINC_ENABLE |         // 内存地址递增
                       DMA_PINC_DISABLE |        // 外设地址不递增
                       DMA_CIRCULAR |            // 循环模式
                       DMA_SxCR_DIR_1 |         // 外设到内存
                       DMA_SxCR_DBM |            // 双缓冲模式
                       DMA_SxCR_TCIE;            // 传输完成中断
    
    // 配置双缓冲：初始使用M0AR
    DMA1_Stream3->CR &= ~DMA_SxCR_CT;  // 清除CT位，使用M0AR
    
    // 注意：DMA请求已在I2S2ext_Init()中启用，这里不需要重复启用
    // 但如果I2S2ext_Init()在DMA初始化之前调用，这里需要确保DMA请求已启用
    // SPI2->CR2 |= SPI_CR2_RXDMAEN;  // 已在I2S2ext_Init()中启用
    
    // 启用中断
    HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
    
    LOGI("[I2S2ext_DMA_DEBUG] RX DMA CR=0x%08lX, FCR=0x%08lX\r\n", DMA1_Stream3->CR, DMA1_Stream3->FCR);
}

/**
 * @brief 启动I2S2ext接收
 * 参考例程：I2S_Rec_Start() - 只是启用DMA，I2S2ext在Init时已启用
 */
void I2S_Rec_Start(void)
{
    LOGI("[I2S_Rec_Start_DEBUG] ===== I2S_Rec_Start() START =====\r\n");
    
    // 读取I2S2和I2S2ext的状态
    uint32_t i2s2_cfg = SPI2->I2SCFGR;
    volatile uint32_t *I2S2ext_I2SCFGR = (volatile uint32_t *)((uint32_t)SPI2 - 0x400 + 0x1C);
    uint32_t i2s2ext_cfg = *I2S2ext_I2SCFGR;
    
    I2S2_PrintI2SCFGR("I2S2 before I2S_Rec_Start", i2s2_cfg);
    LOGI("[I2S_Rec_Start_DEBUG] I2S2ext before start: I2SCFGR=0x%08lX (I2SE=%d @bit10, I2SCFG=%d)\r\n",
         i2s2ext_cfg, (i2s2ext_cfg >> 10) & 1, (i2s2ext_cfg >> 8) & 3);
    
    // 参考例程：I2S_Rec_Start()只是启用DMA
    // DMA_Cmd(DMA1_Stream3,ENABLE);
    
    // 确保DMA被禁用（如果之前被启用）
    LOGI("[I2S_Rec_Start_DEBUG] Disabling DMA3...\r\n");
    DMA1_Stream3->CR &= ~DMA_SxCR_EN;
    while(DMA1_Stream3->CR & DMA_SxCR_EN);  // 等待DMA完全停止
    LOGI("[I2S_Rec_Start_DEBUG] DMA3 disabled, CR=0x%08lX\r\n", DMA1_Stream3->CR);
    
    // 启用DMA（I2S2ext在Init时已启用，不需要再次启用）
    LOGI("[I2S_Rec_Start_DEBUG] Enabling I2S2ext RX DMA...\r\n");
    DMA1_Stream3->CR |= DMA_SxCR_EN;
    LOGI("[I2S_Rec_Start_DEBUG] DMA3 enabled, CR=0x%08lX\r\n", DMA1_Stream3->CR);
    
    // 确保I2S2ext被启用（关键！）
    // 检查I2S2ext的I2SE位（bit10）
    if((*I2S2ext_I2SCFGR & (1 << 10)) == 0) {
        LOGI("[I2S_Rec_Start_DEBUG] WARNING: I2S2ext I2SE is disabled! Enabling...\r\n");
        *I2S2ext_I2SCFGR |= (1 << 10);  // 启用I2S2ext（设置bit10）
    }
    
    // 读取启动后的状态
    uint32_t i2s2_cfg_after = SPI2->I2SCFGR;
    uint32_t i2s2ext_cfg_after = *I2S2ext_I2SCFGR;
    I2S2_PrintI2SCFGR("I2S2 after I2S_Rec_Start", i2s2_cfg_after);
    LOGI("[I2S_Rec_Start_DEBUG] I2S2ext after start: I2SCFGR=0x%08lX (I2SE=%d @bit10, I2SCFG=%d)\r\n",
         i2s2ext_cfg_after, (i2s2ext_cfg_after >> 10) & 1, (i2s2ext_cfg_after >> 8) & 3);
    
    // 验证I2S2ext已启用
    if((i2s2ext_cfg_after & (1 << 10)) == 0) {
        LOGI("[I2S_Rec_Start_DEBUG] ERROR: I2S2ext I2SE still disabled after enable attempt!\r\n");
    } else {
        LOGI("[I2S_Rec_Start_DEBUG] I2S2ext I2SE confirmed enabled (bit10=1)\r\n");
    }
    
    // 检查I2S2是否被影响
    if(i2s2_cfg_after != i2s2_cfg) {
        LOGI("[I2S_Rec_Start_DEBUG] WARNING: I2S2 I2SCFGR changed from 0x%08lX to 0x%08lX!\r\n",
             i2s2_cfg, i2s2_cfg_after);
    }
    
    LOGI("[I2S_Rec_Start_DEBUG] ===== I2S_Rec_Start() END =====\r\n");
}

/**
 * @brief 停止I2S2ext接收
 */
void I2S_Rec_Stop(void)
{
    // 禁用I2S2ext的DMA请求
    SPI2->CR2 &= ~SPI_CR2_RXDMAEN;
    
    // 停止DMA
    DMA1_Stream3->CR &= ~DMA_SxCR_EN;
    while(DMA1_Stream3->CR & DMA_SxCR_EN);
    
    // 禁用I2S2ext（如果需要）
    // 注意：I2S2ext的I2SCFGR寄存器访问需要验证
}

/* USER CODE END 1 */
