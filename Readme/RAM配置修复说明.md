# RAM配置修复说明

## 问题描述

编译时出现链接错误：
```
Error: L6406E: No space in execution regions with .ANY selector matching ...
```

**根本原因**：链接器脚本和项目文件中的RAM配置不正确。

## STM32F407ZGT6 RAM规格

- **总RAM**: 192KB (0x20000000 - 0x2002FFFF)
- **IRAM1**: 128KB (0x20000000 - 0x2001FFFF)
- **IRAM2**: 64KB (0x20020000 - 0x2002FFFF)

**注意**：虽然IRAM1和IRAM2是分开的，但Keil链接器通常将它们合并为一个连续的192KB区域。

## 修复内容

### 1. 链接器脚本修复 (`MDK-ARM/project/project.sct`)

**修改前**:
```sct
RW_IRAM1 0x20000000 0x00020000  {  ; RW data (128KB)
```

**修改后**:
```sct
RW_IRAM1 0x20000000 0x00030000  {  ; RW data (192KB for STM32F407ZGT6)
```

### 2. 项目文件修复 (`MDK-ARM/project.uvprojx`)

**修改前**:
```xml
<Cpu>IRAM(0x20000000,0x00020000) ...</Cpu>
```

**修改后**:
```xml
<Cpu>IRAM(0x20000000,0x00030000) ...</Cpu>
```

## 为什么会出现这个问题？

1. **CubeMX配置问题**：CubeMX中可能选择了错误的MCU型号（STM32F407VETx只有128KB RAM）
2. **项目迁移**：项目可能是从其他型号迁移过来的，RAM配置没有更新
3. **手动配置错误**：手动配置时使用了错误的RAM大小

## 验证修复

修复后，重新编译应该能够成功。RAM使用情况：

```
总RAM: 192KB
- 堆内存: 48KB (0xC000)
- 栈内存: 16KB (0x4000)
- 静态变量: ~48KB (LVGL内存池)
- 其他数据: ~80KB (剩余空间)
```

## 如果问题仍然存在

如果修复后仍然出现链接错误，可能需要：

1. **检查CubeMX配置**：
   - 打开 `project.ioc`
   - 确认MCU型号为 `STM32F407ZGTx`
   - 重新生成代码

2. **清理并重新编译**：
   - 在Keil中执行 `Project -> Clean Targets`
   - 然后重新编译

3. **检查其他RAM区域**：
   - 确认IRAM2配置是否正确
   - 检查是否有其他内存区域配置错误

## 注意事项

- **不要手动修改**：如果使用CubeMX，建议在CubeMX中修改配置后重新生成代码
- **备份项目**：修改链接器脚本前建议备份
- **验证硬件**：确认实际使用的MCU确实是STM32F407ZGT6
