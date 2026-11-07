# 自定义图标库与法术学派图标

## 已实现

普通法术按游戏的 `GetAssociatedSkill()` 归类为变化、召唤、毁灭、幻术、恢复。恢复系包含治疗、结界等；不通过名称猜测。没有明确学派的法术使用通用魔法图标。龙吼、力量、书籍也具有独立内置图标。卷轴保留卷轴图标，不改变分类、排序、装备行为或存档。

无需安装图标包即可获得内置学派区分。图标包是主题引用的可选资源；同一包可以被多个主题共享。当前不提供游戏内图标浏览器或热重载，修改资源后重新启动游戏。

## 安装和引用

把图标包放在 `Data/SKSE/Plugins/FavoriteWheel/IconPacks/<包名>/`。包名只允许 ASCII 字母、数字、下划线和连字符，最长 64 字符。

在所用主题的 INI 中添加：

```ini
IconPack=example
```

仓库随附 `IconPacks/example/icons.ini` 与透明恢复系示例图片，不会自动修改用户主题。删除 `IconPack` 行即可恢复内置图标。

## 清单格式

清单固定名为 `icons.ini`，UTF-8 编码，可带 BOM。支持整行 `;` 或 `#` 注释；不支持行尾注释。同一键最后一项生效。图标键与插件名匹配不区分 ASCII 大小写。

```ini
[Icons]
restoration=restoration.png|tint
destruction=flame.png|original
my_healing=healing.png|original

[Forms]
Skyrim.esm|00012FCC=my_healing
```

图片路径相对于清单目录；只接受 PNG 和包内相对路径，拒绝绝对路径、冒号与 `..`。`tint` 为默认值，图片 RGB 乘以主题图标颜色，适用于白色透明线稿；`original` 保留图片 RGB。两者都保留透明度并跟随界面淡入淡出。图像保持宽高比，最长边适配原有图标框。

`[Forms]` 左侧是插件文件名和十六进制本地 FormID，允许 `0x` 前缀，不能填写包含加载序号的运行时完整 FormID；ESL 也使用本地 ID。右侧引用 `[Icons]` 定义的键，可以是自定义别名。映射按基础物品生效，不区分同一基础物品的附魔或命名实例。动态生成且无插件来源的表单不做精确映射。物品映射不覆盖功能按钮。

回退顺序：有效的物品映射图片 → 有效的分类/功能图片 → 魔法学派、龙吼、力量的 `magic` 图片 → 内置图标。没有配置 `magic` 时，各学派仍有独立的内置图形。坏图片与缺失映射不阻止轮盘使用，日志会汇总资源告警。

## 标准键

- 武器：`sword dagger axe mace bow crossbow staff arrow`
- 防具：`armor robe helmet gloves boots ring amulet shield`
- 物品：`potion poison food scroll torch book other`
- 魔法：`magic alteration conjuration destruction illusion restoration shout power`
- 功能：`save_outfit import_outfits outfit combat save_combat cycle`
- 导航/面光：`back light_player light_target light_group`

战斗循环成员沿用 `combat`，捕获循环成员沿用 `save_combat`。

## 资源限制与部署

清单最大 256 KiB，每包最多解析 512 个条目，最多加载 32 个被主题引用的包。PNG 单文件最大 8 MiB、每边不超过 1024 像素；总计最多 128 张不同图片与 16M 基础像素（RGBA 基础纹理约 64 MiB，完整 mip 链另有开销）。建议使用 128–256 像素图标。相同路径共享 GPU 纹理。加载发生在渲染器初始化，绘制时不读盘、不查询游戏表单。设备重建时释放并重新加载。

自动部署只补充不存在的示例图标包文件，不覆盖用户同名资源；发布打包包含 assets 下的图标包。主 INI 和用户主题引用不被自动改写。

## 验证范围

`IconLibraryTests` 使用 WARP 和 WIC，检查实际 GPU 纹理透明度/颜色、宽高比、资源共享、指定物品优先级、图片缺失回退及功能按钮隔离。SettingsTests、InventoryTests、WheelLogicTests 回归通过。

使用真实 DrawIcon/DrawWheel 路径离屏渲染了内置学派图标与独立资源副本中的 PNG 替换。历史预览位于 `build/icon-schools-v5-preview.jpg`（其中龙吼已恢复初版，该图不代表当前龙吼外观），替换图局部位于 `build/icon-pack-detail.jpg`。这些是离屏渲染验证，不代表游戏内验收；仍需实际收藏各学派法术并检查显示、主题切换与模组法术分类。

## 图标视觉修订

恢复系改为双手托护治疗十字，移除太阳光芒；毁灭系整理为分离的内外火焰轮廓；幻术系简化眼睛与瞳孔；龙吼按用户要求恢复为最初的喇叭加两道声波版本，供下一轮游戏测试；其余图标修改保留。真实绘制路径的离屏预览已检查，游戏内效果仍待用户验收。

## 本轮测试主题：Icon Pack Test

重启游戏，在轮盘设置中选择 `Icon Pack Test` 主题并应用。该主题独立于用户原有主题，不自动改变当前选择。测试完成后切回原主题。

编号对照：1 变化、2 召唤、3 毁灭、4 幻术、5 恢复、6 龙吼、7 力量/次级力量、8 杂项。2 是透明白色线稿，随主题染色；其他编号保留彩色原图。书籍及未映射分类继续使用内置图标，用来检查部分覆盖与回退。

原版治疗术 `Skyrim.esm|00012FCC` 单独映射到青色 9，优先于恢复系的绿色 5。其他恢复系法术仍显示 5，例如快速治疗 `player.addspell 0002F3B8`。将其收藏即可对照测试。

修复了闭合曲线末点重复起点造成的零长度线段，涉及幻术、毁灭和杂项接缝；召唤图标中央竖线已接到菱形下端。离屏检查图：`build/icons-seams-fixed.jpg`、`build/icons-pack-test.jpg`。本轮之前用户已确认基本功能测试正常，本轮接缝和图标包的游戏内验收仍待反馈。
