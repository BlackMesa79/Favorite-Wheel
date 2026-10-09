# 0.5.1 正式发布记录

日期：2026-10-09。公开名称：Favorite Wheel - Radial Actions。

用户确认最近各项改动测试正常，授权发布正式版0.5.1。本版以公开版0.5.0为基准，合并此前标为0.5.1–0.5.4的开发测试改动。本轮只统一版本、发布材料、编译部署与打包，不改动已验收的游戏逻辑。

## 合并内容

- 入口尊重原生有效控制组，减少与容器交互等上下文按键的冲突；日志明确实际Favorites键位。默认仍跟随游戏Favorites绑定。
- 同一DLL支持GOG 1.6.1179，需对应GOG SKSE64 2.2.6与1179 Address Library。
- 默认使用装备、法术、能力、龙吼和功能操作后保持轮盘打开；药水与食物仍关闭。已有明确KeepOpen=0配置保留。
- 手柄LT/RT支持左/右手装备及中性使用；分类切换可选择LB/RB或LT/RT，并交换另一组的使用功能。
- 设置中的键盘与手柄控制独立分栏。
- 设置路径固定到游戏exe目录下Data/SKSE/Plugins/FavoriteWheel.ini；暂存验证后原位写入目标文件，刷新并重新读取验证，保存失败记录路径和Windows错误。保留未知配置键。

## 编号规则

当前正式版为0.5.1，下一次版本迭代从0.5.2开始，以实际发布版本为基准。历史文档中的0.5.2–0.5.4是本次发布前的测试编号，不代表已发布版本。旧0.5.1测试材料保存在release-materials/0.5.1-test；旧测试源码包保存在dist/FavoriteWheel-0.5.1-test-source.zip。

## 文件与兼容状态

- dist/FavoriteWheel-0.5.1.zip：正式安装包，仅SKSE运行文件及根readme.txt，后者包含完整GPL和第三方许可信息。
- dist/FavoriteWheel-0.5.1-source.zip：匹配源码、固定依赖及SHA256清单。
- release-materials/0.5.1：简介、BBCode详情页、HTML预览与简短中英文changelog。

升级保留现有INI、语言、主题和对应.skse存档。未上传Nexus或创建GitHub Release。

精确支持1.5.97、1.6.640、1.6.1170、GOG 1.6.1179、1.7.99与1.7.104。1.5.97与1.6.1170已有游戏内测试记录；没有明确运行时证据将本轮总体验收扩展为GOG1179、640、1.7或所有第三方组合认证。GOG可选联动插件也需各自支持该运行时。详情页保留实际兼容边界。

设置写入针对已识别路径与虚拟文件系统风险加固；原位写入不具备断电原子性，也不保证不同模组管理器配置档中的文件优先级。

## 发布验证

- Release构建成功，自动部署至H:/Games/Dev Skyrim/mods/36 - FavoriteWheel。旧DLL备份为build/FavoriteWheel-before-release051.dll。
- 十项现有自动测试全部通过：WheelLogic、ActorRuntime、Settings、Outfit、NameEditor、FaceLightClient、RuntimeLayout、ItemInfo、Inventory、Time；结果保存在build/release051-tests.txt。RuntimeLayout验证实际DLL的0.5.1元数据及六运行时模拟布局。
- 现有8个配置文件SHA256完整保留，基线为build/release051-preserved.json；主INI、语言、主题及MO2 meta未重置。
- 构建与部署DLL SHA256一致：2C1C300F9593AB7B8C546DFC7F189BC988863D2546F9FBDA3D0A8763059E1461。
- BBCode标签与248字符简介校验通过；变更review确认游戏代码仅版本日志与元数据调整。
- 安装包逐文件校验，结构仅9个运行/说明文件；readme保留完整许可全文。源码包包含完整逐文件SHA256清单。

没有绘制变更，本轮不重复离屏绘制验证。没有启动游戏，本轮功能验收以用户此前测试反馈为依据。
