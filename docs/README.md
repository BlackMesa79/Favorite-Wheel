# 文档索引

更新：2026-10-28（接手审查 task 20261028-takeover-review）。当前版本 0.9.0。

**先读这三份**：项目根目录的 `HANDOFF_CURRENT.md`（当前交接及“不要回归的修复”）、[ARCHITECTURE.md](ARCHITECTURE.md) 末尾的“2026-10-28 接手补充”（模块、线程、序列化、INI、构建部署）、[reports/20261028-takeover-review-audit.md](reports/20261028-takeover-review-audit.md)（最新审查）。

说明：

- 下表中的“状态”只表示文档的时效。“历史”表示记录的是当时的调查、测试或方案，其中提到的“当前”“待测”不代表今天仍然成立。
- “已被取代”指文档的结论被后续实现或文档替换，取代者在说明列中注明。
- 本索引不改动各文档正文；项目根目录的 `HANDOFF.md` 是旧交接，已被 `HANDOFF_CURRENT.md` 取代。

## 1. 长期参考（按专题，仍然有效）

| 文档 | 内容 | 状态 |
|---|---|---|
| [ARCHITECTURE.md](ARCHITECTURE.md) | 工程说明，包含 2026-10-28 现状补充 | 有效（以末尾补充节为准） |
| [TESTING.md](TESTING.md) | 历次发布验证与游戏内测试清单（倒序日志） | 有效（历史日志，越往下越旧） |
| [LOCALIZATION.md](LOCALIZATION.md) | 语言文件规范与翻译指南 | 有效 |
| [FRENCH_LOCALIZATION.md](FRENCH_LOCALIZATION.md) | 法语译文来源 | 有效 |
| [THEME_AUTHORING_0.6.0.md](THEME_AUTHORING_0.6.0.md) | 主题制作（颜色/组件、材质、形状、详情位置） | 有效 |
| [RESOURCES.md](RESOURCES.md) | 语言与主题资源的安装布局（0.3.14 起） | 有效，主题键以 THEME_AUTHORING 为准 |
| [INPUT_CONTROLS.md](INPUT_CONTROLS.md) | 入口、组合键、手柄、数字快捷槽 | 有效（文件开头是 0.5.x 记录） |
| [MOVEMENT_INPUT.md](MOVEMENT_INPUT.md) | 打开轮盘时的移动键与摇杆规则（0.4.4） | 有效 |
| [CONTROLLER_SPLIT_STICKS.md](CONTROLLER_SPLIT_STICKS.md) | 左杆移动 / 右杆选择 | 有效 |
| [TIME_MODE.md](TIME_MODE.md) | 暂停/减速/正常速度，暂停守卫 | 有效，0.4.2 段落中的菜单上下文描述已过时（文中已注明） |
| [SLOW_TIME_PHYSICS_0.5.3.md](SLOW_TIME_PHYSICS_0.5.3.md) | 50% 物理下限依据 | 有效 |
| [INVENTORY_MODE.md](INVENTORY_MODE.md) | 全部背包模式的目录与详情缓存 | 有效 |
| [ITEM_INFORMATION.md](ITEM_INFORMATION.md) | 详情卡数据契约 | 有效 |
| [HAND_EQUIPMENT_STATUS.md](HAND_EQUIPMENT_STATUS.md) | L/R/L+R/2H 手位标记 | 有效 |
| [OUTFITS.md](OUTFITS.md) | 换装预设 | 有效 |
| [COMBAT_PRESETS.md](COMBAT_PRESETS.md) / [COMBAT_QUICK_SLOTS.md](COMBAT_QUICK_SLOTS.md) / [COMBAT_CYCLES.md](COMBAT_CYCLES.md) | 战斗预设、快捷槽、战斗循环 | 有效 |
| [FACELIGHT_WHEEL.md](FACELIGHT_WHEEL.md) | 面部光照功能轮盘 | 有效 |
| [ICON_DESIGN.md](ICON_DESIGN.md) | 图标设计 | 有效 |
| [CommonLibSSE-NG-REVISION.txt](CommonLibSSE-NG-REVISION.txt) | 固定的依赖版本 | 有效 |

## 2. 0.9.0 功能（当前版本）

| 文档 | 内容 |
|---|---|
| [RELEASE_0.9.0.md](RELEASE_0.9.0.md) | 发布范围与限制 |
| [INVENTORY_SORTING_V1.md](INVENTORY_SORTING_V1.md) | 分类排序、T/Shift+T、F2 |
| [DRAG_SORT_V1.md](DRAG_SORT_V1.md) | 手动排序拖拽 |
| [NUMERIC_SORT_AND_HINT_MIGRATION.md](NUMERIC_SORT_AND_HINT_MIGRATION.md) | 数值排序与旧提示迁移 |
| [WHEEL_MEMORY_AND_ZERO_HEALTH_OUTFITS.md](WHEEL_MEMORY_AND_ZERO_HEALTH_OUTFITS.md) | 轮盘记忆、零值换装 |
| [SPELL_RESELECT_TOGGLE.md](SPELL_RESELECT_TOGGLE.md) | 法术再次选择时解除装备（取代 0.7.1 的双持法术行为） |
| [FIRST_OPEN_FONT_WARMUP.md](FIRST_OPEN_FONT_WARMUP.md) | 首次开盘字体预热 |

## 3. 审查与计划

| 文档 | 内容 | 状态 |
|---|---|---|
| [reports/20261028-takeover-review-audit.md](reports/20261028-takeover-review-audit.md) | 接手全项目审查 | 最新 |
| [plans/20261028-takeover-review.md](plans/20261028-takeover-review.md) | 修复计划草案（等待用户挑选） | DRAFT |
| [plans/_template.md](plans/_template.md) | 计划模板 | — |
| [CODE_REVIEW_2026-10-26.md](CODE_REVIEW_2026-10-26.md) | 0.9.0 发布前审查 | 历史；遗留项已并入最新审查 |
| [CODE_REVIEW_2026-10-24.md](CODE_REVIEW_2026-10-24.md) | 全面审查 | 历史；#2/#3/#4/#6 和 P1 打包问题仍未修复，见最新审查的 A-02、A-08、A-16、A-17、A-18 |
| [COMPATIBILITY_AUDIT_2026-10-24.md](COMPATIBILITY_AUDIT_2026-10-24.md) | 上采样、兽化、乐器堆叠反馈检查 | 历史 |
| [MOD_COMPATIBILITY_0.5.3.md](MOD_COMPATIBILITY_0.5.3.md) | 0.5.3 兼容性审查 | 历史 |
| [REVIEW_0.4.3.md](REVIEW_0.4.3.md) | 0.4.3 代码复核 | 历史 |
| [COMMONLIB_1.7_REVIEW.md](COMMONLIB_1.7_REVIEW.md) | CommonLib v11 / 1.7 适配 | 历史，依赖版本至今未变 |

## 4. 发布记录（历史）

[RELEASE_0.5.0](RELEASE_0.5.0.md) · [0.5.1](RELEASE_0.5.1.md) · [0.5.2](RELEASE_0.5.2.md) · [0.6.0](RELEASE_0.6.0.md) · [0.6.1](RELEASE_0.6.1.md) · [0.6.2](RELEASE_0.6.2.md) · [0.7.0](RELEASE_0.7.0.md) · [0.7.1](RELEASE_0.7.1.md) · [0.7.2](RELEASE_0.7.2.md) · [0.7.3](RELEASE_0.7.3.md) · [0.8.0](RELEASE_0.8.0.md) · [0.9.0](RELEASE_0.9.0.md)；合并测试记录 [TEST_0.5.0_CANDIDATE.md](TEST_0.5.0_CANDIDATE.md)。

## 5. 运行时兼容（历史记录，支持范围以 ARCHITECTURE 补充节 §2 为准）

[RUNTIME_COMPATIBILITY.md](RUNTIME_COMPATIBILITY.md)（0.3.18 / 0.5.2）· [TEST_1.5.97.md](TEST_1.5.97.md)（0.3.13）· [GOG_1.6.1179.md](GOG_1.6.1179.md)（0.5.2）· [GOG_1.6.659.md](GOG_1.6.659.md)（0.6.2）

## 6. 专题调查与测试版记录（历史，按版本）

| 版本 | 文档 | 备注 |
|---|---|---|
| 0.2.x | [TEXT_INPUT.md](TEXT_INPUT.md) | 名称输入兼容 |
| 0.3.x | [WHEEL_ANIMATION.md](WHEEL_ANIMATION.md)、[I4_ICONS.md](I4_ICONS.md) | |
| 0.4.x | [OUTFIT_MATCHING_0.4.5.md](OUTFIT_MATCHING_0.4.5.md)、[OUTFIT_WEAR_BADGE_0.4.6.md](OUTFIT_WEAR_BADGE_0.4.6.md)、[CONTROLLER_PAGING_0.4.7.md](CONTROLLER_PAGING_0.4.7.md)、[MAGIC_CATEGORIES_0.4.8.md](MAGIC_CATEGORIES_0.4.8.md)、[INPUT_ISOLATION_0.4.9.md](INPUT_ISOLATION_0.4.9.md) | |
| 0.5.x | [ENTRY_CONTEXT_0.5.1.md](ENTRY_CONTEXT_0.5.1.md)、[CONFIG_INITIALIZATION_0.5.2.md](CONFIG_INITIALIZATION_0.5.2.md)、[EMPTY_CATEGORIES_AND_OUTFITS_0.5.2.md](EMPTY_CATEGORIES_AND_OUTFITS_0.5.2.md)、[LOCALIZATION_EXPANSION_0.5.2.md](LOCALIZATION_EXPANSION_0.5.2.md)、[SETTINGS_REFINEMENTS_0.5.2.md](SETTINGS_REFINEMENTS_0.5.2.md)、[ANIMATION_PERFORMANCE_0.5.3.md](ANIMATION_PERFORMANCE_0.5.3.md)、[CATEGORY_HITCH_AND_EMPTY_INI_0.5.3.md](CATEGORY_HITCH_AND_EMPTY_INI_0.5.3.md)、[CLASSIC_GOLD_REDESIGN_0.5.3.md](CLASSIC_GOLD_REDESIGN_0.5.3.md)、[CONTINUOUS_USE_0.5.3.md](CONTINUOUS_USE_0.5.3.md)、[LAYOUT_SKINS_0.5.3.md](LAYOUT_SKINS_0.5.3.md)、[POISON_CATEGORY_0.5.3.md](POISON_CATEGORY_0.5.3.md)、[SKIN_MATERIALS_0.5.3.md](SKIN_MATERIALS_0.5.3.md)、[SKYRIM_SKIN_0.5.3.md](SKYRIM_SKIN_0.5.3.md)、[TORCH_PAUSE_POISON_0.5.3.md](TORCH_PAUSE_POISON_0.5.3.md)、[SETTINGS_PERSISTENCE_0.5.4.md](SETTINGS_PERSISTENCE_0.5.4.md) | 0.5.1–0.5.4 是发布前的测试编号，已并入正式版 0.5.1/0.5.2 |
| 0.6.x | [CONFIG_SAVE_0.6.0_FOLLOWUP.md](CONFIG_SAVE_0.6.0_FOLLOWUP.md)、[CONTROLLER_CHORDS_0.6.1.md](CONTROLLER_CHORDS_0.6.1.md)、[MOUSE_HANDS_0.6.2.md](MOUSE_HANDS_0.6.2.md) | |
| 0.7.x | [COC_STARTUP_0.7.0.md](COC_STARTUP_0.7.0.md)、[PIXELATED_TEXT_4K.md](PIXELATED_TEXT_4K.md)、[INVISIBLE_WHEEL_AND_LB_Y_0.7.0.md](INVISIBLE_WHEEL_AND_LB_Y_0.7.0.md)、[RESTORE_STABLE_RENDERER_0.7.0.md](RESTORE_STABLE_RENDERER_0.7.0.md)、[CROSS_HAND_EQUIP_0.7.1_TEST.md](CROSS_HAND_EQUIP_0.7.1_TEST.md)、[WEAPON_STACK_HANDS_0.7.1_TEST.md](WEAPON_STACK_HANDS_0.7.1_TEST.md)、[CONTROLLER_NATIVE_CHORD_0.7.2_TEST.md](CONTROLLER_NATIVE_CHORD_0.7.2_TEST.md)、[OUTFIT_SAVE_DIAGNOSTICS_0.7.3.md](OUTFIT_SAVE_DIAGNOSTICS_0.7.3.md) | INVISIBLE_WHEEL 中的“菜单绘制方案”已撤回，以 RESTORE_STABLE_RENDERER 为准 |
| 0.8.x | [MISC_STACK_AND_VAMPIRE_LORD_TEST.md](MISC_STACK_AND_VAMPIRE_LORD_TEST.md) | 文中的音效实验已于 2026-10-25 撤回，禁止恢复 |
| 其他 | [GPU_LOAD_REPORT.md](GPU_LOAD_REPORT.md) | 1080 Ti 负载反馈，未据此修改 Present |

## 7. 已被取代或仅供参考的方案

| 文档 | 取代者 / 说明 |
|---|---|
| [NATIVE_UI_TARGET_REGRESSION_0.7.0.md](NATIVE_UI_TARGET_REGRESSION_0.7.0.md) | 已撤回；见 [RESTORE_STABLE_RENDERER_0.7.0.md](RESTORE_STABLE_RENDERER_0.7.0.md) |
| [OUTFIT_PREVIEW_DESIGN.md](OUTFIT_PREVIEW_DESIGN.md) | 静态照片方案已放弃；用户偏好见 [REALTIME_OUTFIT_PREVIEW.md](REALTIME_OUTFIT_PREVIEW.md)（实时试穿只是调研，没有实施） |
| [UI_RENDERING_OPTIONS.md](UI_RENDERING_OPTIONS.md) | 2026-10-04 的方案评估；已选定继续使用 ImGui 并在 0.3.3 实施 |
| [SLOW_TIME_AND_INVENTORY_SCOPE.md](SLOW_TIME_AND_INVENTORY_SCOPE.md) | 可行性评估；已由 [TIME_MODE.md](TIME_MODE.md) 和 [INVENTORY_MODE.md](INVENTORY_MODE.md) 实现 |
