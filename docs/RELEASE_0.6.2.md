# 0.6.2 发布记录

日期：2026-10-13。用户授权把手柄组合键修复纳入 0.6.2，同时加入 GOG 1.6.659 兼容与可选鼠标左右手分配。本地正式安装包和源码包使用 0.6.2 新文件名，保留原 0.6.0 / 0.6.1 归档。

## 改动

- 纳入已获用户实机确认的战斗预设：角色独立保存双手组合，应用、重命名、覆盖、删除，图标采用 X 形交叉双剑。详情见 [战斗预设](COMBAT_PRESETS.md)。
- 纳入已获用户确认的手位显示与布局：L/R 靠近扇区内沿快捷键数字两侧，L+R/2H 位于外沿两侧。详情见 [手位显示](HAND_EQUIPMENT_STATUS.md)。
- 手柄按下/释放状态跨输入包保留，修复 X 与 Up 分别上报时修饰键丢失；同包主键先于修饰键也能识别。焦点丢失、读档、设备变更时清理，增加配置和状态日志。源码复现与测试通过，原手柄反馈者环境复测待反馈。调查沿用 [0.6.1 测试记录](CONTROLLER_CHORDS_0.6.1.md)。
- 新增精确 GOG 1.6.659 支持，已验证底层结构、虚函数调用与真实地址库；需 GOG SKSE64 2.2.3 及 659 地址库，实机验证待反馈。见 [兼容记录](GOG_1.6.659.md)。
- 新增 F2 → 键盘 → 交换鼠标装备手位，可开启左键左手、右键右手；物品操作提示同步变化。预设、对话框与手柄控制保持原语义，八种语言均已补齐。见 [使用说明与测试](MOUSE_HANDS_0.6.2.md)。
- 包含 0.6.1 的配置保存修复。现有主题与 0.6.0 主题制作接口继续使用。

## 验证与发布材料

16 项本地测试套件全部通过：Settings、SettingsPersistence、SettingsVfs、WheelLogic、Time、RuntimeLayout、Inventory、Outfit、NameEditor、ItemInfo、FaceLightClient、ActorRuntime、SkinTexture、LayoutSkin、SectorMesh、CombatPreset。RuntimeLayout 用七份真实地址库检查关键 ID，并确认 DLL 导出版本 0.6.2。SettingsVfs 使用独立 usvfs 测试实例，不连接玩家 MO2 会话。

本地 MO2 部署保留玩家主 INI、主题、材质及已有语言内容；八份语言文件各追加三个键。部署前备份与旧 ZIP 哈希记录于 `build/release062-baseline.json`。DLL SHA256：`82FFC2B056F792AE69D069FEF2C52F33621C48966812C12C6EB847EAC46AEB72`。

- `dist/FavoriteWheel-0.6.2.zip`：完整安装包，不包含玩家主 INI。
- `dist/FavoriteWheel-0.6.2-source.zip`：对应源码、固定依赖与逐文件 SHA256 清单。
- `release-materials/0.6.2`：英文详情页、HTML 预览、中英简短 changelog 与验证记录。主题指南沿用 `docs/THEME_AUTHORING_0.6.0.md`。

升级时停用单独的配置/手柄测试补丁，避免旧 DLL 覆盖正式版。保留主 INI、自定义主题和配套 `.skse` 存档。鼠标选项缺省为关闭，须在设置开启并 Apply。本次完成本地正式材料准备与同步，不代表已上传 Nexus、提交/推送 Git 或创建远程 Release。
