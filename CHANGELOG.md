# 变更记录

## 未发布

- 实现兼容 C23、C13、C10 的 UART Bootloader 核心状态机。
- 恢复应用元数据、CRC16、升级重试、末页保留和应用交接。
- 恢复 F303 120 MHz、E230 72 MHz 时钟及双 UART 配置。
- 支持 F303 容量检测与超过 512 KiB 时的第二 Flash bank。
- 加入 Kconfig/menuconfig、宿主协议测试和 GitHub Actions。
