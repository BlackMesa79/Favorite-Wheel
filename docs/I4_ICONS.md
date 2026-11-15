# 已安装 I4 图标调查（2026-09-29）

读取用户 MO2 的 Basic - OStim 配置，按 modlist 的覆盖优先级检查松散文件，并按启用插件筛选 I4 JSON。此次是资源读取与兼容性验证，0.2.1 尚未在轮盘中显示 I4 图标。

## 已读取的资源

| 项目 | 结果 |
| --- | --- |
| 有效启用插件对应的松散 I4 配置 | 26 个 |
| JSON 规则 | 666 条 |
| 不区分大小写的 iconSource | 28 个，全部找到对应松散 SWF |
| SWF 帧标签 | 1267 个（含各源内部标签） |
| JSON 解析失败 | 0 |

BOOBIES 的当前实际启用版本目录包含 10 个 SWF，THICC 包含 46 个 SWF，Rotols More Icons 包含 105 个 JSON。105 个配置文件不意味着本角色全部生效：I4 只按加载插件读取对应配置。用户安装的 InventoryInjector.dll 与日志报告 1.1.0；官网当前显示 1.1.1，不以官网版本替代本机事实。

已经从实际 SWF 中确认例如 `BOOBIES Immersive Icons/bodyparts.swf` 的 `armor_abs` 位于主时间轴第 35 帧。规则同时携带 iconSource、iconLabel、iconColor，以及关键词等匹配条件。因此无需重新绘制这些图标，但单纯搜文件名无法判断一个具体物品应该使用哪个图标。

脚本：`scripts/audit-i4.py`。本地详细结果：`build/i4-icon-audit.json`，包括规则来源、资源路径、标签和未匹配标签。少量规则标签未在其 SWF 中找到，需要接入时回退；不修改用户现有配置。本次不解析 BSA、不在离线阶段判断运行时 KID 关键词，也不等同于完整游戏资源解析器。

## 接入方式

官方 [I4 源码](https://github.com/Exit-9B/InventoryInjector/blob/main/src/AS/Functions.cpp) 注册了 Scaleform 的 `skse.plugins.InventoryInjector.ProcessEntry`，能对符合 SkyUI 字段结构的条目应用已加载规则。实际规则使用 [CustomDataManager](https://github.com/Exit-9B/InventoryInjector/blob/main/src/Data/CustomDataManager.cpp) 按顺序匹配，并处理默认图标与覆盖。这不是可直接返回 D3D11 纹理的 ImGui 接口。

建议后续适配分为两部分：在游戏 UI 上下文提供正确的物品条目，让 I4 计算最终图标源/标签/颜色；再将 SWF 中相应帧渲染为轮盘可用纹理，并按源/标签/分辨率缓存。需要验证加载顺序、关键词、药水效果、装备类型及图标颜色，避免重写简化规则后与背包显示不一致。字体图标或 PNG 格式的 ImGui Icons 主要是按键和字体资源，不能直接代替完整 I4 物品分类。

图标渲染需单独验证 Scaleform 与 ImGui/CS 的桥接和缓存成本；不能把所有 SWF 当作 PNG 直接加载。本次没有复制任何第三方美术进安装包或源码包，也没有引入新的运行时依赖。

参考：[BOOBIES](https://www.nexusmods.com/skyrimspecialedition/mods/89241)、[I4](https://www.nexusmods.com/skyrimspecialedition/mods/85702)。本地安装文件是本次统计依据。

## 2026-11-10 离线验证结论

本节结论说明这条路线可行，正式接入方案见 [plans/20261110-i4-icons.md](plans/20261110-i4-icons.md)（DRAFT，待用户确认）。

- **SWF 结构**：已装的图标 SWF 都是 AS2 时代的格式（版本 8/10），只包含静态矢量形状（DefineShape1–2 为主）和少量无损位图，没有 AS3 代码，每个图标是一个 sprite 加一个主时间轴帧标签。因此不需要 Scaleform 运行时，自研的轻量解析与栅格化就足够。
- **FFDec 导出与 WheelPreview 验证**：BOOBIES 共 225 个标签，其中 189 个非空、36 个是空占位。转成 PNG 并套进现有图标包后，在轮盘里可以正常辨认。舞台画布会裁掉部分图案，所以必须使用 sprite 的实际边界。
- **锯齿**：原因是 ImGui DX11 后端的采样器设置了 `MaxLOD=0`，不使用 mip，与图标转换无关。已在 `035c453` 中通过 `ImageSampler` 修复。
- **配色**：225 个图标都没有明显的第二种颜色，转成“白色 + 原 alpha”的遮罩后，乘色就等于替换成目标色。I4 的 `iconColor`、主题色都可以这样实现；直接给彩色原图乘色会变暗、偏色。
- **规则规模**：已装规则 1117 条图标赋值，其中 213 条引用 SkyUI 自带的 SWF（通常位于 BSA 中），因此运行时必须通过游戏资源系统读取文件。

验证产物（含第三方美术衍生 PNG）只保存在本地 `build/i4-preview/`，不进入仓库或安装包。以上都是离屏渲染结果，尚未进行游戏内验证。

## 2026-11-11 与本项目相关的已装注入器

本机同时还装了 **Inventory Interface Information Injector Improved**（Nexus 192976，v0.4.0）。核实结论：

- 它只提供 `SKSE/Plugins/InventoryInjectorImproved.dll`，**不自带规则文件**。`SKSE/Plugins/InventoryInjector/*.json` 仍由原版 I4 读取，轮盘照常读同一批 JSON，规则来源不变。
- 它 hook 的是 SkyUI 的 `processList` 与 `FavoritesMenu::ProcessMessage`（DLL 内字符串 `Favorites: hooked FavoritesMenu::ProcessMessage`、`I4Hook: wrapped {}.processList`），即“背包里已经填好的 SkyUI 条目”的转写与缓存，另有持久化缓存文件（`ENTR`/`HEAD` 记录）。
- 这是对 Scaleform 层的优化。轮盘不经过 SkyUI，第三期由 `Collect` + `Derive` 自行生成等价字段，因此不受它的缓存影响；两者在同一原版收藏菜单上注册 hook 时可能有加载顺序相关的影响，需在游戏内验收（AC-10）时确认，尤其是 Improved 的 `FavoritesHook` 是否与轮盘快捷键或暂停状态交互。
- 另装有 **Dynamic Inventory Icon Injector**（DIII，状态图标注入），与本方案无关，不读取其规则。


## 磁盘位图缓存（T-4b）

缓存位于游戏 exe 所在目录下的 `Data/SKSE/Plugins/FavoriteWheel/IconCache/`，由 `GetModuleFileNameW` 定位。MO2 下新文件预期进入 overwrite 或指定输出 mod；不同实例使用各自的虚拟 Data 目录，不使用公共 My Games。只缓存本机 SWF 转换出的非预乘 RGBA，不缓存规则、物品匹配结果或 GPU 纹理。每次启动仍通过游戏资源系统读取 SWF，后台对内容计算 SHA-256；内容、标签原文、变体、边长或转换器版本变化都会失效。

主 INI 的 `[Display]` 段支持 `I4IconCache=1` 和 `I4IconCacheMB=64`（MiB，范围 8–512）。它们与 I4 显示设置同属显示资源配置；缺失时使用默认值，只有用户点击应用才写回主 INI。设为 `I4IconCache=0` 后不读写缓存目录，清空按钮也不会执行磁盘操作。已经开始的后台 IO 可能在设置切换前完成。

SKSE Menu Framework 的 FavoriteWheel 外观页提供开关、容量与“清空图标缓存”。清空在后台执行，界面显示排队、完成或不可用状态；当前已准备/上传的纹理保留，清空前已排队的转换不会重新写回磁盘。之后新请求可以再次生成缓存。轮盘内外观页已有 14 行、24 像素行距，因此不再增加缓存控件。也可在退出游戏后删除 IconCache 目录。

每个 SWF 内容哈希对应一个 `<sha256>.fwic`。文件使用显式小端编码，20 字节头含 FWIC、格式版本、转换器版本、条目数及条目表长度。每条记录含标签长度、变体、边长、宽、高、64 位偏移、压缩长度、原始长度、CRC32，后接未经规范化的标签字节；负载是 miniz zlib 流。只接受 1–256 方形图、精确的 RGBA 长度、正确 CRC、连续且不重叠的负载；任一条目无效则丢弃整文件。同一哈希的无效文件每次进程运行只告警一次。上限为每文件 64 MiB、4096 条、标签 1024 字节、总解压数据 256 MiB；超大的库达到写入上限时从当前条目重新建文件，避免生成无法读取的缓存。

哈希、全文件校验、解压、转换、合并写入和清理均复用 I4Textures 的单个工作线程，不在游戏/渲染线程执行，也不在持有管线互斥锁时做 IO。写入先合并已有条目，累计 32 条新增/替换或 4 MiB 新压缩数据后批量落盘；工作队列空闲或切换 SWF 时也会刷新。退出游戏时尚未落盘的条目会丢弃，下次重新转换。落盘时生成同目录 `.fwic.tmp`，刷新文件并原子替换正式文件。中断留下的临时文件不会被读取，下次启用缓存时清除。该线程模型针对单个游戏进程；不提供多个进程同时共享同一物理缓存目录的事务合并。

采用文件 mtime 记录最近使用时间：读取命中时更新，写入时刷新；启用缓存或修改容量时执行一次 Trim，之后累计成功写入 8 次，或累计写入的文件字节数达到容量的 1/4（最多 16 MiB）时，再按最旧 mtime 淘汰整个 SWF 文件。维护间隙允许短暂超出预算；这里的“短暂”指维护点之间，没有固定时间上限，空闲刷新本身不额外触发 Trim。mtime 只用于容量淘汰，绝不用于 SWF 内容失效。

MO2 的虚拟文件系统决定时间戳更新/删除落到哪个实际文件。更新时间失败只告警一次，LRU 顺序可能陈旧，缓存仍继续读写；Trim 的扫描、容量统计、mtime 读取或淘汰失败也只告警一次，在后续维护点重试，期间可能超过预算。缓存内容读写、原子替换、初始化或显式清空失败仍沿用原有的磁盘不可用处理，告警并退化为内存缓存。建议让可再生缓存留在可写的 overwrite/输出 mod。MO2 的实际重定向和时间戳行为仍需实机确认。

离线验收：`IconCacheTests` 覆盖文件校验、独立实例、版本/内容失效、真实 Windows 只读 ACL、容量与清空；`I4IntegrationTests` 覆盖两次独立管线启动，第二次 Render/Parse 均为零。所有缓存夹具只写入 build 目录。

## 2026-11-15 验收修正：扇区与详情颜色一致

部分书籍、非元素法术没有 `iconColor`，原来回退到绘制位置传入的 ink：未悬停扇区为文字色，中心和详情为强调色。切换配色时仍会看到同一批扇区图标偏白。现在 `Theme` 模式统一使用主题 `accent`；`I4` 模式有颜色时保留指定色，无颜色时使用主题 `accent`。`Original` 保留素材原色，素材本身是白色时仍会显示白色。所有模式保留调用位置透明度；图标包覆盖和内置图标原有策略不变。此修复不改变位图缓存，不需要清缓存。

2026-11-15 实机验收：用户重启游戏后确认扇区与详情图标颜色一致，本次颜色不一致问题已修复。验收依据为用户反馈，其他实机项目仍按验收清单记录。
