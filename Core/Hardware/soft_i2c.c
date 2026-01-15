/**
  ******************************************************************************
  * @file    soft_i2c.c
  * @brief   软件I2C实现（用于WM8978）
  ******************************************************************************
  */

#include "soft_i2c.h"

// 定义引脚（使用未占用的GPIO）
#define SCL_PIN  GPIO_PIN_8   // PA8
#define SCL_PORT GPIOA
#define SDA_PIN  GPIO_PIN_9   // PC9
#define SDA_PORT GPIOC

// 延时函数
static void I2C_Delay(void)
{
    volatile uint32_t i = 50;  // 增大延时
    while(i--);
}

// SDA输出模式
static void SDA_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SDA_PORT, &GPIO_InitStruct);
}

// SDA输入模式
static void SDA_IN(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(SDA_PORT, &GPIO_InitStruct);
}

// 读SDA
#define READ_SDA()  HAL_GPIO_ReadPin(SDA_PORT, SDA_PIN)

// 写SCL
#define SCL_H()     HAL_GPIO_WritePin(SCL_PORT, SCL_PIN, GPIO_PIN_SET)
#define SCL_L()     HAL_GPIO_WritePin(SCL_PORT, SCL_PIN, GPIO_PIN_RESET)

// 写SDA
#define SDA_H()     HAL_GPIO_WritePin(SDA_PORT, SDA_PIN, GPIO_PIN_SET)
#define SDA_L()     HAL_GPIO_WritePin(SDA_PORT, SDA_PIN, GPIO_PIN_RESET)

/**
 * @brief 软件I2C初始化
 */
void Soft_I2C_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // SCL
    GPIO_InitStruct.Pin = SCL_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SCL_PORT, &GPIO_InitStruct);
    
    // SDA
    GPIO_InitStruct.Pin = SDA_PIN;
    HAL_GPIO_Init(SDA_PORT, &GPIO_InitStruct);
    
    SCL_H();
    SDA_H();
}

/**
 * @brief I2C起始信号
 */
static void I2C_Start(void)
{
    SDA_OUT();
    SDA_H();
    SCL_H();
    I2C_Delay();
    SDA_L();
    I2C_Delay();
    SCL_L();
}

/**
 * @brief I2C停止信号
 */
static void I2C_Stop(void)
{
    SDA_OUT();
    SCL_L();
    SDA_L();
    I2C_Delay();
    SCL_H();
    I2C_Delay();
    SDA_H();
}

/**
 * @brief 等待ACK
 * @retval 0=ACK, 1=NACK
 */
static uint8_t I2C_Wait_Ack(void)
{
    uint8_t timeout = 0;
    
    SDA_IN();
    SDA_H();
    I2C_Delay();
    SCL_H();
    I2C_Delay();
    
    while(READ_SDA())
    {
        timeout++;
        if(timeout > 250)
        {
            I2C_Stop();
            return 1;
        }
    }
    
    SCL_L();
    return 0;
}

/**
 * @brief 发送ACK
 */
static void I2C_Ack(void)
{
    SCL_L();
    SDA_OUT();
    SDA_L();
    I2C_Delay();
    SCL_H();
    I2C_Delay();
    SCL_L();
}

/**
 * @brief 发送NACK
 */
static void I2C_NAck(void)
{
    SCL_L();
    SDA_OUT();
    SDA_H();
    I2C_Delay();
    SCL_H();
    I2C_Delay();
    SCL_L();
}

/**
 * @brief 发送一个字节
 */
static void I2C_Send_Byte(uint8_t data)
{
    uint8_t i;
    
    SDA_OUT();
    SCL_L();
    
    for(i = 0; i < 8; i++)
    {
        if(data & 0x80)
            SDA_H();
        else
            SDA_L();
        
        data <<= 1;
        I2C_Delay();
        SCL_H();
        I2C_Delay();
        SCL_L();
    }
}

/**
 * @brief 接收一个字节
 */
static uint8_t I2C_Read_Byte(uint8_t ack)
{
    uint8_t i, data = 0;
    
    SDA_IN();
    
    for(i = 0; i < 8; i++)
    {
        SCL_L();
        I2C_Delay();
        SCL_H();
        data <<= 1;
        if(READ_SDA())
            data++;
        I2C_Delay();
    }
    
    if(ack)
        I2C_Ack();
    else
        I2C_NAck();
    
    return data;
}

/**
 * @brief 软件I2C写数据
 */
uint8_t Soft_I2C_Write(uint8_t addr, uint8_t *data, uint16_t len)
{
    uint16_t i;

    I2C_Start();
    I2C_Send_Byte(addr << 1);

    if(I2C_Wait_Ack())
    {
        I2C_Stop();
        return 1;
    }

    for(i = 0; i < len; i++)
    {
        I2C_Send_Byte(data[i]);
        if(I2C_Wait_Ack())
        {
            I2C_Stop();
            return 1;
        }
    }

    I2C_Stop();
    return 0;
}

/**
 * @brief 软件I2C读数据
 */
uint8_t Soft_I2C_Read(uint8_t addr, uint8_t *data, uint16_t len)
{
    uint16_t i;
    
    I2C_Start();
    I2C_Send_Byte((addr << 1) | 1);
    
    if(I2C_Wait_Ack())
        return 1;
    
    for(i = 0; i < len; i++)
    {
        data[i] = I2C_Read_Byte(i < len - 1);
    }
    
    I2C_Stop();
    return 0;
}
