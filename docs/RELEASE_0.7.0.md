# 0.7.0 发布记录

日期：2026-10-13。用户确认自己的本地测试通过，包括鼠标左右手装备切换和战斗预设快捷键分配，授权将当前改动整理为 0.7.0。字体硬边问题在用户自己的游戏中无法复现，因此未进行该问题的游戏内测试；此前已在 4K 原生离屏渲染中复现并验证修复，原反馈者环境结果仍待确认。

## 发布范围

- 战斗预设：保存并恢复左右手武器/法术、混合组合、盾牌/火把、双手装备和空手；交叉双剑图标，应用、重命名、覆盖、删除。
- 共享 1–8 战斗快捷槽：悬浮绑定/解除，游戏内按键应用，处理物品/预设与后来 SkyUI/原版物品绑定的冲突。独立绑定记录保存到角色 `.skse` 存档，保持原战斗装备记录格式。
- L/R/L+R/2H 手位显示、槽号与长名称间距、可选左键左手/右键右手。
- 手柄修饰键跨输入包保留与生命周期清理；GOG 1.6.659 精确兼容。
- 缺失/无效字体优先回退到系统字体，分类箭头改为矢量绘制，补齐原生字体尺寸及诊断。有效自定义字体保留。
- 包含前序配置保存、物品使用、性能和主题支持改动。主题制作接口沿用 [0.6.0 指南](THEME_AUTHORING_0.6.0.md)，安装包仍提供 Classic Gold、Skyrim、Skyrim Polygon。

使用细节见 [战斗快捷槽](COMBAT_QUICK_SLOTS.md)、[战斗预设](COMBAT_PRESETS.md)、[字体调查](PIXELATED_TEXT_4K.md)。

## 验证

用户确认上述游戏测试通过，但未提供逐项案例和运行时矩阵；不扩展为所有管理器/运行时组合均已实测。GOG 1.6.659 等此前标注待实机验证的运行时保持原状态。

0.7.0 Release 构建成功，16 项本地套件全部通过：Settings、SettingsPersistence、SettingsVfs、WheelLogic、Time、RuntimeLayout、Inventory、Outfit、NameEditor、ItemInfo、FaceLightClient、ActorRuntime、SkinTexture、LayoutSkin、SectorMesh、CombatPreset。RuntimeLayout 使用七份真实地址库，并验证 DLL 导出版本为 0.7.0。版本升级未改变已验证的渲染和输入行为，因此沿用此前的 4K 字体、1080p/720p 快捷槽排版与缓存检查结果。

MO2 本地部署与原发布归档保护见 `release-materials/0.7.0/verification.txt`。构建/测试/部署日志位于 `build/release070-*`。包核验覆盖安装文件、主 INI 排除、三套主题、DLL 一致性与源码逐文件 SHA256 清单。

## 发布材料与升级

- `dist/FavoriteWheel-0.7.0.zip`：完整安装包，含资源与许可，不包含玩家主 INI。
- `dist/FavoriteWheel-0.7.0-source.zip`：对应源码、固定依赖、发布材料与 SHA256 清单。
- `release-materials/0.7.0`：英文 BBCode 详情页、HTML 预览、中英文简短 changelog、验证记录。

正常升级并停用独立的配置、手柄及战斗快捷槽测试补丁，避免旧 DLL 覆盖正式版。保留主 INI、自定义资源和配套 `.skse` 存档。已有战斗预设继续读取，没有新绑定记录时使用未绑定状态；预设/快捷槽编辑后保存游戏。原 0.6.x 和测试 ZIP 保留。

本次完成本地正式材料准备与 MO2 同步，未上传 Nexus、提交/推送 Git 或创建远程 Release。
