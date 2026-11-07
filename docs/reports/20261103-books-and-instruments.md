# 书籍支持与乐器入口核实

## 已实现书籍支持

- TESObjectBOOK 纳入可用物品，沿用“其他”分类与卷轴图标；不修改分类编号及排序存档格式。
- 收藏模式仍只显示已收藏书籍；全部背包模式可直接访问书籍。本项目未新增给书籍设收藏标记的功能。
- 书籍快照标记阅读动作，通过现有关闭后执行队列运行；不受装备或消耗品保持打开设置影响。
- 执行前沿用 ResolveItem 校验当前物品及实例；普通书籍传当前 ExtraDataList 至 BookMenu::OpenMenuFromBaseForm。
- 法术书调用 TESObjectBOOK::Read，成功后从对应 ExtraDataList 消耗一本，失败不删除。不手动添加法术、不伪造 OnRead。
- 原生入口参考作者公开项目 FavoriteMiscItems 的书籍处理设计：https://github.com/powerof3/FavoriteMiscItems/blob/8c9f6c46a1a4dc0345768957450b42c0c248e0fd/src/Hooks.cpp 。其原生 ToggleEquipItem 前置调用与本项目入口不同，因此不能据此宣称所有脚本已兼容；OnRead、技能书行为必须实测。

## 验证结果

- Release 构建及 after_build 部署成功，WheelLogicTests 与 InventoryTests 通过。新增验证阅读不受保持打开设置影响；既有库存分配和精确实例测试继续通过。
- git diff --check 通过。
- 构建和部署 DLL SHA256 均为 600B3B1A18FAB45C373B5DE4E9EF0BA0892820ED2F142A2AED2003CD21351426。
- 当前主 INI SHA256 为 1CBC56EE9FFB670FB1107823B42494BF45D63C412450E6081DD90C8A19F205F6。本轮没有记录部署前哈希，不声称用哈希证明前后未变；继续沿用不复制示例主配置的 after_build。
- 未进行游戏内验证。待验收：普通书籍打开/关闭，技能书效果，未学/已学法术书消耗情况，以及 Become a Bard 曲集的 OnRead 学习菜单。

## Skyrim’s Got Talent 调查边界

作者文件页 https://www.nexusmods.com/skyrimspecialedition/mods/50357?tab=files 明确写从库存点击乐器开始演奏；更新记录 https://www.nexusmods.com/skyrimspecialedition/mods/50357?tab=logs 描述装备乐器时重置旧演奏。已确认用户操作入口，不等于已确认脚本事件及底层装备函数。

公开页面未取得脚本实现；本地 mods 顶层目录按 Talent/天赋/吟游/Bard/才华/乐器名称扫描未命中，不代表所有位置都没有安装。尚不能认定 OnEquipped 或某个内部任务函数为正确入口。需要可读取的实际模组包或安装路径继续核实；本轮不增加猜测性的杂项装备/脚本派发。

## 继续核实结果

- 额外检查开发 MO2 downloads 的包名及 profiles 文本，未找到 Skyrim’s Got Talent；已向用户请求实际模组包/安装目录路径。
- 再核对动作队列：书籍标记强制不保留轮盘，队列等待轮盘菜单和暂停退出，执行前重新解析实例。没有添加手动 OnRead 派发，避免与引擎事件重复。
- 构建/部署 DLL 再次哈希核对一致，仍为上述版本；本轮无代码变更，不重复构建。

### 最小游戏内验收步骤

1. 全部背包模式 → 其他 → 普通书籍：点击后轮盘关闭、书籍打开，退出阅读后输入正常。
2. 已收藏书籍在收藏模式仍可访问；未收藏书籍只在全部背包模式显示（设置收藏依赖已有提供者）。
3. 技能书首次阅读及再次阅读：核对技能提升、已读状态是否与背包入口一致。
4. 未学法术书：成功学习仅消耗一本；已学法术书不应额外消耗。
5. Become a Bard 曲集或其他菜单书籍：对照背包入口是否出现同样的 OnRead 交互菜单。若只有书页而无脚本菜单，保留 FavoriteWheel.log 的 Book menu requested 行和对应书籍名称，不先用伪造事件掩盖问题。

以上为待用户实测步骤，不作为已通过的测试记录。

## 用户提供 SGT 1.76 安装包后的实现

包路径：`G:/Downloads/IDM/Compressed/Skyrim's Got Talent - Improve As a Bard-50357-1-76-1725712975.zip`。只读解析，没有安装模组。

- 源码 _Talent_Givespell.psc 明确以 OnEquipped(Actor akActor) 为入口，仅接受 PlayerRef；移除已有 InstrumentEquipped 能力，重置两项演奏状态，再添加能力。
- _Talent_PlayInstrument.psc 的 OnEffectStart 启动演奏，包含等待退出菜单模式的循环。PEX 字符串表也存在相应事件名；未将字符串检查冒充完整反编译验证。
- ESP MISC/VMAD 确认挂载脚本的原版物品：000DABA7、000DABA9、000DABAB、00105177、00105109；另有三个模组 dummy 物品，本次不纳入。
- 已添加限定兼容：只有插件 SkyrimsGotTalent-Bards.esp 存在且属于上述原版 MISC 时可用；关闭轮盘后重新解析当前实例，调用一次 EquipObject(player,object,extra,1)。不伪造事件，不直接添加能力，不改动模组脚本。保留原有杂项显示聚合逻辑。
- 原生 EquipObject 到该脚本 OnEquipped 的运行时衔接尚未游戏实测；当前是待验收的兼容实现，不宣称演奏已通过。插件改名、不同脚本覆盖及其他新乐器不属于本次确认范围。
- Release 构建、InventoryTests、WheelLogicTests、diff 检查通过；已自动部署，DLL 哈希一致：15716B53ACF5324B46B0BE845ADAF5CF521497D00F7F27E78C4B0874582388A0。
- 当前主 INI 哈希仍为 1CBC56EE9FFB670FB1107823B42494BF45D63C412450E6081DD90C8A19F205F6。
- 游戏验收：分别点击鼓/笛/鲁特琴，确认轮盘关闭、开始演奏，重复使用能正常重置；若不触发，记录日志中的 SGT instrument native use requested 及物品名称。尚未修复多格显示反馈，未放宽实例合并条件。
