# 贡献指南

## 基本要求

- 不要提交创想三维原厂固件、完整 Flash 备份、选项字节、设备序列号或其他专有二进制。
- 新结论必须区分“反汇编直接证据”“等价实现”和“尚待硬件验证”。
- 不得把编译成功描述成实机安全性证明。
- 修改协议、Flash、时钟或应用交接代码时，必须同时补充测试和中文文档。

## 开发检查

```sh
python3 -m pip install pytest kconfiglib
make profiles
make hosttest
python3 -m pytest -q -p no:cacheprovider
```

提交前请确认三个 BIN 均未超过 `0x3000`，板卡标识仍位于偏移 `0x2f80`。

## 硬件测试报告

报告应注明 MCU 完整型号、PCB 版本、晶振、连接 UART、供电条件、SWD 恢复条件、测试镜像哈希和可重复步骤。涉及擦写或断电测试时必须使用可恢复的试验板。
