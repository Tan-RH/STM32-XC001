# -*- coding: utf-8 -*-
from __future__ import annotations

import html
import re
import shutil
import subprocess
from datetime import date
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "Docs"
CONFIG = ROOT / "Core" / "Inc" / "xc001_config.h"

MD_PATH = DOCS / "XC001_SCPI指令说明.md"
HTML_PATH = DOCS / "XC001_SCPI指令说明.html"
PDF_PATH = DOCS / "XC001_SCPI指令说明.pdf"


def macro(name: str, default: str = "") -> str:
    text = CONFIG.read_text(encoding="utf-8")
    match = re.search(rf'^\s*#define\s+{re.escape(name)}\s+(.+?)\s*$', text, re.M)
    if not match:
        return default
    value = match.group(1).strip()
    if value.startswith('"') and value.endswith('"'):
        return value[1:-1]
    return value.rstrip("U")


def default_ip(prefix: str) -> str:
    return ".".join(macro(f"{prefix}{i}", "0") for i in range(4))


COMMANDS = [
    ("基础", "`*IDN?` / `IDN?`", "查询设备识别信息。", "`XC001,CONTROL-BOARD,H743,Vx.x.x`", "只读"),
    ("基础", "`PING?`", "通信链路测试。", "`PONG`", "只读"),
    ("基础", "`*CLS`", "清除当前状态/错误标志；当前实现返回 OK。", "`OK`", "写入"),
    ("基础", "`*RST`", "重新初始化板级业务 IO。", "`OK`", "写入"),
    ("基础", "`*STB?`", "查询状态字节；当前实现固定返回 0。", "`0`", "只读"),
    ("基础", "`*OPC?`", "查询操作完成；当前实现固定返回 1。", "`1`", "只读"),
    ("基础", "`*WAI`", "等待操作完成；当前实现返回 OK。", "`OK`", "只读"),
    ("系统", "`SYST:ERR?` / `SYST:ERR:NEXT?`", "查询系统错误队列下一条错误。", '`0,"No error"`', "只读"),
    ("系统", "`SYSTEM:ERROR?` / `SYSTEM:ERROR:NEXT?`", "兼容形式，等同系统错误查询。", '`0,"No error"`', "只读"),
    ("系统", "`SYST:ERR:COUN?` / `SYST:ERR:COUNT?`", "查询系统错误数量。", "`0`", "只读"),
    ("系统", "`SYSTEM:ERROR:COUNT?`", "兼容形式，查询系统错误数量。", "`0`", "只读"),
    ("系统", "`SYST:HELP?`", "返回当前固件支持的 SCPI 指令列表。", "逗号分隔命令列表", "只读"),
    ("系统", "`STAT?`", "查询网络、CAN、RS485、SPI、UART7 接收溢出等综合状态。", "`IP=...;CAN1:...;RS485:...;SPI5:...;UART7:...`", "只读"),
    ("网络", "`NET:STAT?`", "查询当前 IP、网关、子网掩码、UDP 端口、HTTP 端口。", "`IP=...,GW=...,MASK=...,UDP=4000,HTTP=80`", "只读"),
    ("网络", "`NET:DIAG?`", "查询 LwIP、HTTP、PHY、以太网 DMA 等诊断信息。", "`NET:IP=...,UP=...,LINK=...`", "只读"),
    ("网络", "`ND?`", "`NET:DIAG?` 的快捷别名。", "`NET:IP=...,UP=...,LINK=...`", "只读"),
    ("网络", "`NET:PHY?`", "查询 LAN8742 PHY 初始化、地址、链路和寄存器信息。", "`PHY:INIT=...,ADDR=...,LINK=...`", "只读"),
    ("网络", "`NET:MAC?`", "查询由 MCU UID 派生的本机 MAC 地址。", "`02:xx:xx:xx:xx:xx`", "只读"),
    ("网络", "`NET:IP?`", "查询当前运行 IP 地址。", "`192.168.1.10`", "只读"),
    ("网络", "`NET:IP a.b.c.d`", "保存新的 IP 地址到 Flash，重启后生效。", "`OK,SAVED,REBOOT_REQUIRED`", "写入"),
    ("网络", "`NET:MASK?`", "查询当前子网掩码。", "`255.255.255.0`", "只读"),
    ("网络", "`NET:MASK a.b.c.d`", "保存新的子网掩码到 Flash，重启后生效。", "`OK,SAVED,REBOOT_REQUIRED`", "写入"),
    ("网络", "`NET:GATE?`", "查询当前网关。", "`192.168.1.1`", "只读"),
    ("网络", "`NET:GATE a.b.c.d`", "保存新的网关到 Flash，重启后生效。", "`OK,SAVED,REBOOT_REQUIRED`", "写入"),
    ("网络", "`NET:PORT?`", "查询 UDP SCPI 端口。", "`4000`", "只读"),
    ("网络", "`NET:PORT n`", "保存新的 UDP SCPI 端口到 Flash，范围 1~65535，重启后生效。", "`OK,SAVED,REBOOT_REQUIRED`", "写入"),
    ("网络", "`NET:SERV ON` / `NET:SERV:ON`", "请求启动 HTTP/UDP 网络服务。", "`OK,STARTING`", "写入"),
    ("数字 IO", "`DIG:LIST?`", "列出固件识别的可读 GPIO 名称。", "`PE4,PE5,...,ETH_NRST`", "只读"),
    ("数字 IO", "`DIG:OUTP? pin`", "读取指定 GPIO 当前电平。", "`0` 或 `1`", "只读"),
    ("数字 IO", "`DIG:OUTP pin,val`", "设置 GPIO 输出；`val=0/1` 设置电平，`val=T` 翻转。", "`OK`", "写入"),
    ("CAN", "`CAN:STAT?`", "查询 FDCAN1 初始化状态、发送计数、接收计数。", "`CAN1:READY=1,TX=0,RX=0`", "只读"),
    ("CAN", "`CAN:RX?`", "查询最近一次收到的 CAN 帧。", "`EMPTY` 或 `ID=0x123,LEN=8,DATA=...`", "只读"),
    ("CAN", "`CAN:SEND id,hex`", "发送 Classic CAN 数据帧；ID 最大 `0x1FFFFFFF`，数据 0~8 字节十六进制。", "`OK`", "写入"),
    ("RS485", "`RS485:STAT?`", "查询 RS485 发送/接收计数。", "`RS485:TX=0,RX=0`", "只读"),
    ("RS485", "`RS485:RX?`", "查询最近一行 RS485 接收文本。", "`EMPTY` 或最近文本", "只读"),
    ("RS485", "`RS485:SEND text`", "通过 UART8/RS485 发送文本，并自动追加 CRLF。", "`OK`", "写入"),
    ("SPI", "`SPI:STAT?`", "查询 SPI5 初始化状态和传输计数。", "`SPI5:READY=1,COUNT=0`", "只读"),
    ("SPI", "`SPI:TRAN? hex`", "通过 SPI5 进行一次全双工传输；最长 32 字节。", "返回接收字节十六进制", "写入"),
]


def command_table(rows):
    lines = ["| 分类 | 指令 | 说明 | 返回示例 | 权限 |", "|--|--|--|--|--|"]
    lines.extend(f"| {cat} | {cmd} | {desc} | {reply} | {access} |" for cat, cmd, desc, reply, access in rows)
    return "\n".join(lines)


def build_markdown() -> str:
    version = macro("XC001_SOFTWARE_VERSION", "unknown")
    ip = default_ip("XC001_DEFAULT_IP")
    gw = default_ip("XC001_DEFAULT_GW")
    mask = default_ip("XC001_DEFAULT_MASK")
    udp = macro("XC001_DEFAULT_UDP_PORT", "4000")
    http_port = macro("XC001_HTTP_PORT", "80")

    return f"""# XC001 控制板 SCPI 指令说明

生成日期：{date.today().isoformat()}  
固件版本：`{version}`  
适用对象：XC001 STM32H743 控制板

---

## 1. 设备信息

| 项目 | 当前值 |
|--|--|
| 制造商 | GeneralTest |
| 型号 | XC001 CONTROL-BOARD |
| MCU | STM32H743 |
| 软件版本 | `{version}` |
| 默认 IP | `{ip}` |
| 默认网关 | `{gw}` |
| 默认子网掩码 | `{mask}` |
| 默认 UDP SCPI 端口 | `{udp}` |
| HTTP 端口 | `{http_port}` |

## 2. 通信方式

| 接口 | 参数 | 说明 |
|--|--|--|
| UART7 控制台 | `115200, 8N1` | 本地调试串口，直接发送 SCPI 文本命令 |
| UDP SCPI | 默认端口 `{udp}` | 向 PCB IP 的 UDP 端口发送 SCPI 文本命令 |
| Web 页面 | `http://<PCB_IP>/` | 页面内可发送 SCPI、查看版本、修改网络参数、上传升级固件 |
| Chaos JSON API | `POST /api` | 支持 `info.get`、`cmd.execute`、`config.network.get/set` 等方法 |

## 3. 权限与密码

| 操作 | 是否需要密码/密钥 | 说明 |
|--|--|--|
| 查询类 SCPI | 否 | 例如 `*IDN?`、`STAT?`、`NET:STAT?` |
| 网页上传固件 `.bin` | 否 | 固件升级上传接口不要求密码 |
| 修改 IP/网关/子网掩码/UDP 端口 | 是 | 密码为 `GTS`，修改后需重启生效 |
| 写入类 SCPI 远程执行 | 是 | HTTP 使用 `X-XC001-Key`，UDP 使用 `AUTH <远程写入密钥>;<命令>` |
| UART7 本地控制台 | 否 | 本地串口默认视为可信调试通道 |

> 注意：`GTS` 是网络配置密码，不是所有写入类 SCPI 的通用远程密钥。写入类 SCPI 的远程密钥由设备 UID 派生，启动日志中会显示。

UDP 写入类 SCPI 示例：

```text
AUTH XC1234567890ABCDEF;DIG:OUTP PE4,1
```

HTTP 写入类 SCPI 示例：

```http
POST /api/cmd HTTP/1.1
X-XC001-Key: XC1234567890ABCDEF
Content-Type: text/plain

DIG:OUTP PE4,1
```

修改网络参数示例：

```http
POST /api/config HTTP/1.1
X-XC001-Key: GTS
Content-Type: application/x-www-form-urlencoded

ip=192.168.1.20&mask=255.255.255.0&gw=192.168.1.1&port=4000
```

## 4. 指令总表

{command_table(COMMANDS)}

## 5. 参数格式说明

| 参数 | 格式 | 示例 |
|--|--|--|
| `a.b.c.d` | IPv4 地址，四段十进制 | `192.168.1.20` |
| `n` | 十进制端口号，范围 1~65535 | `4000` |
| `pin` | GPIO 名称，不区分大小写 | `PE4`、`LED1` |
| `val` | `0`、`1` 或 `T` | `DIG:OUTP PE4,T` |
| `id` | CAN 标准/扩展 ID，十进制或 `0x` 十六进制 | `0x123` |
| `hex` | 连续十六进制字节，允许空格分隔 | `01020304` 或 `01 02 03 04` |
| `text` | 普通文本 | `RS485:SEND hello` |

## 6. GPIO 名称

当前固件识别的 GPIO：

```text
PE4,PE5,PE6,PC8,PC9,PC10,PC11,PC12,PC13,PA8,PA9,PD2,LED1,LED2,ETH_NRST
```

其中 `ETH_NRST` 可读但不可通过 `DIG:OUTP` 写入。

## 7. 常用示例

### 7.1 查询设备和状态

```scpi
*IDN?
PING?
STAT?
NET:STAT?
ND?
```

### 7.2 修改 IP 地址

通过 SCPI 修改：

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

修改后重新上电或复位生效。

### 7.3 控制数字 IO

```scpi
DIG:LIST?
DIG:OUTP PE4,1
DIG:OUTP PE4,0
DIG:OUTP PE4,T
DIG:OUTP? PE4
```

### 7.4 CAN 发送与接收

```scpi
CAN:STAT?
CAN:SEND 0x123,01020304
CAN:RX?
```

### 7.5 RS485 发送与接收

```scpi
RS485:STAT?
RS485:SEND hello
RS485:RX?
```

### 7.6 SPI5 全双工传输

```scpi
SPI:STAT?
SPI:TRAN? 9F000000
```

## 8. 常见错误返回

| 返回 | 含义 |
|--|--|
| `ERR,-100,"Empty command"` | 空命令 |
| `ERR,-109,"Missing parameter"` | 缺少参数 |
| `ERR,-113,"Undefined header"` | 未识别的 SCPI 指令 |
| `ERR,-222,"Invalid ..."` | 参数非法或外设发送失败 |
| `ERR,-224,"Unknown GPIO"` | GPIO 名称不存在或不可写 |
| `ERR,-300,"Command executor unavailable"` | SCPI 执行器不可用 |

## 9. 网页升级固件说明

网页升级使用应用固件 `.bin` 文件：

```text
Debug/XC001_<版本号>.bin
```

不要上传：

- `.hex`
- `Bootloader.hex`
- `Factory.hex`

升级状态接口：

```http
GET /api/firmware/status
```

网页会显示上传/校验/重启安装状态。
"""


def inline_md(text: str) -> str:
    parts = text.split("`")
    out = []
    for index, part in enumerate(parts):
        if index % 2:
            out.append(f"<code>{html.escape(part)}</code>")
        else:
            out.append(html.escape(part))
    return "".join(out)


def split_row(line: str) -> list[str]:
    line = line.strip()
    if line.startswith("|"):
        line = line[1:]
    if line.endswith("|"):
        line = line[:-1]
    return [cell.strip() for cell in line.split("|")]


def is_sep(line: str) -> bool:
    cells = split_row(line)
    return bool(cells) and all(set(cell.replace(":", "").strip()) <= {"-"} and "-" in cell for cell in cells)


def md_to_html(md: str) -> str:
    lines = md.splitlines()
    blocks: list[str] = []
    i = 0
    in_code = False
    code: list[str] = []
    code_lang = ""

    while i < len(lines):
        line = lines[i].rstrip()
        stripped = line.strip()

        if stripped.startswith("```"):
            if not in_code:
                in_code = True
                code = []
                code_lang = stripped[3:].strip()
            else:
                in_code = False
                cls = f' class="language-{html.escape(code_lang)}"' if code_lang else ""
                blocks.append(f"<pre><code{cls}>{html.escape(chr(10).join(code))}</code></pre>")
            i += 1
            continue

        if in_code:
            code.append(line)
            i += 1
            continue

        if stripped.startswith("|") and i + 1 < len(lines) and is_sep(lines[i + 1]):
            rows = [split_row(line)]
            i += 2
            while i < len(lines) and lines[i].strip().startswith("|"):
                rows.append(split_row(lines[i]))
                i += 1
            blocks.append("<table><thead><tr>" + "".join(f"<th>{inline_md(c)}</th>" for c in rows[0]) + "</tr></thead><tbody>")
            for row in rows[1:]:
                blocks.append("<tr>" + "".join(f"<td>{inline_md(c)}</td>" for c in row) + "</tr>")
            blocks.append("</tbody></table>")
            continue

        if not stripped:
            i += 1
            continue
        if stripped == "---":
            blocks.append("<hr>")
        elif stripped.startswith("# "):
            blocks.append(f"<h1>{inline_md(stripped[2:])}</h1>")
        elif stripped.startswith("## "):
            blocks.append(f"<h2>{inline_md(stripped[3:])}</h2>")
        elif stripped.startswith("### "):
            blocks.append(f"<h3>{inline_md(stripped[4:])}</h3>")
        elif stripped.startswith("> "):
            blocks.append(f"<blockquote>{inline_md(stripped[2:])}</blockquote>")
        elif stripped.startswith("- "):
            items = []
            while i < len(lines) and lines[i].strip().startswith("- "):
                items.append(f"<li>{inline_md(lines[i].strip()[2:])}</li>")
                i += 1
            blocks.append("<ul>" + "".join(items) + "</ul>")
            continue
        else:
            blocks.append(f"<p>{inline_md(stripped)}</p>")
        i += 1

    return "\n".join(blocks)


def write_html(md: str) -> None:
    body = md_to_html(md)
    html_text = f"""<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<title>XC001 SCPI 指令说明</title>
<style>
@page {{ size: A4; margin: 14mm 12mm 16mm; }}
body {{ font-family: "Microsoft YaHei", "Noto Sans SC", Arial, sans-serif; color:#1b2630; line-height:1.55; font-size:13px; }}
h1 {{ color:#102334; font-size:28px; margin:0 0 10px; }}
h2 {{ color:#142536; font-size:21px; margin:20px 0 10px; border-bottom:2px solid #d9e1ea; padding-bottom:5px; break-after:avoid; }}
h3 {{ color:#1f3a54; font-size:16px; margin:14px 0 8px; break-after:avoid; }}
p {{ margin:7px 0; }}
hr {{ border:0; border-top:1px solid #d9e1ea; margin:14px 0; }}
table {{ width:100%; border-collapse:collapse; margin:8px 0 14px; break-inside:auto; }}
tr {{ break-inside:avoid; break-after:auto; }}
th {{ background:#142536; color:white; font-weight:600; }}
th,td {{ border:1px solid #bfc8d2; padding:6px 7px; vertical-align:top; word-break:break-word; }}
code {{ font-family: Consolas, "Microsoft YaHei", monospace; background:#eef2f5; padding:1px 4px; border-radius:3px; }}
pre {{ background:#101820; color:#dff3ff; padding:10px 12px; border-radius:6px; white-space:pre-wrap; break-inside:avoid; }}
pre code {{ background:transparent; color:inherit; padding:0; }}
blockquote {{ margin:8px 0; padding:7px 10px; border-left:4px solid #0f62a8; background:#f2f6fa; color:#4b5563; }}
ul {{ padding-left:24px; }}
</style>
</head>
<body>
{body}
</body>
</html>
"""
    HTML_PATH.write_text(html_text, encoding="utf-8")


def find_browser() -> str | None:
    candidates = [
        r"C:\Program Files\Google\Chrome\Application\chrome.exe",
        r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe",
        r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
    ]
    for candidate in candidates:
        if Path(candidate).is_file():
            return candidate
    return shutil.which("chrome") or shutil.which("msedge")


def write_pdf() -> bool:
    browser = find_browser()
    if not browser:
        print("PDF skipped: Chrome/Edge not found")
        return False
    subprocess.run(
        [
            browser,
            "--headless=new",
            "--disable-gpu",
            "--no-pdf-header-footer",
            f"--print-to-pdf={PDF_PATH}",
            HTML_PATH.as_uri(),
        ],
        check=True,
    )
    return True


def main() -> int:
    DOCS.mkdir(parents=True, exist_ok=True)
    md = build_markdown()
    MD_PATH.write_text(md, encoding="utf-8")
    write_html(md)
    write_pdf()
    print(MD_PATH)
    print(HTML_PATH)
    print(PDF_PATH)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
