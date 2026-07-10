## XC001 控制板 SCPI 指令说明

![GeneralTest Logo](公司logo.png)

---

## 设备信息

|项目|说明|
|--|--|
|制造商|GeneralTest|
|型号|XC001 CONTROL-BOARD|
|MCU|STM32H743|
|固件版本|V1.0|
|默认 IP|192.168.1.10|
|默认 UDP 端口|4000|
|HTTP 端口|80|

## 通信方式

|接口|参数|说明|
|--|--|--|
|UART7 控制台|115200, 8N1|Type-C 调试串口，发送 SCPI 文本指令并接收文本返回|
|UDP SCPI|默认端口 `4000`|向板卡 IP 的 UDP 端口发送 SCPI 文本指令|
|Web 页面|`http://<板卡IP>/`|网页内可发送 SCPI 指令，也可配置网络参数|

> 默认网络参数：IP `192.168.1.10`，子网掩码 `255.255.255.0`，网关 `192.168.1.1`。
> 网页或 SCPI 修改 IP、子网掩码、网关、UDP 端口后会保存到 Flash，重新上电/重启后生效。

## 基础指令

|指令|参数|说明|
|--|--|--|
|`*IDN?`||查询设备身份信息|
|`IDN?`||兼容形式，等同于 `*IDN?`|
|`PING?`||通信链路测试|
|`*CLS`||清除当前状态/错误标志|
|`*RST`||重新初始化板级业务 IO|
|`STAT?`||查询网络、CAN、RS485、SPI 综合状态|

### 示例

```scpi
*IDN?      # 查询设备身份
PING?      # 通信测试
STAT?      # 查询综合状态
*RST       # 重新初始化业务 IO
```

### 返回示例

```text
XC001,CONTROL-BOARD,H743,V1.0
PONG
OK
```

## 系统指令

|指令|参数|说明|
|--|--|--|
|`SYST:ERR?`||查询系统错误状态|
|`SYST:HELP?`||查询当前固件支持的 SCPI 指令列表|

### 示例

```scpi
SYST:ERR?     # 查询系统错误
SYST:HELP?    # 查询支持的指令
```

### 系统错误返回值

|返回值|说明|
|--|--|
|`0,"No error"`|当前无错误|
|`ERR,-100,"Empty command"`|空指令|
|`ERR,-109,"Missing parameter"`|缺少参数|
|`ERR,-113,"Undefined header"`|未知指令|
|`ERR,-222,"Invalid ..."`|参数非法或操作失败|
|`ERR,-224,"Unknown GPIO"`|未知 GPIO 名称|

## 网络配置指令

|指令|参数|说明|
|--|--|--|
|`NET:STAT?`||查询当前运行中的网络配置|
|`NET:DIAG?`||查询网络、PHY、HTTP、DMA 诊断状态|
|`ND?`||`NET:DIAG?` 快捷别名|
|`NET:PHY?`||查询以太网 PHY 寄存器和链路状态|
|`NET:MAC?`||查询当前 MAC 地址|
|`NET:SERV ON`||请求启动 HTTP/UDP 网络服务；当前固件默认上电自动启动|
|`NET:IP?`||查询当前运行 IP 地址|
|`NET:IP`|`a.b.c.d`|设置 IP 地址并保存到 Flash，重启后生效|
|`NET:MASK?`||查询当前运行子网掩码|
|`NET:MASK`|`a.b.c.d`|设置子网掩码并保存到 Flash，重启后生效|
|`NET:GATE?`||查询当前运行网关|
|`NET:GATE`|`a.b.c.d`|设置网关并保存到 Flash，重启后生效|
|`NET:PORT?`||查询 UDP SCPI 端口|
|`NET:PORT`|`1~65535`|设置 UDP SCPI 端口并保存到 Flash，重启后生效|

### 示例

```scpi
NET:STAT?                  # 查询网络配置
NET:DIAG?                  # 查询网络诊断
ND?                        # 快捷查询网络诊断
NET:PHY?                   # 查询 PHY 状态
NET:IP?                    # 查询 IP
NET:IP 192.168.1.20        # 设置 IP，重启后生效
NET:MASK 255.255.255.0     # 设置子网掩码，重启后生效
NET:GATE 192.168.1.1       # 设置网关，重启后生效
NET:PORT 4000              # 设置 UDP 端口，重启后生效
```

### 返回示例

```text
IP=192.168.1.10,GW=192.168.1.1,MASK=255.255.255.0,UDP=4000,HTTP=80
OK,SAVED,REBOOT_REQUIRED
NET:IP=192.168.1.10,UP=1,LINK=1,HTTP=1,HTTP_ACC=2,HTTP_REQ=2,...
PHY:INIT=1,ADDR=0,LINK=...
```

### 网络参数恢复

|信号|说明|
|--|--|
|`ETH_NRST` / `PE15`|低电平有效。上电时保持低电平会清除 Flash 中的网络配置并恢复默认 IP、子网掩码、网关和端口。|

## 数字 IO 指令

|指令|参数|说明|
|--|--|--|
|`DIG:LIST?`||列出固件识别的 GPIO 名称|
|`DIG:OUTP`|`pin,val`|设置指定 IO 输出。`val=0` 输出低电平，`val=1` 输出高电平，`val=T` 翻转|
|`DIG:OUTP?`|`pin`|读取指定 IO 当前电平|

### 支持的 GPIO 名称

|名称|说明|
|--|--|
|`PE3` / `PE4` / `PE5` / `PE6`|扩展 IO，默认低电平|
|`PC8` / `PC9` / `PC10` / `PC11` / `PC12` / `PC13`|扩展 IO，默认低电平|
|`PA8` / `PA9` / `PD2`|扩展 IO，默认低电平|
|`LED1`|PB1，运行状态指示灯|
|`LED2`|PB2，错误状态指示灯，高电平熄灭，低电平点亮|
|`ETH_NRST`|PE15，以太网复位/网络恢复输入，不允许通过 `DIG:OUTP` 写入|

### 示例

```scpi
DIG:LIST?           # 查询 GPIO 名称列表
DIG:OUTP PE4,1      # PE4 输出高电平
DIG:OUTP PE4,0      # PE4 输出低电平
DIG:OUTP PE4,T      # PE4 翻转
DIG:OUTP? PE4       # 查询 PE4 当前电平
```

### 返回示例

```text
PE3,PE4,PE5,PE6,PC8,PC9,PC10,PC11,PC12,PC13,PA8,PA9,PD2,LED1,LED2,ETH_NRST
OK
0
1
```

## CAN 指令

|指令|参数|说明|
|--|--|--|
|`CAN:STAT?`||查询 FDCAN1 初始化状态、发送计数和接收计数|
|`CAN:SEND`|`id,hex`|发送 CAN 数据帧。`id` 支持十进制或 `0x` 前缀，`hex` 为 0~8 字节十六进制数据|
|`CAN:RX?`||查询最近一次收到的 CAN 帧|

### 示例

```scpi
CAN:STAT?                  # 查询 CAN 状态
CAN:SEND 0x123,11223344    # 发送 CAN 帧
CAN:RX?                    # 查询最近一次接收帧
```

### 返回示例

```text
CAN1:READY=1,TX=1,RX=0
OK
ID=0x123,LEN=4,DATA=11223344
EMPTY
```

## RS485 指令

|指令|参数|说明|
|--|--|--|
|`RS485:STAT?`||查询 RS485 发送/接收计数|
|`RS485:SEND`|`text`|通过 RS485 发送文本，固件自动追加 CRLF|
|`RS485:RX?`||查询最近一次 RS485 接收文本|

### 示例

```scpi
RS485:STAT?             # 查询 RS485 状态
RS485:SEND hello        # 发送文本
RS485:RX?               # 查询最近一次接收文本
```

### 返回示例

```text
RS485:TX=1,RX=0
OK
EMPTY
```

## SPI 指令

|指令|参数|说明|
|--|--|--|
|`SPI:STAT?`||查询 SPI5 初始化状态和传输计数|
|`SPI:TRAN?`|`hex`|通过 SPI5 发送十六进制数据，并返回同步接收数据|

### 示例

```scpi
SPI:STAT?            # 查询 SPI 状态
SPI:TRAN? 9F0000     # 发送 3 字节并读取返回
```

### 返回示例

```text
SPI5:READY=1,COUNT=0
FFFFFF
```

## Web HTTP 接口

|接口|方法|说明|
|--|--|--|
|`/`|GET|打开 XC001 网页控制台|
|`/api/cmd`|POST|请求体为 SCPI 指令文本，返回 SCPI 文本响应|
|`/api/config`|GET|无参数时读取当前运行网络配置|
|`/api/config?ip=<ip>&mask=<mask>&gw=<gw>&port=<port>&pwd=<pwd>`|GET|保存网络参数到 Flash，默认密码为 `admin`，重启后生效|

### 示例

```http
POST /api/cmd

*IDN?
```

```http
GET /api/config?ip=192.168.1.20&mask=255.255.255.0&gw=192.168.1.1&port=4000&pwd=admin
```

## 指示灯状态

|引脚|状态|说明|
|--|--|--|
|PB1 / LED1|周期闪烁|运行状态指示灯。闪烁周期由 `XC001_STATUS_BLINK_MS` 配置。|
|PB2 / LED2|高电平熄灭，低电平点亮|错误状态指示灯。正常状态保持高电平熄灭。|

## 注意事项

1. SCPI 指令大小写不敏感。
2. 串口指令建议以回车或换行结束。
3. 网页和 UDP 通道均调用同一套 SCPI 解析逻辑。
4. 修改网络参数后，当前运行网络不会立即切换；请重新上电或复位后使用新地址访问。
5. 设置类指令若参数非法，通常返回 `ERR,-222,"Invalid ..."`。
