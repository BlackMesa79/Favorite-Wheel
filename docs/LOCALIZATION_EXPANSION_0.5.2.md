# 0.5.2 多语言扩充

2026-10-10：用户提供社区法语0.5.0翻译，随后授权补齐巴西葡萄牙语、俄语、日语、韩语、德语。当前仍为0.5.2开发测试版，最近正式0.5.1，不生成发布包。

自带en/zh_CN/fr/pt_BR/ru/ja/ko/de，每份219键，与当前英文集合完全一致，无重复/未知键，除Font外均有文本。fr保留社区原文基底，修复换行、显示名称、路径及三处拼写，补17新键；来源见FRENCH_LOCALIZATION.md。其余新语言及法语补写文本为初始译文，未获得母语用户校对。英文/中文KeepOpen说明也同步注明预设会关盘，保持当前已验收行为。

系统显示语言按既有完整locale→通用语言→英文解析；pt_BR仅用于巴西，不替代pt_PT。显式语言与现有INI继续保留，F2可手动选择。

日/韩Font=auto在资源加载时解析Windows目录及当地系统字体。字体atlas重建时检查请求字符在主字体中的覆盖，仅把缺字按本地Segoe UI/中文/韩文/日文字体链合并。字体文件使用稳定map缓存，merge ranges保留到下一次atlas Clear；仅在字形/字号/主字体/主题字体比例变化时构建。没有每帧文件读取、全CJK字符预烘焙、字体分发或新引擎地址。语言名、UI以及库存/效果/预设中其他文字共享字形回退。

## 验证

Release编译与自动部署成功。构建/安装DLL SHA256一致：`C565B9376D7840B8601971C4DC288933FB726ADEABC55337151F2D42B6E6886F`。旧DLL为build/FavoriteWheel-before-locales052.dll。

- 五项Settings/Inventory/WheelLogic/Time/RuntimeLayout测试通过，结果build/locales052-tests.txt；覆盖8份完整键、各locale、巴西/葡萄牙地区区别与原持久化路径。
- 实际DrawWheel WARP共40张720p图：8语言×外观/手柄/游戏行为/功能预设/药水详情。全部检查有效UI字符和全部语言名称无缺字、动态字形增加/字号变化、八语言切换后缓存稳定、遮罩/扇区/顶点，并人工审图。
- 追加1440p日语命名/韩语面光、1080p俄语键盘/德语游戏行为、720p缺失主字体回退，均通过自动检查与人工审图。build/locales052-extra-preview.txt、locales052-preview.txt记录结果；图为build/locales052-*.png。
- 本机既有8个主配置/翻译/主题/meta INI SHA256完整保持，基线build/locales052-preserved.json。新加入6种语言自动复制；安装文件与源码各新语言逐一一致。以后部署仍仅追加缺失键，不覆盖玩家已有译文。

Review确认合并font数据和range指针生命周期、缺失主字体、同一atlas保持单纹理、稳定帧早返回与资源配置回退。新字体缺失时无法凭空提供字形：需当地Windows可选语言字体或玩家指定字体；本机有相应字体，未验证删字体环境的所有组合。未启动游戏，新语言和字体变化待实机及母语反馈。

1080Ti报告单独记录在GPU_LOAD_REPORT.md：没有实际帧率/功耗数据，不认定过度绘制或硬件性能不足，不从WARP得出GPU时间结论，也不强行加入Present限帧。
