# 0.5.2 正式发布记录

日期：2026-10-10。公开名称：Favorite Wheel - Radial Actions。

用户确认自0.5.1以来的功能和本地化测试正常，并授权准备0.5.2正式发布包。本次保持已测试的游戏逻辑，重新构建DLL并整理发布材料；对照已批准版本迭代，当前正式版为0.5.2。

## 合并内容

- 游戏行为页新增隐藏空分类选项，默认开启，关闭后仍显示全部八种物品分类。
- 换装保留共有装备，先穿新套装主体，再清理旧配件，避免先全部脱光；点击精确已装备套装仍卸下。应用/卸下预设始终关闭轮盘。引擎或其他装备脚本仍可能影响过渡，不承诺原子换装。
- 可选手柄左杆在减速/正常速度自由走跑，右杆选择物品及操作对话窗口；默认关闭。只有成功Apply才切换输入方案，关闭后右杆回中再恢复镜头。
- 新安装/默认值轮盘位置X72%、Y46%，已有位置不修改。
- 自带英、中、法、巴西葡、俄、日、韩、德共8种语言，系统语言自动匹配、英语回退和手动选择保持。感谢[Ardios](https://next.nexusmods.com/profile/Ardios)贡献法语基础翻译；新增字段由项目维护。
- 安装ZIP不带主配置INI。缺文件时用内置默认值，首次成功Apply创建文件和目录；启动不改写已有配置。补齐Enabled/Font保存、统一缺键默认值，自动部署仅二进制追加缺失翻译键。

## 发布材料

- dist/FavoriteWheel-0.5.2.zip：安装包仅DLL、8语言、4主题、根readme.txt（完整GPL及第三方许可），无主INI。
- dist/FavoriteWheel-0.5.2-source.zip：匹配源码、固定依赖、逐文件SHA256清单。
- release-materials/0.5.2：简短中英changelog、简介、BBCode详情、HTML预览、SVG封面及2560/1280宽PNG、生成脚本。
- 详情页在原Themes & translations区块列出8种语言、自动选择规则及Ardios署名，同时更新空分类、换装、手柄分杆、位置和无主INI安装说明。
- 封面沿用原图标/布局/英文标题，去掉中央Q及相应描述，避免暗示只能使用固定键盘入口。

历史0.5.2测试发布材料保存在release-materials/0.5.2-test；本轮打包前旧同号测试源码包备份到build/release052-history，不覆盖旧0.5.0/0.5.1正式包。主INI、语言、主题和meta.ini保留哈希基线为build/release052-preserved.json，旧DLL备份build/FavoriteWheel-before-release052.dll。

精确支持1.5.97、1.6.640、1.6.1170、GOG1.6.1179、1.7.99与1.7.104。1.5.97和1.6.1170有游戏内测试记录，其余支持但尚未获得明确实机验收记录；本轮总体验收不扩展为所有运行时或第三方组合认证。

此次交付为发布文件与源码同步，不自动上传Nexus或创建GitHub Release。本轮没有启动游戏；功能验收依据用户此前测试。封面使用矢量渲染后目视检查，详情BBCode校验后生成预览。

## 发布验证

- Release构建成功（123.938秒），自动部署至指定36 - FavoriteWheel目录；游戏源码没有行为变更。
- Settings、Inventory、WheelLogic、Time、RuntimeLayout五项测试通过。RuntimeLayout验证实际0.5.2 DLL元数据及六支持运行时模拟布局；日志build/release052-tests.txt。
- 14份已有主INI、语言、主题和meta.ini哈希与部署前完全一致。
- 构建和部署DLL SHA256均为 `2C56ED10B7CF92AAF00969CCB9285109AB1FA52814A1913789D12AE27551B251`。
- 详情BBCode标签平衡、248字符简介校验通过；Translations区块实际HTML截图检查通过。封面SVG不含中央Q，2560×1440与1280×720 PNG已经生成和检查。
- 打包脚本检查安装ZIP所有条目的SHA256和结构，主INI禁止项保留；匹配源码ZIP含SOURCE-SHA256SUMS.txt。完整GPL与第三方许可保留在安装包唯一根readme.txt中。
