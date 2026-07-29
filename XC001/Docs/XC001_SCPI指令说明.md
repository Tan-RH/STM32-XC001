# XC001 控制板 SCPI 指令说明

![GeneralTest Logo](公司logo.png)

生成日期：2026-07-29
固件版本：`V1.3.6`
适用对象：XC001 STM32H743 控制板

---

## 1. 设备与通信

### 1.1 设备信息

| 项目 | 当前值 |
|--|--|
| 制造商 | GeneralTest |
| 型号 | XC001 CONTROL-BOARD |
| MCU | STM32H743 |
| 软件版本 | `V1.3.6` |
| 默认 IP | `192.168.1.10` |
| 默认网关 | `192.168.1.1` |
| 默认子网掩码 | `255.255.255.0` |
| 默认 UDP SCPI 端口 | `4000` |
| HTTP 端口 | `80` |

### 1.2 通信方式

| 接口 | 参数 | 说明 |
|--|--|--|
| UART7 控制台 | `115200, 8N1` | Type-C 调试串口，直接发送 SCPI 文本命令 |
| UDP SCPI | 默认端口 `4000` | 向设备 IP 的 UDP 端口发送 SCPI 文本命令 |
| Web 页面 | `http://<设备IP>/` | 页面内可发送 SCPI、修改网络参数、上传升级固件 |
| HTTP JSON API | `POST /api` | 支持网页调用 `cmd.execute` 等接口 |

### 1.3 权限说明

| 操作类型 | 是否需要密码/密钥 | 说明 |
|--|--|--|
| UART7 本地 SCPI | 否 | 本地调试口默认可信 |
| 查询类 SCPI | 否 | 例如 `*IDN?`、`STAT?`、`NET:STAT?` |
| 设备网页 SCPI 控制 | 否 | 网页通过 JSON `cmd.execute` 调用，用于设备操作按钮和调试终端 |
| UDP 写入类 SCPI | 是 | 格式为 `AUTH <远程写入密钥>;<SCPI命令>` |
| 独立 HTTP `/api/cmd` 写入类 SCPI | 是 | 请求头使用 `X-XC001-Key: <远程写入密钥>` |
| 修改网络参数 | 是 | 网页/HTTP 网络配置密码为 `GTS` |
| 网页上传固件 `.bin` | 否 | 固件上传接口不要求密码 |

> 远程写入密钥由 MCU UID 派生，启动日志中会显示。`GTS` 只用于网络配置，不是所有写入类 SCPI 的通用密钥。

---

## 2. 系统与基础功能

用于确认设备在线、查询身份、读取综合状态、清除状态或重新初始化业务 IO。

| 指令 | 说明 | 返回示例 | 权限 |
|--|--|--|--|
| `*IDN?` / `IDN?` | 查询设备识别信息 | `XC001,CONTROL-BOARD,H743,V1.3.6` | 只读 |
| `PING?` | 通信链路测试 | `PONG` | 只读 |
| `STAT?` | 查询网络、CAN、RS485、SPI、UART7 综合状态 | `IP=...;CAN1:...;RS485:...;SPI5:...` | 只读 |
| `*CLS` | 清除当前状态/错误标志，当前实现返回 OK | `OK` | 写入 |
| `*RST` | 重新初始化板级业务 IO | `OK` | 写入 |
| `*STB?` | 查询状态字节，当前固定返回 0 | `0` | 只读 |
| `*OPC?` | 查询操作完成，当前固定返回 1 | `1` | 只读 |
| `*WAI` | 等待操作完成，当前返回 OK | `OK` | 只读 |
| `SYST:HELP?` | 查询固件支持的 SCPI 指令列表 | 逗号分隔指令列表 | 只读 |
| `SYST:CLOCK?` | 查询内核、总线时钟及功耗配置 | `CPU=240000000,HCLK=120000000,PCLK1=120000000,PCLK2=120000000,PROFILE=LOW_POWER` | 只读 |
| `SYST:ERR?` / `SYST:ERR:NEXT?` | 查询系统错误队列下一条错误 | `0,"No error"` | 只读 |
| `SYSTEM:ERROR?` / `SYSTEM:ERROR:NEXT?` | 兼容形式，等同系统错误查询 | `0,"No error"` | 只读 |
| `SYST:ERR:COUN?` / `SYST:ERR:COUNT?` | 查询系统错误数量 | `0` | 只读 |
| `SYSTEM:ERROR:COUNT?` | 兼容形式，查询系统错误数量 | `0` | 只读 |

### 示例

```scpi
*IDN?
PING?
STAT?
SYST:HELP?
SYST:CLOCK?
SYST:ERR?
```

典型返回：

```text
XC001,CONTROL-BOARD,H743,V1.3.6
PONG
CPU=240000000,HCLK=120000000,PCLK1=120000000,PCLK2=120000000,PROFILE=LOW_POWER
0,"No error"
```

### 2.1 状态指示灯

| 状态 | PB1 绿灯 | PB2 红灯 |
|--|--|--|
| 正常运行 | 按状态周期闪烁 | 熄灭，保持高电平 |
| SCPI 指令返回 `ERR,` | 暂停并熄灭 | 立即开始闪烁，持续 2 秒 |
| 指令错误提示结束 | 自动恢复闪烁 | 自动熄灭并恢复高电平 |

连续收到错误指令时，2 秒提示时间从最后一次错误重新计算。该提示是临时状态，不会改变硬件初始化故障标志。

---

## 3. 网络配置与诊断

用于查看当前 IP、网关、子网掩码、UDP 端口、PHY 链路以及以太网诊断信息。修改网络参数会保存到 Flash，重新上电或复位后生效。

| 指令 | 说明 | 返回示例 | 权限 |
|--|--|--|--|
| `NET:STAT?` | 查询当前运行网络配置 | `IP=192.168.1.10,GW=192.168.1.1,MASK=255.255.255.0,UDP=4000,HTTP=80` | 只读 |
| `NET:DIAG?` | 查询 LwIP、HTTP、PHY、DMA 诊断状态 | `NET:IP=...,UP=1,LINK=1,HTTP=1,...` | 只读 |
| `ND?` | `NET:DIAG?` 快捷别名 | `NET:IP=...,UP=1,LINK=1,...` | 只读 |
| `NET:PHY?` | 查询 LAN8742 PHY 初始化、地址、链路和寄存器信息 | `PHY:INIT=1,ADDR=0,LINK=...` | 只读 |
| `NET:MAC?` | 查询由 MCU UID 派生的 MAC 地址 | `02:xx:xx:xx:xx:xx` | 只读 |
| `NET:IP?` | 查询当前 IP 地址 | `192.168.1.10` | 只读 |
| `NET:IP a.b.c.d` | 保存新的 IP 地址到 Flash，重启后生效 | `OK,SAVED,REBOOT_REQUIRED` | 写入 |
| `NET:MASK?` | 查询当前子网掩码 | `255.255.255.0` | 只读 |
| `NET:MASK a.b.c.d` | 保存新的子网掩码到 Flash，重启后生效 | `OK,SAVED,REBOOT_REQUIRED` | 写入 |
| `NET:GATE?` | 查询当前网关 | `192.168.1.1` | 只读 |
| `NET:GATE a.b.c.d` | 保存新的网关到 Flash，重启后生效 | `OK,SAVED,REBOOT_REQUIRED` | 写入 |
| `NET:PORT?` | 查询 UDP SCPI 端口 | `4000` | 只读 |
| `NET:PORT n` | 保存新的 UDP SCPI 端口，范围 1~65535，重启后生效 | `OK,SAVED,REBOOT_REQUIRED` | 写入 |
| `NET:SERV ON` / `NET:SERV:ON` | 请求启动 HTTP/UDP 网络服务，当前固件已默认上电启动 | `OK,STARTING` | 写入 |

### 示例：查询网络状态

```scpi
NET:STAT?
ND?
NET:PHY?
NET:MAC?
```

### 示例：修改网络参数

```scpi
NET:IP 192.168.1.20
NET:MASK 255.255.255.0
NET:GATE 192.168.1.1
NET:PORT 4000
```

返回：

```text
OK,SAVED,REBOOT_REQUIRED
```

> 修改 IP、网关、子网掩码或 UDP 端口后，当前连接不会立即切换。请重启设备后使用新地址访问。

### 示例：UDP 远程写入鉴权

```text
AUTH XC1234567890ABCDEF;NET:PORT 4000
```

---

## 4. 射频 1 出 8 开关控制

用于控制 RFC 到 RF1~RF8 的导通路径。硬件控制线为：

| 信号 | MCU 引脚 |
|--|--|
| V1 | PF7 |
| V2 | PF8 |
| V3 | PF9 |

### 4.1 真值表

| 通道 | V1 | V2 | V3 | 导通路径 |
|--|--:|--:|--:|--|
| 1 | 0 | 0 | 0 | RFC TO RF1 |
| 2 | 0 | 0 | 1 | RFC TO RF2 |
| 3 | 0 | 1 | 0 | RFC TO RF3 |
| 4 | 0 | 1 | 1 | RFC TO RF4 |
| 5 | 1 | 0 | 0 | RFC TO RF5 |
| 6 | 1 | 0 | 1 | RFC TO RF6 |
| 7 | 1 | 1 | 0 | RFC TO RF7 |
| 8 | 1 | 1 | 1 | RFC TO RF8 |

### 4.2 指令表

| 指令 | 说明 | 返回示例 | 权限 |
|--|--|--|--|
| `ROUT:CHAN?` / `ROUTE:CHANNEL?` | 查询当前导通通道 | `1` | 只读 |
| `ROUT:STAT?` / `ROUTE:STATE?` | 查询当前通道、V1/V2/V3 电平和导通路径 | `CHAN=1,V1=0,V2=0,V3=0,PATH=RFC-RF1` | 只读 |
| `ROUT:CHAN n` / `ROUTE:CHANNEL n` | 设置导通通道，`n` 范围 1~8 | `OK,CHAN=3` | 写入 |
| `ROUT:CLOS (@n)` / `ROUTE:CLOSE (@n)` | 按 SCPI 开关语义闭合指定通道，等同于导通 RFn | `OK,CHAN=3` | 写入 |

### 示例：切换到 RF3

```scpi
ROUT:CHAN 3
ROUT:STAT?
```

返回：

```text
OK,CHAN=3
CHAN=3,V1=0,V2=1,V3=0,PATH=RFC-RF3
```

### 示例：使用 SCPI 开关语义

```scpi
ROUT:CLOS (@8)
ROUT:CHAN?
```

返回：

```text
OK,CHAN=8
8
```

---

## 5. 数字 IO 控制

用于控制扩展 GPIO、电平读取和调试输出。部分专用 IO 只读或不建议业务层直接写入。

### 5.1 支持的 GPIO 名称

```text
PE4,PE5,PE6,PC8,PC9,PC10,PC11,PC12,PC13,PA8,PA9,PD2,LED1,LED2,ETH_NRST,PF7,PF8,PF9,V1,V2,V3
```

说明：

| 名称 | 说明 |
|--|--|
| `PE4`、`PE5`、`PE6`、`PC8`~`PC13`、`PA8`、`PA9`、`PD2` | 扩展 IO，默认低电平 |
| `LED1` | PB1，运行状态指示灯 |
| `LED2` | PB2，错误状态指示灯，高电平熄灭，低电平点亮 |
| `ETH_NRST` | PE15，以太网复位/网络配置恢复输入，只读，不允许 `DIG:OUTP` 写入 |
| `PF7/PF8/PF9`、`V1/V2/V3` | 射频 1 出 8 开关控制线，建议优先使用 `ROUT` 指令控制 |

### 5.2 指令表

| 指令 | 说明 | 返回示例 | 权限 |
|--|--|--|--|
| `DIG:LIST?` | 列出固件识别的 GPIO 名称 | `PE4,PE5,...,V3` | 只读 |
| `DIG:OUTP? pin` | 读取指定 GPIO 当前电平 | `0` 或 `1` | 只读 |
| `DIG:OUTP pin,0` | 设置指定 GPIO 输出低电平 | `OK` | 写入 |
| `DIG:OUTP pin,1` | 设置指定 GPIO 输出高电平 | `OK` | 写入 |
| `DIG:OUTP pin,T` | 翻转指定 GPIO 输出电平 | `OK` | 写入 |

### 示例：控制 PE4

```scpi
DIG:OUTP PE4,1
DIG:OUTP? PE4
DIG:OUTP PE4,0
DIG:OUTP PE4,T
```

返回：

```text
OK
1
OK
OK
```

---

## 6. CAN 功能

用于 FDCAN1 状态查询、原始 CAN/CAN FD 帧收发，以及通过 A1 检波板 `0x11B` 协议查询 H/V 接收功率。

### 6.1 总线配置

| 项目 | XC001 当前配置 | 说明 |
|--|--|--|
| 控制器 | FDCAN1 | `PD0=RX`，`PD1=TX` |
| 帧格式 | Classic CAN + CAN FD | CAN FD 不启用 BRS |
| 标称波特率 | `2.5 Mbit/s` | 来源于当前 CubeMX 时钟和位时序；A1 检波板必须配置为相同波特率 |
| 标准 ID | 支持 | A1 协议使用 11 位标准 ID |
| 扩展 ID | 支持 | 仅用于通用 `CAN:SEND` |
| 数据长度 | `0~64` 字节 | `0~8` 字节发送 Classic CAN，超过 8 字节发送 CAN FD |

> 《V3.x A1检波板指令》未规定总线波特率，因此 XC001 沿用当前工程的 `2.5 Mbit/s`。如果 A1 固件采用其他波特率，两端必须统一后才能通信。

### 6.2 SCPI 指令

| 指令 | 说明 | 返回示例 | 权限 |
|--|--|--|--|
| `CAN:STAT?` | 查询 FDCAN1 模式、波特率、收发计数、功率查询成功和超时计数 | `CAN1:READY=1,MODE=FD_NO_BRS,BAUD=2500000,TX=1,RX=1,PWR_OK=1,PWR_TO=0` | 只读 |
| `CAN:RX?` | 查询最近一次收到的 CAN/CAN FD 帧 | `EMPTY` 或 `ID=0x1,LEN=8,DATA=1B 00 EE 55 ED F2 00 00` | 只读 |
| `CAN:SEND id,hex` | 发送原始帧；ID 最大 `0x1FFFFFFF`，数据 `0~64` 字节 | `OK` | 写入 |
| `MEASure:POWer? node,freq_kHz[,sample_ms,mode,threshold,timeout_ms,comp]` | 按 A1 `0x11B` 协议查询 H/V 接收功率；短形式为 `MEAS:POW?` | `NODE=1,FREQ=2400000,H=-45.23dBm,V=-46.22dBm,MODE=0,STATE=0` | 只读 |
| `CAN:POW? node,freq_kHz[,sample_ms,mode,threshold,timeout_ms,comp]` | `MEASure:POWer?` 的 CAN 业务别名 | 同上 | 只读 |

功率查询参数：

| 参数 | 范围 | 默认值 | 含义 |
|--|--|--|--|
| `node` | `1~40` | 无 | A1 检波板 Node ID；功率查询不允许广播 |
| `freq_kHz` | `1~16777215` | 无 | 查询频率，单位 kHz，按 24 位大端写入请求帧 |
| `sample_ms` | `1~255` | `20` | 采样时间，单位 ms |
| `mode` | `0~2` | `0` | `0=定时`，`1=触发`，`2=缓冲` |
| `threshold` | `0~65535` | `0` | 触发阈值，16 位大端；仅触发模式使用 |
| `timeout_ms` | `0~65535` | `1000` | 触发超时，单位 ms，16 位大端 |
| `comp` | `0` 或 `1` | `0` | `0=不使用天线接收补偿`，`1=使用补偿` |

### 6.3 接收功率查询示例

定时采样，使用默认参数：

```scpi
MEAS:POW? 1,2400000
```

典型返回：

```text
NODE=1,FREQ=2400000,H=-45.23dBm,V=-46.22dBm,MODE=0,STATE=0
```

触发采样，采样 50 ms、阈值 1200、触发超时 3000 ms，并启用天线接收补偿：

```scpi
MEASURE:POWER? 2,5800000,50,1,1200,3000,1
```

### 6.4 A1 `0x11B` 帧映射

XC001 发送标准 ID `0x11B`、DLC 12 的 CAN FD 帧，关闭 BRS：

| 字节 | 内容 |
|--|--|
| Byte0 | 目标 Node ID |
| Byte1~3 | 频率，kHz，24 位大端 |
| Byte4 | 采样时间，ms |
| Byte5 | 采样模式：`0=定时`、`1=触发`、`2=缓冲` |
| Byte6~7 | 触发阈值，16 位大端 |
| Byte8~9 | 触发超时，ms，16 位大端 |
| Byte10 Bit0 | 天线接收校准补偿使能 |
| Byte10 Bit1~7、Byte11 | 保留，发送 0 |

A1 使用标准 ID `Node ID` 返回 8 字节响应：

| 字节 | 内容 |
|--|--|
| Byte0 | 命令字 `0x1B` |
| Byte1 | 结果：`0=成功`，`1=采样失败/设备状态冲突`，`2=校准数据或参数非法` |
| Byte2~3 | H 功率，`dBm × 100`，`int16` 大端 |
| Byte4~5 | V 功率，`dBm × 100`，`int16` 大端 |
| Byte6 | 采样模式回显 |
| Byte7 | 采样状态：`0=完成`，`1=触发`，`2=超时` |

例如 `MEAS:POW? 1,2400000` 默认参数对应请求：

```text
ID=0x11B, FD=1, BRS=0, DATA=01 24 9F 00 14 00 00 00 03 E8 00 00
```

若返回如下帧：

```text
ID=0x001, DATA=1B 00 EE 55 ED F2 00 00
```

则 H 功率为 `-45.23 dBm`，V 功率为 `-46.22 dBm`。

### 6.5 原始帧示例

```scpi
CAN:STAT?
CAN:SEND 0x123,01020304
CAN:RX?
```

返回：

```text
CAN1:READY=1,MODE=FD_NO_BRS,BAUD=2500000,TX=0,RX=0,PWR_OK=0,PWR_TO=0
OK
EMPTY
```

---

## 7. RS485 功能

用于 UART8/RS485 文本发送、接收计数和最近接收文本查询。发送文本时固件会自动追加 CRLF。

| 指令 | 说明 | 返回示例 | 权限 |
|--|--|--|--|
| `RS485:STAT?` | 查询 RS485 发送/接收计数 | `RS485:TX=0,RX=0` | 只读 |
| `RS485:RX?` | 查询最近一行 RS485 接收文本 | `EMPTY` 或最近文本 | 只读 |
| `RS485:SEND text` | 通过 RS485 发送文本，并自动追加 CRLF | `OK` | 写入 |

### 示例：发送文本

```scpi
RS485:STAT?
RS485:SEND hello
RS485:RX?
```

返回：

```text
RS485:TX=0,RX=0
OK
EMPTY
```

---

## 8. SPI5 功能

当前硬件版本中，PF7/PF8/PF9 已用于射频 1 出 8 开关的 V1/V2/V3 控制线，因此 SPI5 应用层已禁用，避免和开关控制发生引脚复用冲突。

| 指令 | 说明 | 返回示例 | 权限 |
|--|--|--|--|
| `SPI:STAT?` | 查询 SPI5 状态 | `SPI5:READY=0,COUNT=0,DISABLED=PF7_PF8_PF9_RF_SWITCH` | 只读 |
| `SPI:TRAN? hex` | SPI5 传输接口保留；当前硬件版本返回失败 | `ERR,-222,"SPI transfer failed"` | 写入 |

### 示例

```scpi
SPI:STAT?
SPI:TRAN? 9F000000
```

---

## 9. Web 与固件升级

网页用于本地浏览器控制设备、发送 SCPI、修改网络参数和上传应用固件 `.bin`。

### 9.1 HTTP 接口

| 接口 | 方法 | 说明 |
|--|--|--|
| `/` | GET | 打开 XC001 网页控制台 |
| `/api` | POST | 网页 JSON API；`cmd.execute` 可直接执行 SCPI，网络设置仍需密码 |
| `/api/cmd` | POST | 请求体为 SCPI 文本，返回 SCPI 文本响应 |
| `/api/config` | POST | 保存 IP、网关、子网掩码、UDP 端口，需 `X-XC001-Key: GTS` |
| `/api/firmware/status` | GET | 查询固件上传/校验/重启安装状态 |
| `/firmware_upload` | POST | 上传主程序 `.bin` 文件 |

### 9.2 HTTP SCPI 示例

```http
POST /api/cmd HTTP/1.1
X-XC001-Key: XC1234567890ABCDEF
Content-Type: text/plain

ROUT:CHAN 3
```

### 9.3 修改网络参数示例

```http
POST /api/config HTTP/1.1
X-XC001-Key: GTS
Content-Type: application/x-www-form-urlencoded

ip=192.168.1.20&mask=255.255.255.0&gw=192.168.1.1&port=4000
```

### 9.4 固件升级说明

网页升级使用应用固件 `.bin` 文件：

```text
Debug/XC001_<版本号>.bin
```

不要上传：

- `.hex`
- `Bootloader.hex`
- `Factory.hex`

升级状态查询：

```http
GET /api/firmware/status
```

---

## 10. 参数格式

| 参数 | 格式 | 示例 |
|--|--|--|
| `a.b.c.d` | IPv4 地址，四段十进制 | `192.168.1.20` |
| `n` | 通道号或端口号；通道范围 1~8，端口范围 1~65535 | `3`、`4000` |
| `pin` | GPIO 名称，不区分大小写 | `PE4`、`LED1`、`V1` |
| `val` | `0`、`1` 或 `T` | `DIG:OUTP PE4,T` |
| `id` | CAN 标准/扩展 ID，十进制或 `0x` 十六进制 | `0x123` |
| `hex` | 连续十六进制字节，允许空格或逗号分隔；CAN 最大 64 字节 | `01020304`、`01 02 03 04` |
| `node` | A1 检波板 Node ID，功率查询范围 1~40 | `1` |
| `freq_kHz` | A1 接收功率查询频率，单位 kHz，最大 24 位无符号数 | `2400000` |
| `text` | 普通文本 | `RS485:SEND hello` |

---

## 11. 常见错误返回

| 返回 | 含义 |
|--|--|
| `ERR,-100,"Empty command"` | 空命令 |
| `ERR,-109,"Missing parameter"` | 缺少参数 |
| `ERR,-113,"Undefined header"` | 未识别的 SCPI 指令 |
| `ERR,-222,"Invalid ..."` | 参数非法或外设发送失败 |
| `ERR,-222,"Channel must be 1 to 8"` | 射频开关通道必须为 1~8 |
| `ERR,-224,"Unknown GPIO"` | GPIO 名称不存在或不可写 |
| `ERR,-300,"Command executor unavailable"` | SCPI 执行器不可用 |
| `ERR,-300,"CAN not ready"` | FDCAN1 尚未成功初始化 |
| `ERR,-410,"CAN power response timeout"` | 已发送 `0x11B`，但等待时间内未收到匹配的 A1 响应 |
| `ERR,-221,"CAN power query busy"` | 已有一个接收功率查询正在执行 |
| `ERR,-222,"A1 status 1"` | A1 采样失败，或处于 OTA、校准保存、SN 写入状态 |
| `ERR,-222,"A1 status 2"` | A1 校准数据无效/未写入，或请求参数非法 |
