# 0.6.1 发布记录

日期：2026-10-12。用户反馈原报告者已确认配置正常保存，授权将测试修复纳入正式版。原 0.6.0 归档保留，版本升为 0.6.1。

## 修复范围

合入直接读取/序列化 INI、逐键完整性检查、写入与重新打开后的字节校验，以及空/BOM-only 文件诊断。不再依赖 Windows INI profile API 的缓存和默认值验证。保留注释、自定义键、Unicode 内容及现有 MO2 映射/Vortex 式硬链接。启动只读取，Apply 才保存。具体证据、测试和环境边界见 [调查记录](CONFIG_SAVE_0.6.0_FOLLOWUP.md)。

## 发布文件

- dist/FavoriteWheel-0.6.1.zip：完整安装包，不包含玩家主 INI。
- dist/FavoriteWheel-0.6.1-source.zip：对应源码、固定依赖与逐文件 SHA256 清单。
- release-materials/0.6.1：中英简短 changelog、英文详情页与 HTML 预览。主题指南沿用 0.6.0 的接口文档。

从 0.6.0 正常升级；停用独立的配置保存测试补丁，避免其旧版本 DLL 覆盖正式版。保留主 INI 和自定义主题。0.6.0 的安装包与源码包保留。

## 验证

原反馈者确认配置保存成功，未将其扩大为所有管理器/运行时实测，亦未收到明确的退出重启序列反馈。15 项本地套件通过：Settings、SettingsPersistence、SettingsVfs、WheelLogic、Time、RuntimeLayout、Inventory、Outfit、NameEditor、ItemInfo、FaceLightClient、ActorRuntime、SkinTexture、LayoutSkin、SectorMesh。配置测试覆盖故障注入、独立进程、多编码、硬链接和八种隔离 usvfs 场景；RuntimeLayoutTests 确认 0.6.1 导出版本。

Release 构建与本地 MO2 安装目录同步成功，部署前后的 18 份玩家配置/语言/主题/材质哈希不变。DLL SHA256：`10DB8E4BB4ABF04EB887C05DC612B2DE1495BA744C41F6273980D496BAAF1BB5`。构建日志与逐项测试日志位于 build/release061-*。包/部署核验记录见 release-materials/0.6.1/verification.txt。

本次为本地正式发布材料准备与安装目录同步，不代表已上传 Nexus、提交/推送 Git 或创建 GitHub Release。
