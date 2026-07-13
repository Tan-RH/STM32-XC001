## XC001 控制板 SCPI 指令说明

![GeneralTest Logo](公司logo.png)

---

## 设备信息

|项目|说明|
|--|--|
|制造商|GeneralTest|
|型号|XC001 CONTROL-BOARD|
|MCU|STM32H743|
|软件版本|V1.1.0|
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

### 远程写入鉴权

- 每块控制板使用由 MCU UID 派生的唯一远程写入密钥，密钥会在 UART7 启动日志中显示。
- UART7 是本地可信控制台，不要求密钥。
- HTTP 查询类指令可以直接调用；设置或外设操作类指令必须通过 `X-XC001-Key` 请求头提供密钥。
- UDP 查询类指令可以直接发送；写入类指令格式为 `AUTH <密钥>;<SCPI 指令>`。
- HTTP 端口是明文协议。生产网络仍应使用 VLAN、管理网或上位机网关进行隔离，不应直接暴露到互联网。

```text
AUTH XC1234567890ABCDEF;DIG:OUTP PE4,1
```

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
XC001,CONTROL-BOARD,H743,V1.1.0
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
|`PE4` / `PE5` / `PE6`|扩展 IO，默认低电平|
|`PE3`|RS485 方向控制专用，不对 `DIG:OUTP` 开放|
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
PE4,PE5,PE6,PC8,PC9,PC10,PC11,PC12,PC13,PA8,PA9,PD2,LED1,LED2,ETH_NRST
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
|`/api/cmd`|POST|请求体为 SCPI 指令文本；写入类指令需要 `X-XC001-Key` 请求头|
|`/api/config`|GET|无参数时读取当前运行网络配置|
|`/api/config`|POST|表单正文包含 `ip`、`mask`、`gw`、`port`，并通过 `X-XC001-Key` 请求头鉴权；保存后重启生效|
|`/api/firmware`|POST|请求体为主程序 `.bin` 原始内容，使用 `X-XC001-Key` 鉴权；校验成功后自动重启升级|
|`/api/firmware/status`|GET|读取设备端升级阶段、百分比、已接收字节数、文件总字节数和当前软件版本|

### 示例

```http
POST /api/cmd
X-XC001-Key: XC1234567890ABCDEF

DIG:OUTP PE4,1
```

```http
POST /api/config
X-XC001-Key: XC1234567890ABCDEF
Content-Type: application/x-www-form-urlencoded

ip=192.168.1.20&mask=255.255.255.0&gw=192.168.1.1&port=4000
```

## 网页固件升级

网页首页显示当前软件版本，并提供 `.bin` 文件选择、设备端实时进度和升级状态。进度依次显示暂存区擦除、接收并写入、固件校验以及重启安装阶段。

### 首次安装

网页升级依赖两个常驻组件。首次启用时，推荐通过 ST-Link 等外部下载器直接烧录：

- `Bootloader/build/XC001_Factory.hex`

该 Factory HEX 已合并 Stage0、主程序和 Recovery Bootloader，因此不需要再单独烧录 `bootloader.hex`。只有需要分区调试或手动烧录时，才分别烧录：

新板首次烧录步骤：

1. 通过 SWD 连接 ST-Link，并给目标板正常供电。
2. 打开 STM32CubeProgrammer，选择 ST-Link 并连接目标 MCU。
3. 对全新板执行一次 Full chip erase（整片擦除）。
4. 打开 `Bootloader/build/XC001_Factory.hex`，勾选下载后校验并执行 Download。
5. 校验成功后复位或重新上电，通过 UART7 启动日志确认版本、设备写入密钥和网络初始化状态。
6. 使用默认地址 `192.168.1.10` 访问网页；若电脑不在同一网段，先临时配置同网段静态 IP。

如果不使用 Factory HEX，分区调试时才按以下三个文件分别烧录：

1. `Bootloader/build/XC001_Stage0.hex`，地址 `0x08000000`。
2. `Bootloader/build/XC001_Recovery.hex`，地址 `0x081E0000`。
3. `Bootloader/build/XC001_Application_V1.1.0.hex`（或当前版本的应用 HEX），链接地址为 `0x08020000`。

常驻组件可通过以下命令构建：

```powershell
make -C Bootloader all
```

完成首次安装后，不要再使用“整片擦除”，否则会删除 Stage0 和 Recovery Bootloader。

### 网页升级步骤

1. 使用当前工程生成主程序 `.bin`，文件大小不得超过 655360 bytes。
2. 打开设备网页，确认页面上的当前软件版本。
3. 在“固件升级”区域输入 UART7 启动日志中的设备写入密钥。
4. 选择主程序 `.bin`，点击“上传并升级”。
5. 网页实时显示设备擦除、接收/写入、校验和重启状态；设备重新联网后页面显示当前版本号。

升级文件会先写入 Bank2 暂存区，并检查长度、向量表和 CRC32。断电发生在上传或主程序复制期间时，Stage0 会保留恢复入口，下一次上电由 Recovery Bootloader 重新执行复制。

设备重启后，Recovery Bootloader 将暂存固件复制到主程序区。此时网络服务尚未启动，网页会显示“设备正在重启并安装”，无法提供复制过程中的逐字节进度；主程序启动并恢复网络后，网页会自动确认设备上线和版本号。

> 旧版链接地址为 `0x08000000` 的 `.bin` 会被拒绝。只能上传启用本升级布局后重新构建的主程序 `.bin`。

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
6. 文档中的 `XC1234567890ABCDEF` 仅为格式示例，实际密钥以设备 UART7 启动日志为准。
7. 在线升级目前提供完整性校验和断电恢复，但不包含数字签名；生产环境仍应限制升级密钥和管理网络访问权限。
