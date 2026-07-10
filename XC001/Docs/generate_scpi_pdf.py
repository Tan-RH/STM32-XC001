# -*- coding: utf-8 -*-
from datetime import date
from pathlib import Path
from xml.sax.saxutils import escape

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    PageBreak,
    Paragraph,
    SimpleDocTemplate,
    Spacer,
    Table,
    TableStyle,
)


ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "Docs"
PDF_PATH = OUT_DIR / "XC001_SCPI指令表(内部开发用).pdf"
FONT_REGULAR = Path(r"C:\Windows\Fonts\simhei.ttf")


def p(text, style):
    return Paragraph(escape(str(text)).replace("\n", "<br/>"), style)


def table(data, widths, header=True):
    t = Table(data, colWidths=widths, repeatRows=1 if header else 0)
    style = [
        ("FONTNAME", (0, 0), (-1, -1), "CN"),
        ("FONTSIZE", (0, 0), (-1, -1), 8.4),
        ("GRID", (0, 0), (-1, -1), 0.35, colors.HexColor("#BFC8D2")),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 5),
        ("RIGHTPADDING", (0, 0), (-1, -1), 5),
        ("TOPPADDING", (0, 0), (-1, -1), 4),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
    ]
    if header:
        style.extend(
            [
                ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#142536")),
                ("TEXTCOLOR", (0, 0), (-1, 0), colors.white),
            ]
        )
    t.setStyle(TableStyle(style))
    return t


def header_footer(canvas, doc):
    canvas.saveState()
    canvas.setFont("CN", 8)
    canvas.setFillColor(colors.HexColor("#6B7682"))
    canvas.drawString(14 * mm, 9 * mm, "XC001 控制板 SCPI 指令表（内部开发用）")
    canvas.drawRightString(196 * mm, 9 * mm, f"第 {doc.page} 页")
    canvas.restoreState()


def draw_cover_logo(canvas):
    canvas.saveState()
    x = 32 * mm
    y = 230 * mm
    canvas.setFillColor(colors.HexColor("#E4003A"))
    canvas.rotate(0)
    canvas.translate(x, y)
    canvas.scale(0.34, 0.34)
    canvas.roundRect(0, 40, 145, 44, 12, fill=1, stroke=0)
    canvas.roundRect(118, 105, 66, 34, 12, fill=1, stroke=0)
    canvas.roundRect(14, -26, 130, 42, 12, fill=1, stroke=0)
    canvas.roundRect(126, -38, 96, 40, 12, fill=1, stroke=0)
    canvas.restoreState()
    canvas.saveState()
    canvas.setFillColor(colors.HexColor("#142536"))
    canvas.setFont("CN", 30)
    canvas.drawString(68 * mm, 238 * mm, "GeneralTest")
    canvas.restoreState()


def build_pdf():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    pdfmetrics.registerFont(TTFont("CN", str(FONT_REGULAR)))

    styles = getSampleStyleSheet()
    styles.add(ParagraphStyle("TitleCN", fontName="CN", fontSize=24, leading=32, alignment=TA_CENTER))
    styles.add(ParagraphStyle("SubTitleCN", fontName="CN", fontSize=12, leading=18, alignment=TA_CENTER, textColor=colors.HexColor("#56616D")))
    styles.add(ParagraphStyle("H1CN", fontName="CN", fontSize=15, leading=20, spaceBefore=12, spaceAfter=7, textColor=colors.HexColor("#142536")))
    styles.add(ParagraphStyle("BodyCN", fontName="CN", fontSize=9.2, leading=14))
    styles.add(ParagraphStyle("SmallCN", fontName="CN", fontSize=8.2, leading=12, textColor=colors.HexColor("#56616D")))
    styles.add(ParagraphStyle("CodeCN", fontName="CN", fontSize=8.6, leading=12, textColor=colors.HexColor("#111827")))

    commands = [
        ("通用指令", "*IDN?", "查询设备标识。兼容 IDN?。", "XC001,CONTROL-BOARD,H743,V1.0", "查询"),
        ("通用指令", "PING?", "通信链路测试，用于确认 UART7、UDP 或网页通道可用。", "PONG", "查询"),
        ("通用指令", "*CLS", "清除当前状态/错误标志。当前固件返回 OK。", "OK", "设置"),
        ("通用指令", "*RST", "重新初始化业务板级 IO，扩展 IO 恢复默认输出状态。", "OK", "设置"),
        ("通用指令", "SYST:ERR?", "查询系统错误队列。当前固件固定返回无错误。", '0,"No error"', "查询"),
        ("通用指令", "SYST:HELP?", "查询当前固件支持的指令列表。", "逗号分隔的指令清单", "查询"),
        ("通用指令", "STAT?", "查询网络、CAN、RS485、SPI 综合状态。", "IP=...;CAN1:...;RS485:...;SPI5:...", "查询"),
        ("网络指令", "NET:STAT?", "查询当前运行中的 IP、网关、子网掩码、UDP 端口和 HTTP 端口。", "IP=192.168.1.10,GW=192.168.1.1,MASK=255.255.255.0,UDP=4000,HTTP=80", "查询"),
        ("网络指令", "NET:DIAG?", "查询 LwIP、PHY、HTTP 计数和以太网 DMA 诊断信息。ND? 为快捷别名。", "NET:IP=...,UP=1,LINK=1,HTTP=1,...", "查询"),
        ("网络指令", "ND?", "NET:DIAG? 的快捷别名。", "NET:IP=...,UP=1,LINK=1,HTTP=1,...", "查询"),
        ("网络指令", "NET:PHY?", "查询 LAN8742 PHY 初始化状态、地址、链路和寄存器信息。", "PHY:INIT=1,ADDR=0,LINK=...", "查询"),
        ("网络指令", "NET:IP?", "查询当前运行 IP 地址。", "192.168.1.10", "查询"),
        ("网络指令", "NET:IP a.b.c.d", "设置 IP 地址并保存到 Flash。当前连接不立即切换，重新上电/重启后生效。", "OK,SAVED,REBOOT_REQUIRED", "设置"),
        ("网络指令", "NET:MASK?", "查询当前运行子网掩码。", "255.255.255.0", "查询"),
        ("网络指令", "NET:MASK a.b.c.d", "设置子网掩码并保存到 Flash，重新上电/重启后生效。", "OK,SAVED,REBOOT_REQUIRED", "设置"),
        ("网络指令", "NET:GATE?", "查询当前运行网关。", "192.168.1.1", "查询"),
        ("网络指令", "NET:GATE a.b.c.d", "设置网关并保存到 Flash，重新上电/重启后生效。", "OK,SAVED,REBOOT_REQUIRED", "设置"),
        ("网络指令", "NET:PORT?", "查询 UDP SCPI 端口。", "4000", "查询"),
        ("网络指令", "NET:PORT n", "设置 UDP SCPI 端口并保存到 Flash，重新上电/重启后生效。n 范围 1~65535。", "OK,SAVED,REBOOT_REQUIRED", "设置"),
        ("网络指令", "NET:SERV ON", "请求启动 HTTP/UDP 网络服务。当前固件已配置为上电自动启动。", "OK,STARTING", "设置"),
        ("网络指令", "NET:MAC?", "查询固件配置的 MAC 地址。", "00:80:E1:00:00:00", "查询"),
        ("数字 IO", "DIG:LIST?", "列出固件识别的扩展数字 IO 名称。", "PE3,PE4,...,LED1,LED2,ETH_NRST", "查询"),
        ("数字 IO", "DIG:OUTP pin,val", "设置指定可写 IO 输出。val=0 输出低电平，val=1 输出高电平，val=T/t 表示翻转。", "OK", "设置"),
        ("数字 IO", "DIG:OUTP? pin", "读取指定 IO 当前电平。", "0 或 1", "查询"),
        ("CAN 指令", "CAN:STAT?", "查询 FDCAN1 初始化状态、发送计数和接收计数。", "CAN1:READY=1,TX=0,RX=0", "查询"),
        ("CAN 指令", "CAN:SEND id,hex", "发送 CAN 数据帧。id 支持十进制或 0x 前缀，hex 为 0~8 字节十六进制数据。", "OK", "设置"),
        ("CAN 指令", "CAN:RX?", "查询最近一次收到的 CAN 帧。无数据时返回 EMPTY。", "ID=0x123,LEN=2,DATA=1122", "查询"),
        ("RS485 指令", "RS485:STAT?", "查询 RS485 发送/接收计数。", "RS485:TX=0,RX=0", "查询"),
        ("RS485 指令", "RS485:SEND text", "通过 UART8/RS485 发送文本，自动追加 CRLF。", "OK", "设置"),
        ("RS485 指令", "RS485:RX?", "查询最近一次 RS485 接收文本。无数据时返回 EMPTY。", "EMPTY 或接收文本", "查询"),
        ("SPI 指令", "SPI:STAT?", "查询 SPI5 初始化状态和传输计数。", "SPI5:READY=1,COUNT=0", "查询"),
        ("SPI 指令", "SPI:TRAN? hex", "通过 SPI5 传输十六进制数据，并返回同步接收数据。", "十六进制接收数据", "查询/传输"),
    ]

    story = []
    story.append(Spacer(1, 58 * mm))
    story.append(p("XC001 控制板", styles["TitleCN"]))
    story.append(p("SCPI 指令表（内部开发用）", styles["TitleCN"]))
    story.append(Spacer(1, 8 * mm))
    story.append(p("适用固件：STM32H743 / HAL / FreeRTOS / LwIP", styles["SubTitleCN"]))
    story.append(p(f"文档版本：V1.0　生成日期：{date.today().isoformat()}", styles["SubTitleCN"]))
    story.append(PageBreak())

    story.append(p("1. 修订记录", styles["H1CN"]))
    rev = [
        [p("版本", styles["BodyCN"]), p("日期", styles["BodyCN"]), p("说明", styles["BodyCN"])],
        [p("V1.0", styles["BodyCN"]), p(date.today().isoformat(), styles["BodyCN"]), p("根据当前 XC001 固件源码整理 SCPI 指令、网络参数和网页接口。", styles["BodyCN"])],
    ]
    story.append(table(rev, [25 * mm, 35 * mm, 120 * mm]))

    story.append(p("2. 通信接口", styles["H1CN"]))
    interfaces = [
        [p("接口", styles["BodyCN"]), p("参数", styles["BodyCN"]), p("说明", styles["BodyCN"])],
        [p("UART7 控制台", styles["BodyCN"]), p("115200, 8N1", styles["BodyCN"]), p("Type-C 调试串口。输入 SCPI 指令后返回文本响应。", styles["BodyCN"])],
        [p("UDP SCPI", styles["BodyCN"]), p("默认端口 4000", styles["BodyCN"]), p("向板卡 IP 的 UDP 端口发送 SCPI 文本，返回文本响应。", styles["BodyCN"])],
        [p("Web 页面", styles["BodyCN"]), p("HTTP 80", styles["BodyCN"]), p("默认 http://192.168.1.10/ ，可在网页内发送 SCPI 指令并保存网络参数。", styles["BodyCN"])],
    ]
    story.append(table(interfaces, [32 * mm, 45 * mm, 103 * mm]))

    story.append(p("3. 默认网络参数", styles["H1CN"]))
    story.append(
        p(
            "默认 IP 为 192.168.1.10，子网掩码为 255.255.255.0，网关为 192.168.1.1，"
            "HTTP 端口为 80，UDP SCPI 端口为 4000。网页或 SCPI 修改网络参数后保存到 Flash，"
            "断电不丢失，重新上电/重启后生效。ETH_NRST 为 PE15，低电平有效；上电时保持低电平可恢复默认网络参数。",
            styles["BodyCN"],
        )
    )

    story.append(p("4. 指令格式和返回格式", styles["H1CN"]))
    story.append(
        p(
            "SCPI 指令大小写不敏感。查询类指令通常以 ? 结尾；设置类指令使用空格分隔参数。"
            "建议每条串口指令以回车或换行结束。成功通常返回 OK 或具体数据；参数错误返回 ERR,-222；"
            "未知指令返回 ERR,-113。",
            styles["BodyCN"],
        )
    )

    story.append(p("5. SCPI 指令列表", styles["H1CN"]))
    cmd_data = [[p("分类", styles["BodyCN"]), p("指令", styles["BodyCN"]), p("功能说明", styles["BodyCN"]), p("典型返回", styles["BodyCN"]), p("类型", styles["BodyCN"])]]
    for group, command, desc, reply, kind in commands:
        cmd_data.append([p(group, styles["BodyCN"]), p(command, styles["CodeCN"]), p(desc, styles["BodyCN"]), p(reply, styles["CodeCN"]), p(kind, styles["BodyCN"])])
    story.append(table(cmd_data, [22 * mm, 38 * mm, 72 * mm, 36 * mm, 16 * mm]))

    story.append(p("6. 数字 IO 名称说明", styles["H1CN"]))
    io_data = [
        [p("名称", styles["BodyCN"]), p("端口/引脚", styles["BodyCN"]), p("说明", styles["BodyCN"])],
        [p("PE3, PE4, PE5, PE6", styles["CodeCN"]), p("GPIOE", styles["BodyCN"]), p("扩展输出，默认低电平。PE3 同时用于 RS485 DE 控制。", styles["BodyCN"])],
        [p("PC8~PC13", styles["CodeCN"]), p("GPIOC", styles["BodyCN"]), p("扩展输出，默认低电平。", styles["BodyCN"])],
        [p("PA8, PA9, PD2", styles["CodeCN"]), p("GPIOA/GPIOD", styles["BodyCN"]), p("扩展输出，默认低电平。", styles["BodyCN"])],
        [p("LED1", styles["CodeCN"]), p("PB1", styles["BodyCN"]), p("运行状态指示灯，正常运行时闪烁。", styles["BodyCN"])],
        [p("LED2", styles["CodeCN"]), p("PB2", styles["BodyCN"]), p("错误状态指示灯，高电平熄灭，低电平点亮。", styles["BodyCN"])],
        [p("ETH_NRST", styles["CodeCN"]), p("PE15", styles["BodyCN"]), p("以太网复位/网络参数恢复输入，低电平有效，不允许通过 DIG:OUTP 写入。", styles["BodyCN"])],
    ]
    story.append(table(io_data, [45 * mm, 35 * mm, 100 * mm]))

    story.append(p("7. Web HTTP 接口", styles["H1CN"]))
    http_data = [
        [p("接口", styles["BodyCN"]), p("方法", styles["BodyCN"]), p("说明", styles["BodyCN"])],
        [p("/", styles["CodeCN"]), p("GET", styles["BodyCN"]), p("打开 XC001 网页控制台。", styles["BodyCN"])],
        [p("/api/cmd", styles["CodeCN"]), p("POST", styles["BodyCN"]), p("请求体为 SCPI 指令文本，返回 SCPI 文本响应。", styles["BodyCN"])],
        [p("/api/config", styles["CodeCN"]), p("GET", styles["BodyCN"]), p("无参数时读取当前运行网络配置。", styles["BodyCN"])],
        [p("/api/config?ip=...&mask=...&gw=...&port=...&pwd=...", styles["CodeCN"]), p("GET", styles["BodyCN"]), p("保存网络参数到 Flash。默认网页密码为 admin，保存后重启生效。", styles["BodyCN"])],
    ]
    story.append(table(http_data, [62 * mm, 22 * mm, 96 * mm]))

    story.append(p("8. 使用示例", styles["H1CN"]))
    examples = [
        [p("目标", styles["BodyCN"]), p("示例指令", styles["BodyCN"]), p("预期返回", styles["BodyCN"])],
        [p("查询设备", styles["BodyCN"]), p("*IDN?", styles["CodeCN"]), p("XC001,CONTROL-BOARD,H743,V1.0", styles["CodeCN"])],
        [p("修改 IP", styles["BodyCN"]), p("NET:IP 192.168.1.20", styles["CodeCN"]), p("OK,SAVED,REBOOT_REQUIRED", styles["CodeCN"])],
        [p("设置 PE4 高电平", styles["BodyCN"]), p("DIG:OUTP PE4,1", styles["CodeCN"]), p("OK", styles["CodeCN"])],
        [p("发送 CAN 帧", styles["BodyCN"]), p("CAN:SEND 0x123,11223344", styles["CodeCN"]), p("OK", styles["CodeCN"])],
        [p("SPI 传输", styles["BodyCN"]), p("SPI:TRAN? 9F0000", styles["CodeCN"]), p("十六进制接收数据", styles["CodeCN"])],
    ]
    story.append(table(examples, [45 * mm, 70 * mm, 65 * mm]))

    doc = SimpleDocTemplate(
        str(PDF_PATH),
        pagesize=A4,
        rightMargin=12 * mm,
        leftMargin=12 * mm,
        topMargin=14 * mm,
        bottomMargin=16 * mm,
    )

    def first_page(canvas, doc_obj):
        draw_cover_logo(canvas)
        header_footer(canvas, doc_obj)

    doc.build(story, onFirstPage=first_page, onLaterPages=header_footer)
    print(PDF_PATH)


if __name__ == "__main__":
    build_pdf()
