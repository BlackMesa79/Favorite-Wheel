# 0.6.0 发布记录

日期：2026-10-12。公开名称：Favorite Wheel - Radial Actions。上一正式版为 0.5.2；历史 0.5.3/0.5.4 测试材料已并入旧正式版。用户认可当前主题与修复，授权整理为 0.6.0。

## 合并范围

此次包括自 0.5.2 以来的三层主题支持、Skyrim 材质/Nordic 边框与十边形主题、Classic Gold 重做与旧主题退役、标记与详情卡留白、折扇网格与字体预热优化、火把与暂停装备修复、原版涂毒与独立毒药分类、50–100% 减速保护、未暂停导航菜单门控、Important 标记保护及空 INI Unicode 保存修复。完整简短列表见 [英文 changelog](../release-materials/0.6.0/Nexus-changelog-en.txt) 和 [中文 changelog](../release-materials/0.6.0/changelog-zh-CN.txt)。

STB 热键仍是独立系统，My Preciouses 等虚拟手指槽未新增适配。更泛化的 MO2 空文件报告仍需原反馈者日志；不将已复现的 Unicode 修复认定为所有空文件问题的根因。

## 发布文件与页面

- dist/FavoriteWheel-0.6.0.zip：DLL、8 个语言文件、3 个主题、1 个原创材质 PNG、根 readme.txt；不含玩家主 FavoriteWheel.ini。
- dist/FavoriteWheel-0.6.0-source.zip：当前匹配源码、固定依赖、文档及 SOURCE-SHA256SUMS.txt。
- release-materials/0.6.0：英文 Nexus BBCode 详情页、248 字符简介、HTML 预览、简短中英 changelog、沿用的原创封面预览。
- [主题制作指南](THEME_AUTHORING_0.6.0.md)：三层接口、完整 INI、参数范围、发布与升级说明。README、资源文档及安装包 readme 已同步。

旧安装需先备份本地主题编辑，再从活动 Themes 目录移除 frost.ini、engraved.ini、minimal.ini 和开发版 ellipse.ini，安装新 classic.ini。复制 ZIP 到旧目录不会删除退役文件。主 INI 与其他自定义主题保留；不可用的主题 ID 回退 Classic Gold。源码自动部署将准确四个退役文件归档到 FavoriteWheel/ThemeBackups/0.6.0，仅迁移未自定义的原版 Classic；安装 ZIP 不执行迁移脚本。

## 验证记录

Release 构建成功；项目、启动日志与 SKSE 导出元数据均为 0.6.0。14 项测试通过：Settings、WheelLogic、Time、RuntimeLayout、Inventory、Outfit、NameEditor、ItemInfo、FaceLightClient、ActorRuntime、SkinTexture、LayoutSkin、SectorMesh 和隔离 SettingsVfs。VFS 覆盖新建、空 mod/overwrite 文件、Unicode 字体路径、独立进程重读与只读失败。

三个内置主题及文档完整示例通过生产绘制路径 WARP 检查。详情页 BBCode 标签、简介长度、版本、图片加载与桌面/手机横向溢出检查通过；新主题制作段落已目视检查。此前用户确认类型切换、火把、暂停装备、毒药及新分类通过；当前发布准备未另行启动游戏，不扩展为所有运行时或第三方组合实测。

首次自动部署遇到正在运行的游戏占用 DLL。用户退出后已同步至 H:/Games/Dev Skyrim/mods/36 - FavoriteWheel。主 INI 在 23:56:55、部署前由运行中的游戏更新；保留该配置，其余 13 个起始基线文件哈希不变。退出后重建 14 文件基线，后续部署按此检查。旧 DLL 备份 build/FavoriteWheel-before-release060.dll，日志与基线为 build/release060-*。

构建和安装 DLL SHA256：D6F207BB5F30F0CF2A924E4041F2DA5484ACE535B9F47537D716BE4E85EBEEAD。

package.ps1 校验安装 ZIP 结构与逐文件 SHA256，禁止主 INI，保留完整 GPL 和第三方许可；源码 ZIP 附逐文件清单。这是本地发布文件准备，不代表已经上传 Nexus 或创建 GitHub Release。