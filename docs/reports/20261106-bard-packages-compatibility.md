# BA Bard Songs 与 The Dragon Bard Ultimate 兼容核查

日期：2026-11-06。

## 结论

本次用户确认上一版 SGT 乐器触发和数量修复实测正常，SKSE Menu 四分类菜单也已通过用户实测。这个结果不自动扩展到本报告中的两个模组。

| 模组 | 包内实际入口 | 收藏轮盘是否需要单独兼容 |
| --- | --- | --- |
| BA Bard Songs 1.3 | 音频与 SNDR 声音描述扩充 | 不需要；不新增玩家物品使用入口 |
| The Dragon Bard Ultimate 提供包 | ARMO 乐器装备 + Play Instrument 次级能力 | 现有装备与能力路径已覆盖；静态核查未发现必须新增的专用适配，尚未游戏内验证 |

本轮没有改动功能代码、构建、部署或安装这两个模组。

## 安装包与检查方式

用户提供：

- `G:/Downloads/IDM/Compressed/DragonBardUltimate 151231 3.2 2026-09-25T16-54Z Dc5uGkmd2.zip`
- `G:/Downloads/IDM/Compressed/BA_BardSongs_FOMOD-47202-1-3-1625966592.7z`

只选择性解包插件、脚本和安装配置至 `build/feedback/DragonBardUltimate32`、`build/feedback/BABardSongs13`；未展开无关音频/贴图，未执行包内文件。Dragon Bard 包名标示 3.2，但随包 meta.ini 仍写 3.1.0.0，结论针对实际收到的文件内容。

Nexus 原页面本次 HTTP 读取返回 403，未以网页重新核实作结论依据；以下证据来自用户提供的安装包。

## BA Bard Songs

7z 清单内没有 PEX/PSC 或 SKSE DLL。四个插件的记录类型如下：

| 插件 | 记录 |
| --- | --- |
| BA_BardSongs_AIO.esp | TES4 ×1、SNDR ×13 |
| BA_BardSongs_Drums.esp | TES4 ×1、SNDR ×3 |
| BA_BardSongs_Flute.esp | TES4 ×1、SNDR ×6 |
| BA_BardSongs_Lute.esp | TES4 ×1、SNDR ×4 |

FOMOD 说明明确添加酒馆吟游诗人的随机器乐曲库，完整数量含原版为鼓 24、笛 36、鲁特琴 40。安装配置要求在 AIO 与 PICK 两条路线中选一；PICK 再选择各类乐器。无需为了轮盘同时安装 AIO 和全部分项。

该包没有新增 MISC、ARMO、SPEL 或玩家演奏脚本。单独安装它不会让玩家点击原版乐器开始演奏。与 SGT 一起使用时，玩家入口继续由 SGT 提供，轮盘已修复的 SGT 路径不需要因曲库变化而增加一次兼容。

本地 SGT 包内 meta.ini 作者说明明确要求 BA Bard Songs；这解释其曲库依赖，但未代表本轮已测试音频播放组合。

## Dragon Bard Ultimate

插件 `DragonBardUltimate.esp` 的主文件为 Skyrim.esm、Update.esm、Dawnguard.esm、HearthFires.esm、Dragonborn.esm。没有 MISC 记录；包含 ARMO ×17、SPEL ×18、MGEF ×18，以及场景、曲谱、任务等记录。

玩家乐器在 ARMO 中，包括 Violin、Violin (Ebony/Dragon/Daedric)、Lyre Harp、Kravik Lyre、Tagelharpa、Qanun、Qanun (Ornate)、Pan Flute、Handpan。乐器 ARMO 没有 VMAD；并非靠物品 OnEquipped 直接演奏。这些物品按轮盘现有装备分类显示，不能要求在 SGT 乐器的“其他”分类中找到。

`DBUPlayInstrumentSpell`（本地 FormID 0x01F3BB，文件内示例 0501F3BB）名称 Play Instrument。SPIT 中 SpellType=3，即 kLesserPower；ETYP=00025BEE（声音/能力槽）。轮盘 MagicCategory 已接纳 kLesserPower，使用时通过 EquipSpell 选中其作者设置的装备槽，遵循普通能力操作，不在点击时替玩家施放。

该能力的 EFID 指向 `DBUPlayInstrumentMagicEffect`，VMAD 挂载 `DBUPlayInstrumentScript`。本轮按 PEX 结构读取了函数体、指令和调用参数，并检查解析消费至文件末尾；不是只搜索字符串。其 `OnEffectStart`：

1. 检查已存在的乐器演奏效果，必要时 DispelSpell。
2. 对作者绑定的乐器逐一调用 akCaster.IsEquipped。
3. 按当前乐器调用对应 Song Menu Spell.Cast。
4. 无乐器时显示 `No instrument equipped!`。

VMAD 属性确认 Violin1/2/3/4、Lyre、Kravik、Tagelharpa、Qanun、OrnateQanun、PanFlute、HandPan 均绑定对应 ARMO；曲目菜单能力分别绑定相应 MGEF。抽查 `ViolinSongMenuScript.OnEffectStart` 调用 Menu(0)，由模组自己的脚本接管曲目菜单。

`DBUMagicInstrumentScript` 是另一个效果脚本，按已装备乐器提供 Melodies of the Dragon Bard 的短动画与魔法效果，也不能当成普通 SGT 杂项入口使用。

轮盘现有 Supported 已接纳 TESObjectARMO，原装备逻辑处理其穿戴；已收藏的主动能力通过 MagicFavorites 收集，kLesserPower 分到“能力”。因此目前不需要把 DBU FormID 加进 SGT 白名单，也不应强制从轮盘调用作者内部 PlaySong 或伪造事件。

## 游戏内验证步骤

1. 正常安装 DBU 及其要求，从作者流程取得玩家乐器和 Play Instrument。
2. 将目标乐器、Play Instrument 收藏。轮盘收藏模式仍依赖收藏标记；全部背包仅扩展物品来源，不自动展示所有未收藏能力。
3. 从装备分类穿戴 DBU 乐器；从“能力”分类选择 Play Instrument。
4. 关闭轮盘，按游戏的龙吼/能力键（默认 Z），观察曲目菜单与演奏；重复使用核对停止/切换行为。
5. 用库存/魔法菜单走相同步骤对照。若原入口正常而轮盘不同，再按乐器名称、能力名称和日志定位。

这是基于实际记录和脚本得出的“无需预先新增专用补丁”判断，不是游戏内联测通过。包内 NPC 道具、Do Not Use 能力、钢琴/竖琴场景设施不应作为通用便携乐器测试目标。

## 调查产物

- `build/bard-followup-20261106/DragonBardUltimate-records.json`
- `build/bard-followup-20261106/DragonBardUltimate-additional-records.json`
- `build/bard-followup-20261106/DBUPlayInstrument-vmad.json`
- `build/bard-followup-20261106/DBUPlayInstrumentScript-bytecode.json`
- `build/bard-followup-20261106/ViolinSongMenuScript-bytecode.json`
- `build/bard-followup-20261106/inspect-packages.py`
