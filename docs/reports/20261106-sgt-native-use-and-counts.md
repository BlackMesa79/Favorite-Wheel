# SGT 乐器使用入口与数量修复

日期：2026-11-06。

## 用户实测结论

用户已确认四分类 SKSE Menu 设置菜单可用，本轮未改动菜单。库存点击乐器能演奏，轮盘点击不能；库存有 3 个鼓、2 个 Flute，而轮盘分别显示 ×1。

## 使用入口

读取本次游戏日志，确认鼓、笛子、鲁特琴均到达 `SGT instrument native use requested`，并非输入未识别或物品未被支持。但旧日志只证明请求提交，不能证明脚本触发。

对本地 1.5.97 可读 EXE 静态核查原生调用：

- FavoritesMenu::UseQuickslotItem，ID 50654、RVA 0x8793D0，在 +0xC4 调用 ToggleEquipItem（ID 37951，RVA 0x638710），其装备分支传 `queueEquip=false`。
- 原有轮盘调用省略参数，使用 `queueEquip=true`。EquipObject（ID 37938，RVA 0x637A80）经过管理器内部调用，进入 Actor 排队函数 RVA 0x5F82E0。MISC（0x20）在跳转表中落到 0x5F8387，直接返回。
- 非排队分支进入 Actor 函数 RVA 0x60B9E0，MISC 的分支最终执行 0x60C2A6 的原生事件发送调用；目标 RVA 0x61E420 向 ScriptEventSourceHolder +0x478（TESEquipEvent 源）派发 equipped=true。
- 本轮将限定 SGT 乐器入口改为 `EquipObject(player, object, extra, 1, nullptr, false, false, true, false)`，匹配库存菜单参数。继续在轮盘关闭后重新校验实例；不手动派发 Papyrus/装备事件，不直接添加演奏能力，不更改 SGT。
- 用户当前日志运行时为 1.6.1170。其磁盘 EXE 受保护，未取得有效对应函数反汇编；以上静态流程来自 1.5.97，不能冒充 1.6.1170 游戏内验证。本轮仍使用已有跨版本 EquipObject API，没有增加引擎地址或 hook。

## 数量

修正两条漏计路径：有 UniqueID 的普通乐器不能作为剩余未跟踪数量的锚点；收藏模式提前丢弃同 Form 未收藏副本，导致只显示带收藏标记的那一份。

现在限定 SGT 乐器先收集副本，按 FormID、名称、所有者及兼容的额外数据合并显示。未绑定收藏标记 255 与无标记副本可合计；优先保留真实收藏实例作为操作锚点，最后再执行收藏模式筛选。允许 UniqueID 不同的普通乐器承担剩余数量，但独立绑定或扩展标记不能吸收这些数量。

独立快捷键、Important 标记、不同名称/所有者、未知 ExtraData 仍保持边界。不合并实际 ExtraDataList，不修改收藏标记，不以显示总数执行批量使用。其他物品沿用原有规则。

新增 `SGT instrument display` 日志记录每行数量、收藏状态和实例地址，便于下一次实测核对。当前没有读取实际存档的逐实例 ExtraData，具体现场数据仍以新日志为准。

## 后台验证与部署

- InventoryTests 通过，新增 3 鼓/2 笛子、各仅一份收藏、两种收集顺序、未跟踪副本、操作锚点及分组边界回归。
- WheelLogicTests 通过，包含延后动作与装备策略。
- Release 构建及 after_build 部署成功，目标 `H:/Games/Dev Skyrim/mods/36 - FavoriteWheel`。
- DLL 构建/部署 SHA256 一致：`8E24F591ACBB82BBB4B25AA5677FD01363DA16D535B34328B648D78E05FB7CDC`。
- 部署前后 26 个配置/资源文件哈希全部不变。旧 DLL 备份：`build/FavoriteWheel-before-sgt-native-fix-20261106.dll`。
- 校验记录：`build/sgt-fix-verification.json`。静态调查文件：`build/sgt-disasm-638710.txt`、`build/sgt-disasm-5f82e0.txt`、`build/sgt-disasm-60b9e0.txt`。
- 未启动游戏、未接管键鼠，不将静态检查或后台测试写成游戏内通过。

## 下一次实测

重启游戏加载新 DLL，先核对鼓 ×3、Flute ×2，再分别使用鼓、笛子、鲁特琴及重复使用同一乐器。观察轮盘关闭后是否开始/重置演奏。新使用日志应含 `queueEquip=false count=1`；若仍不触发，再结合这些行和库存入口对照继续定位。

## 用户后续确认

2026-11-06，用户明确反馈本轮修复经游戏测试没有问题。SGT 乐器演奏触发与数量显示按用户实测结果记为通过；不扩展为其他吟游模组也已联测。
