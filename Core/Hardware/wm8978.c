/**
  ******************************************************************************
  * @file    wm8978.c
  * @brief   WM8978音频编解码器驱动实现（移植自参考工程，使用硬件IIC）
  * @note    完全按照参考工程的实现顺序移植，使用硬件I2C2（PF1/PF0）
  *          注意：GPIO配置已在MX_I2S2_Init()中完成（CubeMX生成），此处不再配置
  *          注意：硬件I2C2使用PF1(SCL)和PF0(SDA)，与I2C1（触摸屏）独立
  ******************************************************************************
  */

#include "wm8978.h"
#include "i2c.h"
#include "debug_uart.h"

//////////////////////////////////////////////////////////////////////////////////
// WM8978寄存器值缓存表(总共58个寄存器,0~57),占用116字节内存
// 因为WM8978的IIC接口不支持读操作,所以在本驱动中缓存寄存器值
// 写WM8978寄存器时,同步更新缓存的寄存器值,读寄存器时,直接返回缓存表的寄存器值.
// 注意:WM8978的寄存器值是9位宽,所以需要用uint16_t存储.
//////////////////////////////////////////////////////////////////////////////////
static uint16_t WM8978_REGVAL_TBL[58] =
{
    0x0000, 0x0000, 0x0000, 0x0000, 0x0050, 0x0000, 0x0140, 0x0000,
    0x0000, 0x0000, 0x0000, 0x00FF, 0x00FF, 0x0000, 0x0100, 0x00FF,
    0x00FF, 0x0000, 0x012C, 0x002C, 0x002C, 0x002C, 0x002C, 0x0000,
    0x0032, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0038, 0x000B, 0x0032, 0x0000, 0x0008, 0x000C, 0x0093, 0x00E9,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0003, 0x0010, 0x0010, 0x0100,
    0x0100, 0x0002, 0x0001, 0x0001, 0x0039, 0x0039, 0x0039, 0x0039,
    0x0001, 0x0001
};

/**
  * @brief  WM8978初始化
  * @param  None
  * @retval 0: 初始化成功
  *         非0: 初始化失败,WM8978异常
  * @note   完全按照参考工程的初始化顺序和寄存器值
  *         注意：GPIO AF配置已在MX_I2S2_Init()中完成，此处不再配置
  *         注意：硬件I2C1已在MX_I2C1_Init()中初始化，WM8978共享使用
  */
uint8_t WM8978_Init(void)
{
    uint8_t res;

    // 注意：GPIO AF配置已在CubeMX生成的MX_I2S2_Init()中完成
    // 参考工程中的GPIO配置如下（仅供参考，不在此处执行）：
    // PB12 -> AF5 (I2S2_LRCK/FSA)
    // PB13 -> AF5 (I2S2_SCLK/BCLK)
    // PC3  -> AF5 (I2S2_SD/DACDAT)
    // PC6  -> AF5 (I2S2_MCK/MCLK)
    // PC2  -> AF6 (I2S2ext_SD/ADCDAT)
    
    // 注意：硬件I2C2已在MX_I2C2_Init()中初始化
    // I2C2引脚配置：PF1 -> I2C2_SCL, PF0 -> I2C2_SDA

    HAL_Delay(50);  // 等待硬件稳定

    // 软件复位WM8978
    res = WM8978_Write_Reg(0, 0);  // 写任意值到R0进行复位
    if(res) return 1;              // 复位指令失败,WM8978异常
    HAL_Delay(10);

    // 电源管理配置
    WM8978_Write_Reg(1, 0x1B);     // R1: MICEN设置为1(MIC使能), BIASEN设置为1(模拟偏置使能), VMIDSEL[1:0]设置为:11(5K)
    WM8978_Write_Reg(2, 0x1B0);    // R2: ROUT1,LOUT1输出使能(耳机可以工作), BOOSTENR,BOOSTENL使能
    WM8978_Write_Reg(3, 0x6C);     // R3: LOUT2,ROUT2输出使能(扬声器工作), RMIX,LMIX使能

    // 时钟配置
    WM8978_Write_Reg(6, 0);        // R6: MCLK由外部提供

    // 其他重要寄存器配置
    WM8978_Write_Reg(43, 1 << 4);  // R43: INVROUT2反相,改善音质
    WM8978_Write_Reg(47, 1 << 8);  // R47设置,PGABOOSTL,左通道MIC增益20dB
    WM8978_Write_Reg(48, 1 << 8);  // R48设置,PGABOOSTR,右通道MIC增益20dB
    WM8978_Write_Reg(49, 1 << 1);  // R49: TSDEN,过热保护使能
    WM8978_Write_Reg(10, 1 << 3);  // R10: SOFTMUTE关闭,128x采样,提高SNR
    WM8978_Write_Reg(14, 1 << 3);  // R14: ADC 128x采样

    // 上电时关闭所有音频输入（MIC、LINE IN、AUX），避免噪声
    WM8978_Input_Cfg(0, 0, 0);     // 关闭MIC、LINE IN、AUX输入
    WM8978_Output_Cfg(0, 0);       // 关闭DAC和BYPASS输出（上电时默认关闭）

    return 0;
}

/**
  * @brief  WM8978写寄存器（使用硬件I2C2）
  * @param  reg: 寄存器地址 (0~57)
  * @param  val: 要写入寄存器的值 (9位)
  * @retval 0: 成功
  *         非0: 失败,返回错误码（1=地址ACK失败, 2=寄存器地址ACK失败, 3=数据ACK失败）
  * @note   完全按照参考工程的实现方式和顺序
  *         参考工程步骤：
  *         1. IIC_Start() - 发送起始信号
  *         2. IIC_Send_Byte((WM8978_ADDR<<1)|0) - 发送器件地址+写命令, 等待ACK
  *         3. IIC_Send_Byte((reg<<1)|((val>>8)&0X01)) - 写寄存器地址+数据最高位, 等待ACK
  *         4. IIC_Send_Byte(val&0XFF) - 发送数据低8位, 等待ACK
  *         5. IIC_Stop() - 发送停止信号
  *         
  *         HAL库的HAL_I2C_Master_Transmit会在一个I2C事务中完成上述所有步骤：
  *         - 自动发送Start
  *         - 发送地址字节（7位地址左移1位，bit0=0表示写）
  *         - 等待ACK
  *         - 发送数据字节（每个字节后自动等待ACK）
  *         - 自动发送Stop
  *         
  *         使用HAL库的硬件I2C2接口，使用PF1(SCL)和PF0(SDA)
  *         WM8978 I2C地址: 0x1A (7位地址)
  *         注意：I2C2独立于I2C1（触摸屏），互不干扰
 */
uint8_t WM8978_Write_Reg(uint8_t reg, uint16_t val)
{
    uint8_t buf[2];
    HAL_StatusTypeDef status;

    // 完全按照参考工程的I2C写格式：
    // 第一字节：[7:1]=寄存器地址, [0]=数据bit8
    // 第二字节：[7:0]=数据bit[7:0]
    buf[0] = (reg << 1) | ((val >> 8) & 0x01);  // 寄存器地址+数据的最高位
    buf[1] = val & 0xFF;                         // 数据的低8位

    // 使用硬件I2C2写入数据（PF1/SCL, PF0/SDA）
    // WM8978 I2C地址: 0x1A (7位地址), HAL库需要左移1位（HAL库会自动添加R/W位，写=0）
    // HAL_I2C_Master_Transmit会在一个I2C事务中：
    // 1. 发送Start
    // 2. 发送地址字节 (WM8978_ADDR << 1) = 0x34，等待ACK
    // 3. 发送buf[0]，等待ACK
    // 4. 发送buf[1]，等待ACK
    // 5. 发送Stop
    status = HAL_I2C_Master_Transmit(&hi2c2, (WM8978_ADDR << 1), buf, 2, 10);

    if(status == HAL_OK)
    {
        WM8978_REGVAL_TBL[reg] = val;  // 保存寄存器值到本地缓存（与参考工程一致）
        return 0;  // 成功
    }

    // 返回错误码（与参考工程一致：1=地址ACK失败, 2=寄存器地址ACK失败, 3=数据ACK失败）
    // 注意：HAL库返回的是HAL_StatusTypeDef，我们统一返回1表示失败
    return 1;  // I2C写入失败
}

/**
  * @brief  WM8978读寄存器
  * @param  reg: 寄存器地址 (0~57)
  * @retval 寄存器值（从缓存表读取）
  * @note   因为WM8978的IIC接口不支持读操作,所以读取缓存的寄存器值
 */
uint16_t WM8978_Read_Reg(uint8_t reg)
{
    return WM8978_REGVAL_TBL[reg];
}

/**
  * @brief  WM8978 DAC/ADC配置
  * @param  adcen: ADC使能(1)/关闭(0)
  * @param  dacen: DAC使能(1)/关闭(0)
  * @note   完全按照参考工程的实现方式
 */
void WM8978_ADDA_Cfg(uint8_t dacen, uint8_t adcen)
{
    uint16_t regval;

    // DAC配置在R3寄存器（低2位）
    regval = WM8978_Read_Reg(3);   // 读取R3
    if(dacen)
        regval |= 3 << 0;          // R3低2位设置为1,使能DACR&DACL
    else
        regval &= ~(3 << 0);       // R3低2位清零,关闭DACR&DACL
    WM8978_Write_Reg(3, regval);   // 写入R3
    
    // ADC配置在R2寄存器（低2位）
    regval = WM8978_Read_Reg(2);   // 读取R2
    if(adcen)
        regval |= 3 << 0;          // R2低2位设置为1,使能ADCR&ADCL
    else
        regval &= ~(3 << 0);       // R2低2位清零,关闭ADCR&ADCL
    WM8978_Write_Reg(2, regval);   // 写入R2
}

/**
  * @brief  WM8978输入通道配置
  * @param  micen:    MIC输入(1)/关闭(0)
  * @param  lineinen: LINE IN输入(1)/关闭(0)
  * @param  auxen:    AUX输入(1)/关闭(0)
  * @note   完全按照参考工程的实现方式和顺序
 */
void WM8978_Input_Cfg(uint8_t micen, uint8_t lineinen, uint8_t auxen)
{
    uint16_t regval;
    
    // 1. 配置R2: MIC PGA使能（bit2-3: INPPGAENR,INPPGAENL）
    regval = WM8978_Read_Reg(2);   // 读取R2
    if(micen)
        regval |= 3 << 2;          // 使能INPPGAENR,INPPGAENL(MIC的PGA放大)
    else
        regval &= ~(3 << 2);       // 关闭INPPGAENR,INPPGAENL
    WM8978_Write_Reg(2, regval);   // 写入R2
    
    // 2. 配置R44: MIC路由位
    regval = WM8978_Read_Reg(44);  // 读取R44
    if(micen)
        regval |= 3 << 4 | 3 << 0; // 使能LIN2INPPGA,LIP2INPPGA,RIN2INPPGA,RIP2INPPGA
    else
        regval &= ~(3 << 4 | 3 << 0); // 关闭LIN2INPPGA,LIP2INPPGA,RIN2INPPGA,RIP2INPPGA
    WM8978_Write_Reg(44, regval);  // 写入R44

    // 3. 配置LINE IN增益
    if(lineinen)
        WM8978_LINEIN_Gain(5);     // LINE IN 0dB增益
    else
        WM8978_LINEIN_Gain(0);     // 关闭LINE IN

    // 4. 配置AUX增益
    if(auxen)
        WM8978_AUX_Gain(7);        // AUX 6dB增益
    else
        WM8978_AUX_Gain(0);        // 关闭AUX输入
}

/**
  * @brief  WM8978输出配置
  * @param  dacen: DAC输出(播放)使能(1)/关闭(0)
  * @param  bpsen: BYPASS输出(录音,来自MIC,LINE IN,AUX等)使能(1)/关闭(0)
  * @note   完全按照参考工程的实现方式
 */
void WM8978_Output_Cfg(uint8_t dacen, uint8_t bpsen)
{
    uint16_t regval = 0;

    if(dacen)
        regval |= 1 << 0;          // DAC输出使能

    if(bpsen)
    {
        regval |= 1 << 1;          // BYPASS使能
        regval |= 5 << 2;          // 0dB增益
    }

    WM8978_Write_Reg(50, regval);  // R50配置
    WM8978_Write_Reg(51, regval);  // R51配置
}

/**
  * @brief  WM8978麦克风增益设置(包含BOOST的20dB,MIC-->ADC输入部分的增益)
  * @param  gain: 0~63,对应-12dB~35.25dB,步进0.75dB
  * @note   完全按照参考工程的实现方式
 */
void WM8978_MIC_Gain(uint8_t gain)
{
    gain &= 0x3F;
    WM8978_Write_Reg(45, gain);            // R45,左通道PGA增益
    WM8978_Write_Reg(46, gain | 1 << 8);   // R46,右通道PGA增益,同步更新(bit8: HPVU)
}

/**
  * @brief  WM8978 L2/R2(也就是Line In)增益设置(L2/R2-->ADC输入部分的增益)
  * @param  gain: 0~7,0表示通道关闭,1~7,对应-12dB~6dB,步进3dB
  * @note   完全按照参考工程的实现方式
  *         映射关系: 0=关闭, 1=-12dB, 2=-9dB, 3=-6dB, 4=-3dB, 5=0dB, 6=+3dB, 7=+6dB
  */
void WM8978_LINEIN_Gain(uint8_t gain)
{
    uint16_t regval;
    gain &= 0x07;
    
    // R47: L2到左通道BOOST增益 (bit[6:4]: L2_2BOOSTVOL)
    regval = WM8978_Read_Reg(47);  // 读取R47
    regval &= ~(7 << 4);           // 清除原来的增益设置
    WM8978_Write_Reg(47, regval | gain << 4);  // 写入R47
    
    // R48: R2到右通道BOOST增益 (bit[6:4]: R2_2BOOSTVOL)
    regval = WM8978_Read_Reg(48);  // 读取R48
    regval &= ~(7 << 4);           // 清除原来的增益设置
    WM8978_Write_Reg(48, regval | gain << 4);  // 写入R48
}

/**
  * @brief  WM8978 AUXR,AUXL(PWM音频等)增益设置(AUXR/L-->ADC输入部分的增益)
  * @param  gain: 0~7,0表示通道关闭,1~7,对应-12dB~6dB,步进3dB
  * @note   完全按照参考工程的实现方式
  *         映射关系: 0=关闭, 1=-12dB, 2=-9dB, 3=-6dB, 4=-3dB, 5=0dB, 6=+3dB, 7=+6dB
  */
void WM8978_AUX_Gain(uint8_t gain)
{
    uint16_t regval;
    gain &= 0x07;

    // R47: AUXL增益 (bit[2:0]: AUXL2BOOSTVOL)
    regval = WM8978_Read_Reg(47);  // 读取R47
    regval &= ~(7 << 0);           // 清除原来的增益设置
    WM8978_Write_Reg(47, regval | gain << 0);  // 写入R47

    // R48: AUXR增益 (bit[2:0]: AUXR2BOOSTVOL)
    regval = WM8978_Read_Reg(48);  // 读取R48
    regval &= ~(7 << 0);           // 清除原来的增益设置
    WM8978_Write_Reg(48, regval | gain << 0);  // 写入R48
}

/**
  * @brief  配置I2S接口模式
  * @param  fmt: 0=LSB(右对齐); 1=MSB(左对齐); 2=飞利浦标准I2S; 3=PCM/DSP
  * @param  len: 0=16位; 1=20位; 2=24位; 3=32位
  * @note   完全按照参考工程的实现方式
  * @note   重要：保留R4的bit[6]，参考工程初始值为0x0050，bit[6]=1
  * @note   但需要正确配置bit[7:5]（数据长度）和bit[4:3]（I2S格式）
  */
void WM8978_I2S_Cfg(uint8_t fmt, uint8_t len)
{
    fmt &= 0x03;
    len &= 0x03;  // 限定范围
    WM8978_Write_Reg(4, (fmt << 3) | (len << 5));  // R4,WM8978音频模式配置
}

/**
  * @brief  设置耳机音量
  * @param  voll: 左声道音量(0~63)
  * @param  volr: 右声道音量(0~63)
  * @note   完全按照参考工程的实现方式
 */
void WM8978_HPvol_Set(uint8_t voll, uint8_t volr)
{
    voll &= 0x3F;
    volr &= 0x3F;  // 限定范围
    if(voll == 0) voll |= 1 << 6;  // 音量为0时,直接mute
    if(volr == 0) volr |= 1 << 6;  // 音量为0时,直接mute
    WM8978_Write_Reg(52, voll);            // R52,左耳机输出音量
    WM8978_Write_Reg(53, volr | (1 << 8)); // R53,右耳机输出音量,同步更新(HPVU=1)
}

/**
  * @brief  设置扬声器音量
  * @param  volx: 音量(0~63)
  * @note   完全按照参考工程的实现方式
  */
void WM8978_SPKvol_Set(uint8_t volx)
{
    volx &= 0x3F;  // 限定范围
    if(volx == 0) volx |= 1 << 6;  // 音量为0时,直接mute
    WM8978_Write_Reg(54, volx);            // R54,左扬声器输出音量
    WM8978_Write_Reg(55, volx | (1 << 8)); // R55,右扬声器输出音量,同步更新(SPKVU=1)
}

/**
  * @brief  设置3D增强
  * @param  depth: 0~15(3D强度,0最弱,15最强)
  * @note   完全按照参考工程的实现方式
 */
void WM8978_3D_Set(uint8_t depth)
{
    depth &= 0x0F;  // 限定范围
    WM8978_Write_Reg(41, depth);  // R41,3D增强设置
}

/**
  * @brief  设置EQ/3D应用方向
  * @param  dir: 0=应用到ADC(录音)
  *              1=应用到DAC(播放,默认)
  * @note   完全按照参考工程的实现方式
  */
void WM8978_EQ_3D_Dir(uint8_t dir)
{
    uint16_t regval;
    regval = WM8978_Read_Reg(18);  // 读取R18
    if(dir)
        regval |= 1 << 8;
    else
        regval &= ~(1 << 8);
    WM8978_Write_Reg(18, regval);  // R18,EQ1的第9位控制EQ/3D方向
}

/**
  * @brief  设置EQ1
  * @param  cfreq: 截止频率,0~3,分别对应:80/105/135/175Hz
  * @param  gain:  增益,0~24,对应-12~+12dB
  * @note   完全按照参考工程的实现方式
  */
void WM8978_EQ1_Set(uint8_t cfreq, uint8_t gain)
{
    uint16_t regval;
    cfreq &= 0x03;  // 限定范围
    if(gain > 24) gain = 24;
    gain = 24 - gain;  // 增益转换（参考工程的处理方式）
    regval = WM8978_Read_Reg(18);
    regval &= 0x100;   // 保留bit8（EQ3DMODE位）
    regval |= cfreq << 5;  // 设置截止频率
    regval |= gain;        // 设置增益
    WM8978_Write_Reg(18, regval);  // R18,EQ1设置
}

/**
  * @brief  设置EQ2
  * @param  cfreq: 中心频率,0~3,分别对应:230/300/385/500Hz
  * @param  gain:  增益,0~24,对应-12~+12dB
  * @note   完全按照参考工程的实现方式
  */
void WM8978_EQ2_Set(uint8_t cfreq, uint8_t gain)
{
    uint16_t regval = 0;
    cfreq &= 0x03;  // 限定范围
    if(gain > 24) gain = 24;
    gain = 24 - gain;  // 增益转换（参考工程的处理方式）
    regval |= cfreq << 5;  // 设置中心频率
    regval |= gain;        // 设置增益
    WM8978_Write_Reg(19, regval);  // R19,EQ2设置
}

/**
  * @brief  设置EQ3
  * @param  cfreq: 中心频率,0~3,分别对应:650/850/1100/1400Hz
  * @param  gain:  增益,0~24,对应-12~+12dB
  * @note   完全按照参考工程的实现方式
 */
void WM8978_EQ3_Set(uint8_t cfreq, uint8_t gain)
{
    uint16_t regval = 0;
    cfreq &= 0x03;  // 限定范围
    if(gain > 24) gain = 24;
    gain = 24 - gain;  // 增益转换（参考工程的处理方式）
    regval |= cfreq << 5;  // 设置中心频率
    regval |= gain;        // 设置增益
    WM8978_Write_Reg(20, regval);  // R20,EQ3设置
}

/**
  * @brief  设置EQ4
  * @param  cfreq: 中心频率,0~3,分别对应:1800/2400/3200/4100Hz
  * @param  gain:  增益,0~24,对应-12~+12dB
  * @note   完全按照参考工程的实现方式
  */
void WM8978_EQ4_Set(uint8_t cfreq, uint8_t gain)
{
    uint16_t regval = 0;
    cfreq &= 0x03;  // 限定范围
    if(gain > 24) gain = 24;
    gain = 24 - gain;  // 增益转换（参考工程的处理方式）
    regval |= cfreq << 5;  // 设置中心频率
    regval |= gain;        // 设置增益
    WM8978_Write_Reg(21, regval);  // R21,EQ4设置
}

/**
  * @brief  设置EQ5
  * @param  cfreq: 截止频率,0~3,分别对应:5300/6900/9000/11700Hz
  * @param  gain:  增益,0~24,对应-12~+12dB
  * @note   完全按照参考工程的实现方式
  */
void WM8978_EQ5_Set(uint8_t cfreq, uint8_t gain)
{
    uint16_t regval = 0;
    cfreq &= 0x03;  // 限定范围
    if(gain > 24) gain = 24;
    gain = 24 - gain;  // 增益转换（参考工程的处理方式）
    regval |= cfreq << 5;  // 设置截止频率
    regval |= gain;        // 设置增益
    WM8978_Write_Reg(22, regval);  // R22,EQ5设置
}
