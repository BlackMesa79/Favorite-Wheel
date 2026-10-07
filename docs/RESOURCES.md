# 语言与主题资源（0.3.14）

安装目录：

```text
SKSE/Plugins/FavoriteWheel/
  Languages/en.ini
  Languages/zh_CN.ini
  Themes/classic.ini
  Themes/frost.ini
```

文件为 UTF-8（可带 BOM），每行 key=value；以 ; 或 # 开头的行是注释。文件名（不含 .ini）为稳定 ID，Name 为界面显示名。复制现有文件并换一个文件名，即可增加资源；重新启动游戏后出现在设置里。自动部署保留已有值，语言文件仅追加新版本缺失的键。

## 语言

默认 `Language=auto`：按 Windows 显示语言选择译文，依次匹配完整语言代码和去掉地区/脚本后缀的通用代码，没有对应文件回退英文。`zh-CN` 匹配现有 `zh_CN.ini`，代码不区分大小写，连字符与下划线等价；不把繁体中文等其他地区强行替换成简体中文。系统语言只在启动时读取，不跟随输入法变化。F2 的“跟随系统（实际语言）”参与手动语言循环，保存时仍保存 auto。已有 Language 或旧 Chinese 键保留手动选择；需要用户选择跟随系统才切换。详见 [英文翻译指南](LOCALIZATION.md)。

复制 en.ini，翻译等号后的内容，保持键名。未提供或空白文本回退内置英文。语言文件只翻译界面、通知和操作提示，物品名称仍来自游戏。长文本会按可用宽度省略，翻译时应避免过长。

Font 可留空，也可以指定本机字体的绝对路径。字体优先级为语言 Font、主题 Font、主 INI Display/Font。请使用覆盖目标文字的字体；不附带或分发 Windows 字体。当前支持字体可覆盖的常见从左至右文字，没有增加阿拉伯文复杂排版、从右至左布局或字体自动回退链。

## 主题

复制 classic.ini；支持 Accent、Text、Muted、Sector、Empty、Hover、Panel、Border。颜色格式是八位十六进制 RRGGBBAA，例如 E7C98BFF。非法颜色保留该项默认值，缺失主题回退 classic。旧 Background 字段仍可读取，但当前全屏遮罩固定黑色，由主 INI 的 OverlayOpacityPercent 控制；旧 DimPercent 不再生效。普通、空格、高亮扇区强制 alpha=255，以提高辨识度；主题 RGB 仍生效，轮盘间隙不填充。

Font 为可选字体路径。0.3.3 开放下列视觉参数，旧主题不需要补键即可继承默认值。非法、非有限值或带杂字符的数字回退默认；有效超范围值会限制到安全范围。图标纹理、图片背景和自由布局尚未支持；刻纹/金属效果由原创几何和顶点颜色生成，不载入第三方素材。旧 BlurStrength 已忽略，背景模糊及其设置已移除。

| 参数 | 默认 | 范围 / 含义 |
| --- | --- | --- |
| BorderWidth | 1 | 0.5–2，随界面缩放的边框宽度 |
| CornerRadius | 10 | 0–16，面板/按钮圆角 |
| Ornament | 0.65 | 0–1，外环刻度与装饰强度 |
| Relief | 0.5 | 0–1，扇区局部明暗，不改变背景透明度 |
| TextShadow | 0.32 | 0–1，居中文字轻阴影强度 |
| IconScale | 1 | 0.8–1.15，图标相对比例 |
| TitleScale | 1 | 0.85–1.15，标题字号比例，按实际像素重新烘焙 |
| LabelScale | 1 | 0.9–1.1，槽位名称与按钮/设置标签字号比例 |
| HoverDuration | 0.10 | 0–0.25 秒，悬浮缓动时间尺度；0 为即时 |
| PageDuration | 0.12 | 0–0.25 秒，分类/翻页文字淡入时长；0 为即时 |

已有动画开关同时控制上述局部过渡与0.3.8的逐扇区220ms展开/220ms收起；动画不延迟实际操作。开闭时长当前固定，不新增主题键。新增 engraved.ini（Engraved Gold）偏金属刻纹，minimal.ini（Quiet Slate）偏简洁深蓝灰。语言、主题与个人位置/按键配置独立。

## 保存

轮盘内应用设置写入 Data/SKSE/Plugins/FavoriteWheel.ini；MO2 虚拟文件系统最终将写入落在哪个物理目录取决于其文件来源/Overwrite 设置。源码工程不会被运行中的游戏修改。未应用的修改在取消、失焦、读档或关闭菜单时撤销。

0.1.7 的遮罩是固定黑色，由 OverlayOpacityPercent 控制；扇区在稳定状态仍为不透明，开关动画期间整体淡入淡出。新增文字键包含 wheelSize、positionX、positionY、overlayOpacity、sounds、animations、layoutSettingsHelp。

0.3.8顶部邻近分类仍使用已有语言键；选中标题使用TitleScale，邻近项固定较小的原生字号。新增A/D/W/S键帽沿用已有颜色；ShowHints=0时隐藏键帽和底部两行提示，分类/分页状态继续显示。不新增主题/语言键。物品详情卡为右侧分页保留空间，主题字体和字号按最终布局烘焙，不随开闭动画缩放。

0.3.16 设置分外观/控制页，独立键盘入口、组合修饰键及手柄入口/修饰键。新提示均可本地化，新增key见en.ini末尾；界面根据最后操作设备切换手柄与键鼠提示。手柄采用Xbox按键名称，PlayStation按键对应位置相同。已有翻译文件缺新键回退英文。
