# 手绘图解索引

这里有 17 张图，每张都保留同名的 `.svg` 和 `.excalidraw` 文件。SVG 由 Excalidraw 0.18.1 导出，用于文档预览；源文件中的文字、形状和连线可以继续编辑。

## 查看与编辑

点击 SVG 可以放大阅读。要修改图，将 `.excalidraw` 文件拖入 [Excalidraw](https://excalidraw.com)。编辑后保存源文件，再导出 SVG 替换同名图片，让正文预览与源文件保持一致。

图中使用 Excalidraw 的手绘线条和排线填充。英文使用 Excalifont，中文使用它配套的 Xiaolai 手写字体；SVG 已嵌入所需字形，查看时无须安装字体。方框表示组件或数据，蓝色标出当前关注的内容。虚线的含义在各图中说明，例如 ACK、缺口或新输入区间。

重新导出时选择浅色背景，并保留字体嵌入。SVG 同时包含可编辑场景，也可以直接拖回 Excalidraw 打开。

## 图目录

| 章节 | 图解 | SVG | Excalidraw |
| --- | --- | --- | --- |
| 导读 | 学习路线 | [预览](lab-roadmap.svg) | [编辑](lab-roadmap.excalidraw) |
| 导读 / Lab 4 | 一个端点的双向架构 | [预览](tcp-architecture.svg) | [编辑](tcp-architecture.excalidraw) |
| Lab 0 | 容量、分块读取与结束 | [预览](byte-stream.svg) | [编辑](byte-stream.excalidraw) |
| Lab 1 | 接收窗口与应用读取 | [预览](reassembly-window.svg) | [编辑](reassembly-window.excalidraw) |
| Lab 1 | 重叠片段的左右裁剪 | [预览](reassembly-overlap.svg) | [编辑](reassembly-overlap.excalidraw) |
| Lab 1 | EOF 先到、缺口后补 | [预览](reassembly-eof.svg) | [编辑](reassembly-eof.excalidraw) |
| Lab 2 | 三套编号与 SYN / FIN | [预览](sequence-space.svg) | [编辑](sequence-space.excalidraw) |
| Lab 2 | 序号回绕与 checkpoint | [预览](sequence-wrap.svg) | [编辑](sequence-wrap.excalidraw) |
| Lab 2 | 乱序与累计 ACK | [预览](receiver-ack.svg) | [编辑](receiver-ack.excalidraw) |
| Lab 3 | 发送窗口与部分确认 | [预览](sender-window.svg) | [编辑](sender-window.excalidraw) |
| Lab 3 | 超时重传与 RTO 退避 | [预览](sender-retransmission.svg) | [编辑](sender-retransmission.excalidraw) |
| Lab 4 | TCPMessage 的两个方向 | [预览](peer-message.svg) | [编辑](peer-message.excalidraw) |
| Lab 4 | 建立连接与半关闭 | [预览](connection-lifecycle.svg) | [编辑](connection-lifecycle.excalidraw) |
| Lab 5 | ARP 请求、学习与排队释放 | [预览](arp-resolution.svg) | [编辑](arp-resolution.excalidraw) |
| Lab 6 | 最长前缀匹配 | [预览](routing-prefix.svg) | [编辑](routing-prefix.excalidraw) |
| Lab 6 | 跨路由器换帧与 TTL | [预览](routing-forward.svg) | [编辑](routing-forward.excalidraw) |
| Lab 7 | 两端拓扑与内外层封装 | [预览](endtoend-path.svg) | [编辑](endtoend-path.excalidraw) |

图中的数值对应章节示例。组件职责与协议细节以各 Lab 文档和当前源码为准。
