#include "recorder_audio.h"
#include "wm8978.h"
#include "i2s.h"
#include "debug_uart.h"

// 外部DMA句柄
extern DMA_HandleTypeDef hdma_i2s2_ext_rx;
extern I2S_HandleTypeDef hi2s2;

// 录音状态
static volatile uint8_t recorder_audio_recording = 0;

// 静音数据（用于I2S2 TX提供时钟，录音时I2S2作为Master发送时钟）
// 使用较大的缓冲区，确保DMA可以持续传输
static const int16_t silence_data[256] = {0};  // 512字节，足够大的循环缓冲区

// 打印WM8978关键寄存器（录音链路）
static void Recorder_Log_WM8978_RecPath(const char* tag)
{
    uint16_t r1  = WM8978_Read_Reg(1);
    uint16_t r2  = WM8978_Read_Reg(2);
    uint16_t r3  = WM8978_Read_Reg(3);
    uint16_t r4  = WM8978_Read_Reg(4);
    uint16_t r6  = WM8978_Read_Reg(6);
    uint16_t r10 = WM8978_Read_Reg(10);
    uint16_t r14 = WM8978_Read_Reg(14);
    uint16_t r43 = WM8978_Read_Reg(43);
    uint16_t r44 = WM8978_Read_Reg(44);
    uint16_t r45 = WM8978_Read_Reg(45);
    uint16_t r46 = WM8978_Read_Reg(46);
    uint16_t r47 = WM8978_Read_Reg(47);
    uint16_t r48 = WM8978_Read_Reg(48);
    LOGI("[WM8978_DEBUG] %s: R1=0x%04X R2=0x%04X R3=0x%04X R4=0x%04X R6=0x%04X\r\n",
         tag, r1, r2, r3, r4, r6);
    LOGI("[WM8978_DEBUG] %s: R10=0x%04X R14=0x%04X R43=0x%04X R44=0x%04X R45=0x%04X R46=0x%04X R47=0x%04X R48=0x%04X\r\n",
         tag, r10, r14, r43, r44, r45, r46, r47, r48);
}

/**
 * @brief 初始化录音音频模块
 */
void Recorder_Audio_Init(void)
{
    recorder_audio_recording = 0;
    LOGI("[RECORDER_AUDIO] Initialized\r\n");
}

/**
 * @brief 进入录音模式（配置WM8978和I2S）
 * 参考例程：recoder_enter_rec_mode()
 */
void Recorder_Audio_EnterRecordMode(void)
{
    LOGI("[RECORDER_AUDIO] Entering record mode...\r\n");
    
    // 1. 配置WM8978为录音模式
    WM8978_ADDA_Cfg(0, 1);        // 禁用DAC，使能ADC
    // 根据模块原理图：优先使用 MIC 输入（最常见）
    // 如果 MIC 没有信号，可以尝试只使能 MIC（不使能 LINEIN，避免冲突）
    WM8978_Input_Cfg(1, 0, 0);    // 只使能MIC输入，关闭LINE IN和AUX（避免冲突）
                                    // 注意：WM8978_Input_Cfg会自动调用WM8978_LINEIN_Gain(0)关闭LINE IN增益
    WM8978_Output_Cfg(0, 1);      // 禁用DAC输出，使能BYPASS输出（用于监听MIC输入）
                                    // 注意：BYPASS输出只路由当前使能的输入（MIC），LINE IN已被关闭
    
    // 读取配置后的寄存器值用于调试
    uint16_t r2_val = WM8978_Read_Reg(2);
    uint16_t r44_val = WM8978_Read_Reg(44);
    uint16_t r45_val = WM8978_Read_Reg(45);
    uint16_t r46_val = WM8978_Read_Reg(46);
    LOGI("[RECORDER_AUDIO] After Input_Cfg: R2=0x%04X, R44=0x%04X, R45=0x%04X, R46=0x%04X\r\n", 
         r2_val, r44_val, r45_val, r46_val);
    // 详细解析 R44/R45 的输入路由位
    LOGI("[RECORDER_AUDIO] R44 detail: LIP2INPPGA=%d, LIN2INPPGA=%d, L2_2INPPGA=%d, INPPGAVOLL=%d\r\n",
         (r44_val >> 0) & 1, (r44_val >> 1) & 1, (r44_val >> 2) & 1, (r44_val >> 0) & 0x3F);
    LOGI("[RECORDER_AUDIO] R44 detail: RIN2INPPGA=%d, RIP2INPPGA=%d\r\n",
         (r44_val >> 4) & 1, (r44_val >> 5) & 1);
    
    WM8978_MIC_Gain(46);          // MIC增益设置（参考例程：46对应约+17.25dB）
    
    // 读取设置增益后的寄存器值
    r44_val = WM8978_Read_Reg(44);
    r45_val = WM8978_Read_Reg(45);
    r46_val = WM8978_Read_Reg(46);
    uint16_t r47_val = WM8978_Read_Reg(47);
    uint16_t r48_val = WM8978_Read_Reg(48);
    LOGI("[RECORDER_AUDIO] After MIC_Gain: R44=0x%04X, R45=0x%04X, R46=0x%04X, R47=0x%04X, R48=0x%04X\r\n", 
         r44_val, r45_val, r46_val, r47_val, r48_val);
    // 详细解析输入路由和增益
    LOGI("[RECORDER_AUDIO] R44: LIP2INPPGA=%d, LIN2INPPGA=%d, L2_2INPPGA=%d, INPPGAVOLL=%d (%.1fdB)\r\n",
         (r44_val >> 0) & 1, (r44_val >> 1) & 1, (r44_val >> 2) & 1, 
         (r44_val >> 0) & 0x3F, ((int16_t)(r44_val & 0x3F) - 16) * 0.75f);
    LOGI("[RECORDER_AUDIO] R44: RIN2INPPGA=%d, RIP2INPPGA=%d\r\n",
         (r44_val >> 4) & 1, (r44_val >> 5) & 1);
    LOGI("[RECORDER_AUDIO] R45: R2_2INPPGA=%d, INPPGAVOLR=%d (%.1fdB)\r\n",
         (r45_val >> 6) & 1, (r45_val >> 0) & 0x3F, ((int16_t)(r45_val & 0x3F) - 16) * 0.75f);
    LOGI("[RECORDER_AUDIO] R46: INPPGAVOLR=%d (%.1fdB)\r\n",
         (r46_val >> 0) & 0x3F, ((int16_t)(r46_val & 0x3F) - 16) * 0.75f);
    LOGI("[RECORDER_AUDIO] R47: L2_2BOOSTVOL=%d, AUXL_Gain=%d, L2_2INPPGA=%d\r\n",
         (r47_val >> 4) & 7, (r47_val >> 0) & 7, (r47_val >> 9) & 1);
    LOGI("[RECORDER_AUDIO] R48: R2_2BOOSTVOL=%d, AUXR_Gain=%d, R2_2INPPGA=%d\r\n",
         (r48_val >> 4) & 7, (r48_val >> 0) & 7, (r48_val >> 9) & 1);
    LOGI("[RECORDER_AUDIO] Input config: MIC=1, LINEIN=1, AUX=1 (all enabled for debugging)\r\n");
    LOGI("[RECORDER_AUDIO] MIC Gain: 46 (+17.25dB)\r\n");
    Recorder_Log_WM8978_RecPath("After WM8978 MIC/Input/I2S cfg");
    
    // 2. 配置I2S格式（Philips标准，16bit）
    // 参考例程直接使用 WM8978_I2S_Cfg(2, 0)，这会设置：
    // bit[4:3]=2 (I2S格式), bit[7:5]=0 (16bit)
    // 但会清除bit[6]，而参考工程的R4初始值是0x0050（bit[6]=1）
    // 为了与参考例程保持一致，我们也直接使用WM8978_I2S_Cfg
    // 如果后续有问题，再考虑保留bit[6]
    WM8978_I2S_Cfg(2, 0);  // I2S格式，16bit（与参考例程一致）
    
    // 验证R4配置
    uint16_t r4_after = WM8978_Read_Reg(4);
    uint8_t fmt_verify = (r4_after >> 3) & 0x03;  // bit[4:3]，2位
    uint8_t len_verify = (r4_after >> 5) & 0x07;  // bit[7:5]，3位
    LOGI("[RECORDER_AUDIO] R4 after I2S_Cfg: 0x%04X (bit[4:3]=%d=I2S格式, bit[7:5]=%d=数据长度)\r\n",
         r4_after, fmt_verify, len_verify);
    
    // 重要：检查WM8978的ADC输出是否已配置到I2S接口
    // R4的bit[4:3]应该为2（I2S格式），bit[7:5]应该为0（16bit）
    if(fmt_verify != 2) {
        LOGI("[RECORDER_AUDIO] ERROR: R4 I2S format is not correct! Expected 2 (I2S), got %d\r\n", fmt_verify);
    }
    if(len_verify != 0) {
        LOGI("[RECORDER_AUDIO] ERROR: R4 data length is not correct! Expected 0 (16bit), got %d\r\n", len_verify);
    }
    
    // 3. 初始化I2S2（Master TX模式，提供时钟）
    // 注意：I2S2_Init()会直接操作寄存器，不需要HAL库
    // 但为了确保I2S2被禁用后再配置，先禁用
    LOGI("[RECORDER_AUDIO] Before I2S2_Init: I2SCFGR=0x%08lX\r\n", SPI2->I2SCFGR);
    SPI2->I2SCFGR &= ~SPI_I2SCFGR_I2SE;  // 直接禁用I2S2
    LOGI("[RECORDER_AUDIO] After disable before Init: I2SCFGR=0x%08lX\r\n", SPI2->I2SCFGR);
    hi2s2.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_ENABLE;  // 设置全双工模式标志（虽然不使用HAL库，但保留标志）
    I2S2_Init(I2S_Standard_Phillips, I2S_Mode_MasterTx, I2S_CPOL_Low, I2S_DataFormat_16b);
    
    // 调试：检查I2S2配置后的寄存器值
    uint32_t i2s2_cfg_check = SPI2->I2SCFGR;
    LOGI("[RECORDER_AUDIO] I2S2 after Init: I2SCFGR=0x%08lX (I2SE=%d, I2SCFG=%d, I2SMOD=%d)\r\n",
         i2s2_cfg_check,
         (i2s2_cfg_check >> 10) & 1,     // I2SE位(bit10)
         (i2s2_cfg_check >> 8) & 3,      // I2SCFG[1:0]
         (i2s2_cfg_check >> 11) & 1);    // I2SMOD位
    
    // 如果I2S2配置不正确，强制修正（包括I2SE使能位）
    if(((i2s2_cfg_check >> 8) & 3) != 2 || ((i2s2_cfg_check >> 10) & 1) == 0) {
        LOGI("[RECORDER_AUDIO] WARNING: I2S2 config incorrect, correcting...\r\n");
        // 禁用I2S2（确保I2S2处于禁用状态才能修改配置）
        SPI2->I2SCFGR &= ~SPI_I2SCFGR_I2SE;
        
        // 等待I2S2完全停止（根据STM32参考手册，修改I2SCFGR需要I2S2禁用）
        volatile uint32_t timeout = 1000;
        while((SPI2->SR & SPI_SR_BSY) && timeout--);
        
        // 设置I2SCFG为Master TX (10)
        i2s2_cfg_check &= ~(3 << 8);  // 清除I2SCFG位
        i2s2_cfg_check |= (2 << 8);   // 设置为Master TX (10)
        i2s2_cfg_check |= (1 << 11);  // 确保I2SMOD=1
        i2s2_cfg_check &= ~SPI_I2SCFGR_I2SSTD;  // Philips标准
        SPI2->I2SCFGR = i2s2_cfg_check;
        
        // 重新启用I2S2（关键！）
        SPI2->I2SCFGR |= SPI_I2SCFGR_I2SE;
        
        // 验证I2S2已启用
        uint32_t verify = SPI2->I2SCFGR;
        if(((verify >> 10) & 1) == 0) {
            LOGI("[RECORDER_AUDIO] ERROR: I2S2 I2SE still disabled after correction! Retrying...\r\n");
            // 再次尝试启用
            SPI2->I2SCFGR |= SPI_I2SCFGR_I2SE;
            verify = SPI2->I2SCFGR;
        }
        LOGI("[RECORDER_AUDIO] I2S2 corrected: I2SCFGR=0x%08lX (I2SE=%d)\r\n", verify, (verify >> 10) & 1);
    }
    
    // 4. 初始化I2S2ext（Slave RX模式，接收录音数据）
    // 参考例程：I2S2ext_Init(I2S_Standard_Phillips, I2S_Mode_SlaveRx, I2S_CPOL_Low, I2S_DataFormat_16b)
    I2S2ext_Init(I2S_Standard_Phillips, I2S_Mode_SlaveRx, I2S_CPOL_Low, I2S_DataFormat_16b);
    
    // 调试：检查I2S2ext配置
    // I2S2ext_BASE = SPI2_BASE - 0x400, I2SCFGR偏移=0x1C
    volatile uint32_t *I2S2ext_I2SCFGR = (volatile uint32_t *)((uint32_t)SPI2 - 0x400 + 0x1C);
    uint32_t i2s2ext_cfg = *I2S2ext_I2SCFGR;
    LOGI("[RECORDER_AUDIO] I2S2ext I2SCFGR=0x%08lX (I2SE=%d, I2SCFG=%d, I2SMOD=%d)\r\n",
         i2s2ext_cfg, 
         (i2s2ext_cfg >> 10) & 1,     // I2SE位(bit10)
         (i2s2ext_cfg >> 8) & 3,      // I2SCFG[1:0]
         (i2s2ext_cfg >> 11) & 1);    // I2SMOD位
    
    // 调试：检查I2S2配置（I2S2ext_Init()可能影响了I2S2）
    uint32_t i2s2_cfg = SPI2->I2SCFGR;
    LOGI("[RECORDER_AUDIO] I2S2 I2SCFGR=0x%08lX (I2SE=%d, I2SCFG=%d, I2SMOD=%d)\r\n",
         i2s2_cfg,
         (i2s2_cfg >> 10) & 1,        // I2SE位(bit10)
         (i2s2_cfg >> 8) & 3,         // I2SCFG[1:0]
         (i2s2_cfg >> 11) & 1);       // I2SMOD位
    
    // 如果I2S2的I2SCFG不是Master TX模式（应该是2），强制修正
    if(((i2s2_cfg >> 8) & 3) != 2) {
        LOGI("[RECORDER_AUDIO] WARNING: I2S2 I2SCFG changed after I2S2ext_Init, correcting...\r\n");
        // 禁用I2S2
        SPI2->I2SCFGR &= ~(1 << 0);
        // 设置I2SCFG为Master TX (10)
        i2s2_cfg &= ~(3 << 8);  // 清除I2SCFG位
        i2s2_cfg |= (2 << 8);   // 设置为Master TX (10)
        i2s2_cfg |= (1 << 11);  // 确保I2SMOD=1
        SPI2->I2SCFGR = i2s2_cfg;
        LOGI("[RECORDER_AUDIO] I2S2 corrected: I2SCFGR=0x%08lX\r\n", SPI2->I2SCFGR);
        
        // 修正I2S2后，需要重新检查I2S2ext配置（可能被HAL库改变了）
        // I2S2ext_BASE = SPI2_BASE - 0x400, I2SCFGR偏移=0x1C
        volatile uint32_t *I2S2ext_I2SCFGR_check = (volatile uint32_t *)((uint32_t)SPI2 - 0x400 + 0x1C);
        uint32_t i2s2ext_cfg_check = *I2S2ext_I2SCFGR_check;
        if(((i2s2ext_cfg_check >> 8) & 3) != 1) {  // 如果不是Slave RX模式（应该是1）
            LOGI("[RECORDER_AUDIO] WARNING: I2S2ext I2SCFG changed after I2S2 correction, correcting...\r\n");
            // 禁用I2S2ext
            *I2S2ext_I2SCFGR_check &= ~(1 << 0);
            // 设置I2SCFG为Slave RX (01)
            i2s2ext_cfg_check &= ~(3 << 8);  // 清除I2SCFG位
            i2s2ext_cfg_check |= (1 << 8);   // 设置为Slave RX (01)
            i2s2ext_cfg_check |= (1 << 11);  // 确保I2SMOD=1
            *I2S2ext_I2SCFGR_check = i2s2ext_cfg_check;
            // 重新启用I2S2ext
            *I2S2ext_I2SCFGR_check |= (1 << 0);
            LOGI("[RECORDER_AUDIO] I2S2ext corrected: I2SCFGR=0x%08lX\r\n", *I2S2ext_I2SCFGR_check);
        }
    }
    
    // 调试：检查R2寄存器（ADC使能）
    uint16_t r2_val_check = WM8978_Read_Reg(2);
    LOGI("[RECORDER_AUDIO] R2 after all config: 0x%04X (ADC_EN=%d, INPPGA_EN=%d)\r\n",
         r2_val_check,
         (r2_val_check >> 0) & 3,     // ADC使能位
         (r2_val_check >> 2) & 3);    // INPPGA使能位
    
    // 重要：验证ADC是否真的使能
    if((r2_val_check & 0x03) == 0) {
        LOGI("[RECORDER_AUDIO] ERROR: ADC is NOT enabled! R2 bit[1:0]=0, ADC will not output data!\r\n");
        LOGI("[RECORDER_AUDIO] Attempting to re-enable ADC...\r\n");
        // 重新使能ADC
        WM8978_ADDA_Cfg(0, 1);
        r2_val_check = WM8978_Read_Reg(2);
        LOGI("[RECORDER_AUDIO] R2 after re-enable: 0x%04X (ADC_EN=%d)\r\n", 
             r2_val_check, (r2_val_check >> 0) & 3);
    } else {
        LOGI("[RECORDER_AUDIO] ADC is enabled: R2 bit[1:0]=%d\r\n", (r2_val_check >> 0) & 3);
    }
    
    // 5. 设置I2S采样率为16kHz（录音使用16kHz）
    if(I2S2_SampleRate_Set(RECORDER_SAMPLE_RATE) != 0) {
        LOGI("[RECORDER_AUDIO] ERROR: Failed to set I2S sample rate!\r\n");
    } else {
        // 调试：检查I2S时钟配置
        uint32_t i2spr_val = SPI2->I2SPR;
        uint32_t plli2sn = (RCC->PLLI2SCFGR >> 6) & 0x1FF;
        uint32_t plli2sr = (RCC->PLLI2SCFGR >> 28) & 0x07;
        LOGI("[RECORDER_AUDIO] I2S clock config: I2SPR=0x%04lX, PLLI2SN=%lu, PLLI2SR=%lu\r\n",
             i2spr_val, plli2sn, plli2sr);
    }
    
    // 6. 配置I2S2 TX DMA（发送静音数据提供时钟）
    // 注意：录音时I2S2作为Master发送时钟，I2S2ext作为Slave接收数据
    // 使用双缓冲模式，但只发送静音数据
    // 使用较大的缓冲区，确保DMA可以持续传输
    I2S2_TX_DMA_Init((uint8_t*)&silence_data[0], (uint8_t*)&silence_data[128], 256);
    
    // 7. 配置I2S2ext RX DMA（接收录音数据）
    // 注意：必须在I2S2ext_Init()之后调用，因为需要I2S2ext已配置
    // 这个调用在Recorder_Start()中进行，因为需要分配缓冲区
    
    // 5. 禁用I2S2 TX DMA中断（不需要中断处理，只是提供时钟）
    DMA1_Stream4->CR &= ~DMA_SxCR_TCIE;
    
    LOGI("[RECORDER_AUDIO] Record mode configured\r\n");
}

/**
 * @brief 退出录音模式（恢复播放模式配置）
 * 参考例程：recoder_enter_play_mode()
 */
void Recorder_Audio_ExitRecordMode(void)
{
    LOGI("[RECORDER_AUDIO] Exiting record mode...\r\n");
    
    // 1. 配置WM8978为播放模式
    WM8978_ADDA_Cfg(1, 0);        // 使能DAC，禁用ADC
    WM8978_Input_Cfg(0, 0, 0);    // 关闭输入通道
    WM8978_Output_Cfg(1, 0);      // 使能DAC输出，禁用BYPASS
    WM8978_MIC_Gain(0);           // MIC增益设为0
    
    // 2. 停止I2S播放和录音
    I2S_Play_Stop();
    Recorder_Audio_Stop();
    
    LOGI("[RECORDER_AUDIO] Exited record mode\r\n");
}

/**
 * @brief 启动录音（启动I2S和DMA）
 * 参考例程：I2S_Play_Start() 和 I2S_Rec_Start()
 */
void Recorder_Audio_Start(void)
{
    if(recorder_audio_recording) {
        LOGI("[RECORDER_AUDIO] Already recording\r\n");
        return;
    }
    
    LOGI("[RECORDER_AUDIO] Starting recording...\r\n");
    
    // 参考例程：先启动I2S2 TX DMA（提供时钟），然后启动I2S2ext RX DMA（接收数据）
    // I2S_Play_Start();  // 启动I2S2 TX DMA（I2S2在Init时已启用）
    // I2S_Rec_Start();  // 启动I2S2ext RX DMA（I2S2ext在Init时已启用）
    
    // 1. 启动I2S2 TX DMA（发送静音数据提供时钟）
    // 注意：I2S2在Init时已启用，这里只需要启用DMA
    LOGI("[RECORDER_AUDIO] Before I2S_Play_Start: I2SCFGR=0x%08lX\r\n", SPI2->I2SCFGR);
    LOGI("[RECORDER_AUDIO] Starting I2S2 TX DMA (Master) to provide clock...\r\n");
    I2S_Play_Start();
    LOGI("[RECORDER_AUDIO] After I2S_Play_Start: I2SCFGR=0x%08lX\r\n", SPI2->I2SCFGR);
    
    // 2. 启动I2S2ext RX DMA（接收录音数据）
    // 注意：I2S2ext在Init时已启用，这里只需要启用DMA
    LOGI("[RECORDER_AUDIO] Before I2S_Rec_Start: I2SCFGR=0x%08lX\r\n", SPI2->I2SCFGR);
    LOGI("[RECORDER_AUDIO] Starting I2S2ext RX DMA (Slave) to receive data...\r\n");
    I2S_Rec_Start();
    LOGI("[RECORDER_AUDIO] After I2S_Rec_Start: I2SCFGR=0x%08lX\r\n", SPI2->I2SCFGR);
    
    // 3. 检查并修正I2S2和I2S2ext配置（DMA启动可能影响了配置）
    uint32_t i2s2_cfg_after = SPI2->I2SCFGR;
    // I2S2ext_BASE = SPI2_BASE - 0x400, I2SCFGR偏移=0x1C
    volatile uint32_t *I2S2ext_I2SCFGR = (volatile uint32_t *)((uint32_t)SPI2 - 0x400 + 0x1C);
    uint32_t i2s2ext_cfg_after = *I2S2ext_I2SCFGR;
    
    // 检查并修正I2S2配置（包括I2SE使能位）
    uint8_t i2s2_need_correct = 0;
    if(((i2s2_cfg_after >> 8) & 3) != 2) {  // 如果不是Master TX
        i2s2_need_correct = 1;
        LOGI("[RECORDER_AUDIO] ERROR: I2S2 I2SCFG is not Master TX after start! Correcting...\r\n");
    }
    if(((i2s2_cfg_after >> 10) & 1) == 0) {  // 如果I2S2被禁用（关键！）
        i2s2_need_correct = 1;
        LOGI("[RECORDER_AUDIO] ERROR: I2S2 I2SE is disabled after start! Correcting...\r\n");
    }
    
    if(i2s2_need_correct) {
        // 禁用I2S2（如果已启用）
        SPI2->I2SCFGR &= ~SPI_I2SCFGR_I2SE;
        
        // 设置I2SCFG为Master TX (10)
        i2s2_cfg_after &= ~(3 << 8);
        i2s2_cfg_after |= (2 << 8);   // Master TX
        i2s2_cfg_after |= (1 << 11);  // I2SMOD
        i2s2_cfg_after &= ~SPI_I2SCFGR_I2SSTD;  // Philips标准
        SPI2->I2SCFGR = i2s2_cfg_after;
        
        // 重新启用I2S2（关键：必须启用才能输出时钟）
        SPI2->I2SCFGR |= SPI_I2SCFGR_I2SE;
        LOGI("[RECORDER_AUDIO] I2S2 corrected after start: I2SCFGR=0x%08lX\r\n", SPI2->I2SCFGR);
        i2s2_cfg_after = SPI2->I2SCFGR;  // 更新值
    }
    
    // 检查并修正I2S2ext配置
    if(((i2s2ext_cfg_after >> 8) & 3) != 1) {  // 如果不是Slave RX
        LOGI("[RECORDER_AUDIO] ERROR: I2S2ext I2SCFG is not Slave RX after start! Correcting...\r\n");
        // 禁用I2S2ext
        *I2S2ext_I2SCFGR &= ~(1 << 10);
        // 设置I2SCFG为Slave RX (01)
        i2s2ext_cfg_after &= ~(3 << 8);
        i2s2ext_cfg_after |= (1 << 8);   // Slave RX
        i2s2ext_cfg_after |= (1 << 11);  // I2SMOD
        *I2S2ext_I2SCFGR = i2s2ext_cfg_after;
        // 重新启用I2S2ext
        *I2S2ext_I2SCFGR |= (1 << 10);
        LOGI("[RECORDER_AUDIO] I2S2ext corrected after start: I2SCFGR=0x%08lX\r\n", *I2S2ext_I2SCFGR);
    }
    
    LOGI("[RECORDER_AUDIO] After start: I2S2 I2SCFGR=0x%08lX (I2SE=%d, I2SCFG=%d), I2S2ext I2SCFGR=0x%08lX (I2SE=%d, I2SCFG=%d)\r\n",
         SPI2->I2SCFGR, (SPI2->I2SCFGR >> 10) & 1, (SPI2->I2SCFGR >> 8) & 3,
         *I2S2ext_I2SCFGR, (*I2S2ext_I2SCFGR >> 10) & 1, (*I2S2ext_I2SCFGR >> 8) & 3);
    
    // 4. 详细的调试信息
    // 4.1 检查WM8978 ADC状态
    uint16_t r2_final = WM8978_Read_Reg(2);
    uint16_t r44_final = WM8978_Read_Reg(44);
    uint16_t r45_final = WM8978_Read_Reg(45);
    LOGI("[RECORDER_AUDIO] WM8978 ADC status: R2=0x%04X (ADC_EN=%d), R44=0x%04X, R45=0x%04X\r\n",
         r2_final, (r2_final >> 0) & 3, r44_final, r45_final);
    
    // 重要：检查WM8978的ADC输出到I2S接口的完整路径
    // R2 bit[1:0] = ADC使能（必须为11）
    // R4 bit[4:3] = I2S格式（必须为10=I2S）
    // R4 bit[7:5] = 数据长度（必须为000=16bit）
    uint16_t r4_final = WM8978_Read_Reg(4);
    LOGI("[RECORDER_AUDIO] Final WM8978 config check:\r\n");
    LOGI("[RECORDER_AUDIO]   R2=0x%04X: ADC_EN[1:0]=%d (should be 3), INPPGA_EN[3:2]=%d\r\n",
         r2_final, (r2_final >> 0) & 3, (r2_final >> 2) & 3);
    LOGI("[RECORDER_AUDIO]   R4=0x%04X: I2S_FMT[4:3]=%d (should be 2=I2S), DATA_LEN[7:5]=%d (should be 0=16bit)\r\n",
         r4_final, (r4_final >> 3) & 3, (r4_final >> 5) & 3);
    
    if(((r2_final >> 0) & 3) != 3) {
        LOGI("[RECORDER_AUDIO] ERROR: ADC is NOT enabled! R2 bit[1:0]=%d, should be 3!\r\n", (r2_final >> 0) & 3);
    }
    if(((r4_final >> 3) & 3) != 2) {
        LOGI("[RECORDER_AUDIO] ERROR: I2S format is NOT I2S! R4 bit[4:3]=%d, should be 2!\r\n", (r4_final >> 3) & 3);
    }
    if(((r4_final >> 5) & 3) != 0) {
        LOGI("[RECORDER_AUDIO] ERROR: Data length is NOT 16bit! R4 bit[7:5]=%d, should be 0!\r\n", (r4_final >> 5) & 3);
    }
    
    // 4.2 检查I2S2时钟配置
    uint32_t i2spr_final = SPI2->I2SPR;
    uint32_t i2scfgr_final = SPI2->I2SCFGR;
    uint32_t cr2_final = SPI2->CR2;
    LOGI("[RECORDER_AUDIO] I2S2 clock status: I2SPR=0x%04lX, I2SCFGR=0x%08lX, CR2=0x%08lX\r\n",
         i2spr_final, i2scfgr_final, cr2_final);
    LOGI("[RECORDER_AUDIO] I2S2 clock: I2SE=%d, I2SCFG=%d, MCKOE=%d, TXDMAEN=%d\r\n",
         (i2scfgr_final >> 10) & 1, (i2scfgr_final >> 8) & 3, (i2spr_final >> 9) & 1, (cr2_final >> 1) & 1);
    
    // 4.3 检查I2S2ext DMA状态
    uint32_t dma1_s3_cr = DMA1_Stream3->CR;
    uint32_t dma1_s3_ndtr = DMA1_Stream3->NDTR;
    uint32_t dma1_s3_par = DMA1_Stream3->PAR;
    uint32_t dma1_s3_m0ar = DMA1_Stream3->M0AR;
    uint32_t dma1_s3_m1ar = DMA1_Stream3->M1AR;
    LOGI("[RECORDER_AUDIO] I2S2ext DMA status: CR=0x%08lX, NDTR=%lu, PAR=0x%08lX, M0AR=0x%08lX, M1AR=0x%08lX\r\n",
         dma1_s3_cr, dma1_s3_ndtr, dma1_s3_par, dma1_s3_m0ar, dma1_s3_m1ar);
    LOGI("[RECORDER_AUDIO] I2S2ext DMA: EN=%d, DIR=%d, CH=%d, TCIF=%d, HTIF=%d\r\n",
         (dma1_s3_cr >> 0) & 1, (dma1_s3_cr >> 6) & 3, (dma1_s3_cr >> 25) & 7,
         (DMA1->LISR >> 5) & 1, (DMA1->LISR >> 3) & 1);
    
    // 4.4 检查I2S2 TX DMA状态
    uint32_t dma1_s4_cr = DMA1_Stream4->CR;
    uint32_t dma1_s4_ndtr = DMA1_Stream4->NDTR;
    LOGI("[RECORDER_AUDIO] I2S2 TX DMA status: CR=0x%08lX, NDTR=%lu, EN=%d\r\n",
         dma1_s4_cr, dma1_s4_ndtr, (dma1_s4_cr >> 0) & 1);
    
    // 4.5 检查I2S2ext寄存器状态
    uint32_t i2s2ext_cr2 = SPI2->CR2;  // I2S2ext使用SPI2的CR2寄存器
    LOGI("[RECORDER_AUDIO] I2S2ext CR2: RXDMAEN=%d\r\n", (i2s2ext_cr2 >> 1) & 1);
    
    // 4.6 检查PLLI2S状态
    uint32_t rcc_cr = RCC->CR;
    uint32_t plli2s_rdy = (rcc_cr >> 27) & 1;
    uint32_t plli2s_on = (rcc_cr >> 26) & 1;
    uint32_t plli2sn = (RCC->PLLI2SCFGR >> 6) & 0x1FF;
    uint32_t plli2sr = (RCC->PLLI2SCFGR >> 28) & 0x07;
    LOGI("[RECORDER_AUDIO] PLLI2S status: ON=%lu, RDY=%lu, PLLI2SN=%lu, PLLI2SR=%lu\r\n",
         plli2s_on, plli2s_rdy, plli2sn, plli2sr);
    
    // 4.7 检查WM8978 I2S接口配置
    uint16_t r4_wm8978 = WM8978_Read_Reg(4);  // R4: I2S接口配置
    LOGI("[RECORDER_AUDIO] WM8978 I2S config: R4=0x%04X (I2S format and data length)\r\n", r4_wm8978);
    
    // 4.8 检查I2S2 DR寄存器（数据寄存器）状态
    // 注意：I2S2作为Master TX，应该能写入数据
    // I2S2ext作为Slave RX，应该能读取数据
    uint32_t i2s2_sr = SPI2->SR;  // I2S2状态寄存器
    LOGI("[RECORDER_AUDIO] I2S2 SR: TXE=%d, RXNE=%d, BSY=%d, UDR=%d, CHSIDE=%d\r\n",
         (i2s2_sr >> 1) & 1, (i2s2_sr >> 0) & 1, (i2s2_sr >> 7) & 1,
         (i2s2_sr >> 3) & 1, (i2s2_sr >> 2) & 1);
    
    // 4.9 等待一小段时间，然后再次检查数据
    HAL_Delay(50);  // 等待50ms，让DMA传输一些数据
    
    // 检查DMA是否在运行
    uint32_t dma_s3_cr_after = DMA1_Stream3->CR;
    uint32_t dma_s3_ndtr_after = DMA1_Stream3->NDTR;
    uint32_t dma_s3_par_after = DMA1_Stream3->PAR;
    LOGI("[RECORDER_AUDIO] After 50ms: DMA3 CR=0x%08lX, NDTR=%lu, PAR=0x%08lX, EN=%d\r\n",
         dma_s3_cr_after, dma_s3_ndtr_after, dma_s3_par_after, (dma_s3_cr_after >> 0) & 1);
    
    // 检查I2S2ext状态寄存器（如果可访问）
    // 注意：I2S2ext可能没有独立的状态寄存器，使用SPI2的SR寄存器
    uint32_t i2s2_sr_after = SPI2->SR;
    LOGI("[RECORDER_AUDIO] After 50ms: I2S2 SR=0x%08lX (TXE=%d, RXNE=%d, BSY=%d)\r\n",
         i2s2_sr_after, (i2s2_sr_after >> 1) & 1, (i2s2_sr_after >> 0) & 1, (i2s2_sr_after >> 7) & 1);
    
    recorder_audio_recording = 1;
    LOGI("[RECORDER_AUDIO] Recording started\r\n");
}

/**
 * @brief 停止录音（停止I2S和DMA）
 */
void Recorder_Audio_Stop(void)
{
    if(!recorder_audio_recording) {
        return;
    }
    
    LOGI("[RECORDER_AUDIO] Stopping recording...\r\n");
    
    // 1. 停止I2S2ext接收
    I2S_Rec_Stop();
    
    // 2. 停止I2S2 TX（停止时钟发送）
    I2S_Play_Stop();
    
    recorder_audio_recording = 0;
    LOGI("[RECORDER_AUDIO] Recording stopped\r\n");
}

/**
 * @brief 检查是否正在录音
 */
uint8_t Recorder_Audio_IsRecording(void)
{
    return recorder_audio_recording;
}
