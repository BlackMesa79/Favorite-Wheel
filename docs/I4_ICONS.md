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
