# 用户反馈功能实施与游戏测试清单

日期：2026-11-06

## 本次实施范围

本次按既定顺序处理预设槽位、通知控制、人形 Voice 循环和狼人形态支持。

- Combat 预设的左右手和 Voice 槽位均可分别设置、忽略或卸下。Ignore 表示应用预设时保留该槽当前状态；Empty 表示将该槽卸下。预设记录升级为 FWC 2，旧 FWC 1 角色记录仍可读取并迁移。
- Voice 槽可保存和恢复龙吼或 Voice Power。应用过程单独读取、清理、装备并核验 Voice 槽，不将 Voice 与左右手状态混在一起。
- “装备与循环通知”设置控制 Combat 预设和 Cycle 操作产生的左上角 HUD 通知。各语言的预设槽位标签和说明已补齐。
- 狼人模式按玩家当前 Race 单独识别。动作轮盘可进入 Combat/Cycle 页面；收藏目录和动作权限仅开放已拥有的龙吼与 Voice Power。狼人形态下新捕获的预设将左右手固定为 Ignore，且只允许修改 Voice 槽。
- 对部署目录中与旧版默认值完全一致的 combatStorageHint 做了精确迁移。每种语言的部署前文件均保留 .before-controls-migration 备份；非匹配的自定义内容不会被迁移规则替换。

本次没有修改双手武器装备逻辑。当前收藏装备代码会根据武器双手标记选择原生双手装备槽，但这还不能确定它与 Dynamic Grip、Dual Wielding Unlocker 或其他装备改写模组之间的交互。

## 主要代码位置

- include/CombatPresetModel.h：槽位类型、FWC 1/2 编解码、步骤完成策略。
- include/FormMode.h、include/FormRuntime.h：狼人形态判定及受限动作白名单。
- src/CombatPresets.cpp：预设捕获、校验、编辑状态、应用和 Voice 操作。
- src/CombatCycles.cpp：循环预设应用与通知设置。
- src/Favorites.cpp：狼人形态 Voice 收藏目录过滤。
- src/Wheel.cpp：狼人形态轮盘入口、Combat/Cycle 页面和后台预设执行。
- assets/Languages/*.ini、scripts/language_migrations.lua：翻译和保留自定义内容的精确旧文案迁移。

## 自动验证与部署

2026-11-06 构建通过，部署到：

H:/Games/Dev Skyrim/mods/36 - FavoriteWheel

下列测试均通过：

- CombatPresetTests
- CombatCycleTests
- SettingsTests
- SettingsPersistenceTests
- LayoutSkinTests
- InventoryTests

部署 DLL 与构建产物 SHA-256 一致：

D9EEF9414EFCFE7931E6047A0DEB6777BD8CB990BB7A9584212E1DD64581D1AF

自动测试没有在真实 Skyrim 进程中调用装备管理器或 VM 回调；以下游戏内检查尚未完成。

## 游戏内测试清单

建议先备份存档。每项记录通过/失败；失败时保留当前形态、预设内容、操作步骤、游戏提示和相关日志。

### 1. 通知开关

- [ ] 在轮盘设置中关闭“装备与循环通知”。
- [ ] 应用一个 Combat 预设并切换一次 Cycle。
- [ ] 确认左上角不显示装备或循环通知。
- [ ] 开启该选项后重复操作，确认通知恢复。

### 2. 人形 Voice：龙吼

- [ ] 收藏一个已学会的龙吼，确认它出现在“龙吼”分类。
- [ ] 选择它并确认 Voice 装备成功。
- [ ] 保存一个左右手均为 Ignore、Voice 为该龙吼的预设。
- [ ] 换成另一个龙吼后应用预设，确认切换回原龙吼。
- [ ] 编辑 Voice 槽并测试 Set、Ignore、Unequip：Ignore 应保留当前状态；Unequip 应卸下龙吼。

### 3. 人形 Voice：能力

- [ ] 收藏一个已拥有的 Voice Power，确认它出现在“能力”分类。
- [ ] 选择并装备该能力。
- [ ] 保存 Voice 预设，切换到另一个能力后应用，确认能切回。
- [ ] 将 Voice 槽设为 Unequip 并应用，确认能力已卸下。
- [ ] 若卸下失败或预设超时，记录能力名称、当前 Voice 和提示。Voice Power 卸下调用尚未经过游戏内验证。

### 4. 左右手槽位状态

- [ ] 应用左右手均为 Ignore 的预设，确认当前双手装备不变。
- [ ] 分别测试左手和右手为 Unequip 的预设，确认只清空对应槽位。
- [ ] 保存手持装备后换装并应用，确认装备回到正确手槽。
- [ ] 应用双手武器预设，检查角色是否双手持握，第一人称武器是否可见。

### 5. Cycle 循环

- [ ] 创建一个 Cycle，加入至少两个 Voice 不同的 Combat 预设。
- [ ] 用循环快捷键切换，确认 Voice 按成员变化。
- [ ] 在狼人形态重复切换，确认循环继续运行且双手装备不变。

### 6. 狼人形态与 Howls Menu

- [ ] 变身后打开轮盘，确认能进入 Combat/Cycle 页面。
- [ ] 确认收藏动作只包含龙吼和 Voice Power；普通装备和消耗品不能执行。
- [ ] 在狼人形态保存 Voice 预设，确认左右手显示为 Ignore。
- [ ] 应用狼人形态下保存的预设，确认 Voice 切换正常。
- [ ] 变回人形，确认普通轮盘和装备操作恢复。
- [ ] 若安装 Howls Menu，测试两者切换龙吼时是否冲突或互相覆盖。

### 7. 双手武器问题

- [ ] 在未启用和启用解锁类模组的情况下分别测试；Dynamic Grip 可先保持关闭。
- [ ] 用收藏轮盘装备弓、双手剑或双手战斧，检查双手动作和第一人称模型。
- [ ] 再用 Combat 预设装备同一武器并检查。
- [ ] 若复现，记录武器名称、装备入口、启用的相关模组，以及第三人称/第一人称表现。

