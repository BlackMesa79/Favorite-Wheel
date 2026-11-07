# 0.8.0 发布包记录

日期：2026-10-25。用户要求将循环战斗预设、中间区域所属组提示和此前合入 0.7.3 的 controller-chord 修复整合为 0.8.0。本记录描述本地构建，不表示已上传或公开发布。

## 本次定版

包含收藏杂物显示合并和吸血鬼领主技能白名单/功能限制，保留循环预设与中心提示。无额外领主音效、暂停时序试验或能力键诊断。PureDark/TRP 可见性问题未修复；不声称修复其他模组的 T-pose。详情页更新在 release-materials/0.8.0，安装包和源码包更新在 dist。本记录仅确认本地发布包，不代表线上上传。

## 内容

- 整合八个战斗循环组、成员管理、组合键及角色 co-save 持久化。用户已在游戏中测试此前循环版本，反馈没有发现问题；未提供逐项清单，不扩大为所有版本和边界均验证通过。
- 悬浮战斗预设或循环成员时，在轮盘中间显示所属循环组编号。多个组全部列出；未加入则隐藏。扇区不增加文字或标记。复用已有本地化标题，编号不随组名或快捷键改变。
- 保留 0.7.3 的 controller-chord 修复：原生当前手柄包只在含同一主键时补足入口修饰键，修复临时 LB release 导致 LB+Y 打开原版收藏菜单的问题，保留真实释放、单按 Y 和非原生设备的事件路径。
- 保留当前工作区已有的换装保存诊断。先前整体优化建议记录于 `CODE_REVIEW_2026-10-24.md`，未在本次开展整体重构。

## 补丁核对

直接读取 `dist/FavoriteWheel-0.7.3-source.zip` 比较：

- `include/InputBindings.h` 与正式源码内容一致（忽略 CRLF/LF）。
- `NativeGamepadModifiers`、`OpeningControllerHeld`、`ReplacedEntrance`、`ToggleKey` 四个函数与正式源码一致。
- 输入回调中的原生设备判定和 ControllerSample 采样代码一致。
- 修改版本预期前，`RuntimeLayoutTests.cpp` 与正式源码一致；0.8.0 将导出版本预期更新为 0.8.0。
- WheelLogicTests 保留原补丁覆盖的临时 LB 释放、真实释放、缺主键、不可用采样和非原生设备场景。

## 验证范围

新提示已通过中文 classic（全部八组）和英文 skyrim（多组）的 1280x720 WARP 实际离屏渲染，名称、装备状态和组提示没有重叠，扇区保持原布局。预览保存在 `build/review-cycle-membership-cn.jpg` 和 `build/review-cycle-membership-en.jpg`。用户随后确认中心组提示、杂物堆叠及吸血鬼领主轮盘测试正常；修复 T-pose 后还原形态恢复。音效试验已按要求全部撤回，回滚版经用户复测通过。

构建、回归、安装和归档的最终结果记录在 `release-materials/0.8.0/verification.txt`。

升级时停用旧的独立 controller-chord、combat-cycle 和换装保存诊断测试补丁，避免旧 DLL 覆盖 0.8.0。保留用户主 INI、自定义语言主题和角色 `.skse` 配套存档；安装包不附带主 INI。
