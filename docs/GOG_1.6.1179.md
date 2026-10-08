# GOG 1.6.1179 支持（0.5.2测试版）

2026-10-08：用户请求兼容GOG游戏本体1.6.1179。原0.5.1与之前的运行时白名单没有该版本，加载函数会直接返回false。0.5.2精确接受1.6.1179.0，其他未审计的GOG版本仍拒绝；不泛化到所有GOG运行时。

## 依赖与代码依据

固定CommonLibSSE-NG v11.0.0提交94faaed0c60eddd8347767f2d4d29a97c93bde8c已声明RUNTIME_SSE_1_6_1179（GOG），运行时按AE分类。1.6.1179使用AE地址ID，但数据库必须为versionlib-1-6-1179-0.bin，不能用Steam1170库替换。无需改动或升级上游依赖，也不需要单独的GOG DLL。

按上游分版本访问器，1179的ControlMap数据起点0xF0、PlayerRuntimeData起点0x3E0、ActorState起点0xC0、Actor运行块起点0xE8；1.7的玩家数据0x3E8不应用到1179。死亡/Potion/Update虚表槽为0x99/0x10F/0xAD。输入所在函数继续AE ID68617，派发call偏移0x7B，安装时仍检查原生E8 call签名并保留前一个钩子。

白名单、版本元数据/日志及测试目标更新，游戏行为、收藏入口、时间、预设、输入隔离与0.5.1上下文修复保持。没有新增地址ID、签名扫描、引擎补丁、配置/翻译键或存档格式。面部光照与Text Bridge仍是可选联动，它们本身也需要支持GOG版本。

## 安装要求

- Skyrim GOG 1.6.1179.0。
- **GOG版SKSE64 2.2.6**。2026-10-08核对[SKSE官网](https://skse.silverlock.org/)的GOG构建标为2.2.6、game version1.6.1179；不能只因Steam构建也叫2.2.6就混用。
- Address Library包含SKSE/Plugins/versionlib-1-6-1179-0.bin。不要从轮盘包内寻找地址库；安装匹配版本的Address Library。
- 其他要求与现有发布一致，保留自己的INI和matching .skse存档。

## 验证结果与边界

RuntimeLayoutTests新增1179独立原生字节夹具、AE选择、控制映射/玩家/角色/按钮/渲染/快捷槽布局，并可追加1179真实地址库。既有可选库顺序不变：1.5.97、1.6.1170、1.7.99、1.7.104、1.6.640，最后1179。ActorRuntimeTests增加1179下IsDead、DrinkPotion、Update槽派发。1178/1180/1179.1及旧GOG659继续拒绝。

本机存在1179正式地址库：796422字节，格式2、pointer size8、428510条；文件头版本1.6.1179.0、模块SkyrimSE.exe。SHA256：3EE46B2F3A8A24B9CDA1F2AA63B0F0F47DEA347E52701A6739327E5EA1B5838E。通过CommonLib实际IDDB解码检查关键ID，不把合成夹具当成真实地址库证据。

没有本机GOG游戏可执行文件或1179实机运行。地址解析无法证明+0x7B处指令、所有游戏虚函数或第三方钩子组合必然有效。对外状态为**支持GOG1.6.1179，尚待实机测试**，不标为已测试。

2026-10-08本地验证完成：Release编译成功，RuntimeLayoutTests使用上述六个运行时的真实地址库全部通过（包括1179的20个关键ID及PlayerCharacter虚表解析），六版模拟布局、地址库格式1/2/5及实际0.5.2 DLL导出元数据通过。ActorRuntimeTests六版槽派发、WheelLogicTests与TimeTests通过。测试日志留在本机build/gog052-runtime-tests.txt，不随发行包提供。

已自动部署到H:/Games/Dev Skyrim/mods/36 - FavoriteWheel。构建与安装DLL的SHA256一致：9886C559D94DB359905A35BA4CEE8D4FD7C7D493A7CC607407A1271501ECBDA8。部署前后8份已有配置、语言、主题及MO2 meta.ini哈希保持一致。没有界面绘制变化，无需重复离屏截图；上述本地验证不代替1179实机检查。

## 给GOG测试者

1. 安装0.5.2测试包及匹配的GOG SKSE/Address Library，启动游戏后检查日志runtime=1-6-1179-0、family=AE与renderer/input/player-update钩子正常。
2. 当前游戏Favorites键打开收藏，Shift+该键打开功能；独立自定义键和手柄（有设备时）抽查。默认不是固定物理Q，映射不同按0.5.1说明处理。
3. 普通收藏/全部背包分类、数字1–8槽、物品右/左手装备与卸装、药水/食物、法术/龙吼/能力正常。
4. 保存/应用/卸下预设，手动穿齐标记、保存与重新读档保持；原有.ini/主题/翻译不重置。
5. 暂停、减速、正常模式及设置/命名窗口，关闭恢复时间/移动正常；轮盘按钮不漏到攻击或声音能力，QuickLoot上下文如有安装也抽查。
6. 可选面光和Text Bridge只有各自DLL支持1179时再检查，不把它们无法加载当作轮盘核心不可用。

失败提供FavoriteWheel.log、SKSE加载日志和（如有）崩溃日志。若input call签名拒绝，保持原版菜单；需1179可执行文件附近的真实指令依据再修正，不能盲目猜偏移。
