# 主配置初始化与发布包检查（0.5.2开发版）

2026-10-10：用户确认此前功能和本地化测试正常，要求以后安装包不再携带主配置文件，避免升级覆盖玩家设置，并参考面部光照此前的问题审查初始化代码。当前开发版仍为0.5.2，最近正式版为0.5.1；本次不升版、不改写历史正式包。

## 安装与初始化规则

- 安装ZIP只包含DLL、语言/主题资源和含完整许可的readme.txt，不包含 `SKSE/Plugins/FavoriteWheel.ini`。语言和主题本身采用INI格式，仍需随包分发。
- `scripts/package.ps1`取消主INI复制，并在生成的ZIP上检查禁止项；自动部署同样不再复制源码中的示例主INI，已有玩家文件保持不变。
- 没有主INI时使用内置 `Settings{}` 默认值。启动不创建文件、目录，也不补写旧INI缺失的字段；首次成功点击Apply才创建主INI和父目录。
- 默认路径从 `SkyrimSE.exe` 所在目录解析到 `Data/SKSE/Plugins/FavoriteWheel.ini`，不依赖启动器工作目录或DLL物理安装目录；使用Unicode文件接口。
- 现有配置只读加载。显式设置、旧Chinese字段、未知键和注释不会因为启动而被覆盖。新增字段只在内存中回退默认值。
- Apply继续通过系统临时目录准备、字段验证、目标原位写入、字节验证和重开验证；只有成功才更新已应用快照。只读或目录创建失败有明确诊断，可取消预览。管理器可能把新建INI放到Overwrite或其可写覆盖目录。
- 仓库及源码ZIP中的根目录 `FavoriteWheel.ini` 是手动配置参考，不是安装包内容。

## 审查及修复

对照面部光照 `src/Settings.cpp` 和交接记录：此前问题涉及相对工作目录路径、Unicode路径和缺少父目录。轮盘已有绝对游戏目录路径、W接口和保存前创建目录，不需要再次引入启动生成文件的逻辑。

本次发现SaveSettings没有写入 `General/Enabled` 和 `Display/Font`。已有INI保留这两项时通常不会暴露问题，但目标文件不存在且内存中有非默认值时，完整字段校验会失败。本次补齐这两个字段，支持Unicode字体路径；新建文件会保存完整设置。

部署核对还发现语言文件即使没有缺键也被无条件重写，导致六份新语言资源换行格式变化。脚本现只在缺键时以二进制追加，不重写原内容；本次变化已对照部署前SHA256和源码原件验证为仅换行转换，并恢复原始字节后重新部署。

ReadSettings的缺键默认值统一引用刚构造的Settings字段，消除独立硬编码默认值，保留旧Chinese迁移。LoadSettings先检查文件属性，缺文件直接回退Settings；属性查询异常和同名目录使用load-unavailable诊断，不再把目录误报为已加载。

## 验证范围

SettingsTests新增：缺文件/缺中文父目录时无启动写入、默认值完整一致、首次Apply创建目录并保存Enabled和Unicode Font、删除INI后回退默认且不重建、已有空文件不改写、旧部分配置启动字节不变、只读配置加载字节不变、父目录被普通文件阻挡时失败且原文件保留。

已有测试继续覆盖取消/默认值/应用快照、保存失败、未知字段、Unicode路径、启动工作目录变化、独立进程重新读取、八种语言和主题。测试未启动Skyrim，也不能替代真实模组管理器虚拟文件系统的实机验证。保存仍是原位写入和尽力回滚，不是断电原子事务。

验证安装包使用 `scripts/package.ps1 -TestPackage -SkipSource`，不覆盖历史正式包；主INI缺席、8语言/4主题和readme内许可均需核对。

## 本地交付

Release构建与自动部署成功，SettingsTests及RuntimeLayoutTests通过；后者包括六支持运行时、地址库格式1/2/5和实际DLL元数据。最终DLL及部署SHA256均为 `DC3225746A9A3723E83ACF888DB2A0EDD5A2722B0520F1CE4FB15EC42574E319`。已有主INI、8语言、4主题和meta.ini共14个文件，最终哈希与部署前完全一致。

`dist/FavoriteWheel-0.5.2-test.zip` 已验证14个文件、734581字节：DLL、8语言、4主题和readme.txt，无主INI，完整GPL和第三方许可保留。没有生成新正式发布包或源码包，没有覆盖历史正式包。旧DLL备份 `build/FavoriteWheel-before-config-init052.dll`；基线 `build/config-init052-preserved.json`，测试日志 `build/config-init052-settings-tests.txt` 和 `build/config-init052-runtime-tests.txt`，最终构建日志 `build/config-init052-final-build.txt`。
