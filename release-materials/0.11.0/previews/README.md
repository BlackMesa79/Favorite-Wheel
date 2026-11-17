# I4 轮盘发布预览

三张图片均为 2560×1440，提供无损 PNG 与 JPEG，由真实 WheelPreview / Draw.cpp、I4Textures、SwfRaster 与 WARP D3D11 离屏渲染生成，使用示例库存，不是游戏截图。发布页优先使用 PNG。

标题原先放大了界面默认小字号的字形纹理，导致边缘发虚。2026-11-17 复调后，宣传文字按最终像素字号单独烘焙；轮盘内部字体和游戏 DLL 不变。

- weapons.png / weapons.jpg：Classic Gold 主题，Theme 配色。
- armor.png / armor.jpg：Skyrim 主题，I4 配色，示例 iconColor 展示多色。
- magic.png / magic.jpg：Skyrim Polygon 主题，Original 配色，使用法术书等实际素材。

图标来自本机单独安装的 B.O.O.B.I.E.S (aka Immersive Icons)。图片注明来源；安装包不附带第三方 SWF 或独立图标文件。示例名称、分类和颜色用于展示，不表示这些示例已在游戏中逐条通过规则匹配验收。

复现：构建 WheelPreview，设置 FW_PREVIEW_I4_MANIFEST 指向 UTF-8 文本，每行依次为带双引号的物品名、SWF 绝对路径、标签，以及十进制 RGB 颜色。使用 i4-icons / i4-armor / i4-magic 场景，依次选择 classic / skyrim / polygon。

本次清单及原始 BMP 保留在 build/release-011/，第三方源素材仍位于本机模组目录。模型视觉检查只读取了压缩预览。
