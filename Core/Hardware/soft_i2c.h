/**
  ******************************************************************************
  * @file    soft_i2c.h
  * @brief   软件I2C头文件
  ******************************************************************************
  */

#ifndef __SOFT_I2C_H
#define __SOFT_I2C_H

#include "main.h"

void Soft_I2C_Init(void);
uint8_t Soft_I2C_Write(uint8_t addr, uint8_t *data, uint16_t len);
uint8_t Soft_I2C_Read(uint8_t addr, uint8_t *data, uint16_t len);

#endif
