# 0.5.1 入口按键与容器交互调查

此文记录发布前0.5.1测试版；正式0.5.1合并结果与包信息见[RELEASE_0.5.1.md](RELEASE_0.5.1.md)，本文构建哈希为历史值。

2026-10-08收到两个0.4.0版本反馈，本轮基于0.5.0继续修复，交付0.5.1测试包。0.5.0正式包保留。

## 1. Q和Shift+Q无响应：日志已确定是键位未匹配

提供的日志显示运行时1.6.1170、插件/渲染器/输入派发/玩家更新均初始化成功，按Q的多次记录均为：scan=16、mapped=51、match=false、blocked=none、override=-1。DirectInput扫描码51是逗号键，16是Q；事件为空字符串也表明该Q没有有效的收藏语义。

Hotkey=-1按设计跟随游戏Favorites映射，ActionHotkey=-1跟随有效收藏入口，所以此配置的默认入口是逗号和Shift+逗号。日志不能证明是玩家手工重绑、控制映射模组还是游戏自带绑定保存造成；Windows俄语显示语言不影响物理扫描码匹配，也不是现有证据中的故障原因。不能通过放宽到任意Favorites语义事件或另加隐式Q入口解决，否则会重新抢占数字快捷键/其他动作。

不需要新DLL即可解决：使用当前游戏收藏键，或者在Data/SKSE/Plugins/FavoriteWheel.ini中配置并重启：

```ini
[Controls]
Hotkey=16
HotkeyModifier=0
ActionHotkey=-1
ActionModifier=1
```

应检查MO2中实际生效的INI，包含Overwrite和后加载的文件覆盖。另一选项是直接把游戏Favorites绑定改回Q。没有修改反馈者配置或本机个人键位。

0.5.1读档日志补充effectiveFavorite/favoriteModifiers/effectiveAction/actionModifiers；默认跟随且不是Q时额外输出明确说明，避免按Q无反应时误判初始化失败。界面中的默认Q指“通常的原版键位”，并非强制物理Q。

## 2. 搜索容器的Q被抢：确认了一个通用控制组兼容性缺口

反馈者未给出模组名单或日志，无法确认确切实现。原版搜索容器通常为E；Q打开搜索/转移页面更符合QuickLoot一类的操作。不要把“准星内有容器时原版禁止收藏”当成已知原版规则。

只读检查本机QuickLoot IE 4.1.3配置/日志，并参考上游公开源码提交43baf0cc4d44fdcb89482d10050106d38f6595ae。来源为[QuickLootIE InputManager](https://github.com/MissCorruption/QuickLootIE/blob/43baf0cc4d44fdcb89482d10050106d38f6595ae/src/Input/InputManager.cpp)与[LootMenuManager](https://github.com/MissCorruption/QuickLootIE/blob/43baf0cc4d44fdcb89482d10050106d38f6595ae/src/LootMenuManager.cpp)。仅用于理解控制映射状态，没有复制其实现、API头、代码或资源，也不增加依赖。

上游会将与搜刮按键冲突的原生映射加入自定义userEventGroupFlag，在搜刮面板显示时ToggleControls禁用该组，离开后恢复。FavoriteWheel此前只检查IsMenuControlsEnabled的总菜单位，物理键匹配成功便开盘/吞事件，绕过附加禁用组。该缺口在0.4.0与0.5.0都存在；0.4.9解决的是已开轮盘时向下游漏事件，与这个入口优先级问题不同。

## 实现与review

- EntryControlsEnabled只检查候选入口物理键在Gameplay中的映射和enabledControls，标准组/插件附加组都必须启用。mapping含kInvalid表示不使用有效控制组，按无分组处理；缺上下文或没有该键的映射允许独立自定义入口。
- 关闭/空闲状态时，入口或被替换的原生收藏入口遇到禁用组，既不开盘也不吞按下/持续/松开，交给下游。原生入口要求修饰符但当前组合不匹配的分支也必须让出，不能只保护打开分支。
- 轮盘已经打开、排队打开、关闭等待，或此前已吞的按钮，保持既有完整周期防穿透。内部翻页/分类/关闭不因为某插件切换控制组而停用。
- PendingOpen携带入口设备和原始码，真正开盘前在玩家更新中再次检查控制组，避免请求排队期间上下文已经改变。若新阻塞产生则取消，不伪造额外按键事件。
- 对键盘与手柄统一处理原始设备码，不用SKSE归一化手柄码去查原始映射。独立自定义键如果没有被原生控制组禁用，可以继续使用；若物理键本身被第三方禁用，其Shift+组合也让出。
- 入口日志blocked新增entry control group disabled，能区分键位不匹配和上下文禁用。不检查裸容器类型/准星，不硬编码LootMenu名称，不写游戏控制组，不修改QuickLoot设置。
- 开盘候选时扫描该设备Gameplay映射，成本随固定按键映射数增长，不遍历库存；没有新增每帧库存/角色扫描、地址、钩子、配置键、翻译键或存档格式。

如果第三方只在自己的事件监听器中吞键而没有使用ControlMap禁用状态，或它的状态变化晚于本轮盘处理，则本修复不能承诺覆盖。需要该用户的具体模组及新日志确认，不宣称所有QuickLoot分支或任意容器交互都已实测。

## 验证与复测

WheelLogicTests增加真实反馈的逗号映射/Q覆盖/default Shift组合策略；控制组包括原生菜单位仍启用但附加组禁用、离开后重新启用、无分组/无效哨兵、真实禁用位；检查让出的完整按钮周期透传与已吞入口保护。沿用原输入隔离、移动、数字槽、手柄和动作策略回归。真实引擎状态仍需复测。

反馈者测试：

1. 第一用户用逗号/Shift+逗号，或固定Hotkey=16后重启，确认收藏与功能轮盘正常。仍失败请提供实际生效INI与新日志的effective键位。
2. QuickLoot/容器用户对准容器并显示搜刮面板，按搜索键只进入容器；看向空地后同键打开轮盘，完整松键再试，键盘/手柄均抽查。
3. 禁用状态下同物理键的Shift+Q也让出；不冲突的独立入口依旧可开盘。已打开轮盘时LB/RB/上下等仍仅操作轮盘，不触发Horde或攻击。
4. 无QuickLoot时对准普通容器，收藏入口保持原设计；搜索/激活键照常使用。重绑收藏键、原生数字快捷槽、移动恢复、预设命名/IME回归。

本轮未启动游戏，不把这些新行为标为实机通过；1.6.640/1.7实机状态不变。

## 本地交付记录

Release构建成功并自动部署至指定36 - FavoriteWheel目录。DLL SHA256为57C9CE528582E6056C210E706670550F6CFB607B2F9BDCEC73F8F01A0339D70F，与安装一致；现有8个配置文件哈希保持。旧正式DLL备份在本地build/FavoriteWheel-before-051.dll，配置基线在build/entry051-preserved.json。0.5.0正式ZIP保留。没有绘制/资源变化，不重复离屏渲染。

WheelLogicTests、TimeTests、SettingsTests、NameEditorTests、RuntimeLayoutTests通过；布局验证包括五支持运行时模拟布局、地址库格式1/2/5和实际0.5.1导出元数据。Review检查默认/自定义入口、无效分组哨兵、原生入口修饰分支、完整周期透传、已捕获输入保护及排队再次校验，没有发现新的阻断问题。QuickLoot真实交互仍待实机确认。
