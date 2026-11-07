# SKSE Menu 分类页面与 SGT 测试物品核查

日期：2026-11-06。

## 配置菜单改动

- FavoriteWheel 左侧新增四个入口：外观、游戏行为、键盘、手柄；移除单一 Settings 总页。入口名称使用注册时的当前语言，应用语言后页面内容会更新，左侧入口名称需重启游戏后更新。
- 各页使用双列表格：设置标签在左、控件在右。说明自动换行，控件宽度随面板调整并限制最大宽度；减少默认满宽控件带来的拥挤。沿用框架自己的主题和字体，不修改其全局样式。
- 语言/主题显示可读名称；应用按钮改为“应用并保存”，不再错误暗示会返回轮盘。非减速模式禁用减速比例控件。
- 四页共享一个草稿，切页不丢失修改。应用/取消作用于全部页面；恢复默认只作用于当前页，保留其他页、Enabled 与 Font。
- 新增未应用修改提示；保留已有外部编辑冲突保护、轮盘打开时禁止编辑、原 INI 写入及校验路径。修正应用成功当帧沿用旧配置快照导致的冲突提示。
- 保留可选依赖：逐项解析框架 C 导出，不使用轮盘自带 ImGui 操作框架上下文。只读检查本地 3.18 DLL，25 项所需导出齐全。

## Modex 提取与 SGT 乐器

只读检查本地 `36 - Skyrim's Got Talent - Improve As a Bard/SkyrimsGotTalent-Bards.esp`：

| FormID | 类型 | EditorID | 用途 |
| --- | --- | --- | --- |
| 000DABA7 | MISC | Flute | 原版笛子，挂载 _Talent_Givespell |
| 000DABA9 | MISC | Drum | 原版鼓，挂载 _Talent_Givespell |
| 000DABAB | MISC | Lute | 原版鲁特琴，挂载 _Talent_Givespell |
| 00105177 | MISC | Flute01 | 原版笛子记录，挂载相同脚本 |
| 00105109 | MISC | DancersFlute | 任务笛子，挂载相同脚本；不建议作为常规测试物品 |

另外存在 3 个模组新增 Dummy MISC 和 13 个 ALCH 记录。ALCH 包括 Beating of the Drum、Charming Melody、Inspiring Tune 等，均使用药剂模型，并非正常可见乐器。未取得用户提取出的物品名称/FormID，不能逐件认定其“食物”具体对应哪条记录。

本地 Modex `Interface/Modex/user/settings.json` 中 `Use Base Plugin=true`。英文/中文自带说明都明确：开启时按最初定义记录的插件分组，关闭时按最后修改记录的插件分组，改变该选项需要重启。因此上述原版乐器应在 Skyrim.esm 下查找；仅选择 SGT 时看不到它们，与当前现象吻合。本轮未修改 Modex 配置或 SGT 文件。

测试可在 Modex 选择 Skyrim.esm 或全部物品搜索鼓、笛子、鲁特琴；也可手动在游戏控制台执行：

```text
player.additem 000DABA7 1
player.additem 000DABA9 1
player.additem 000DABAB 1
```

轮盘设为“全部背包”，从“其他”分类分别使用。原有 SGT 兼容继续走关闭轮盘后调用原生 EquipObject 的路径；本轮没有增加虚构乐器、改物品分类或伪造脚本事件。实际演奏是否触发仍待游戏内验证。

## 验证与部署

- Release 构建成功，按既有 after_build 自动部署至 `H:/Games/Dev Skyrim/mods/36 - FavoriteWheel`。
- SettingsTests、SettingsPersistenceTests 通过；语言迁移脚本通过。
- 新增 FrameworkSettingsPreview：使用生产页面代码，通过独立 ImGui 适配器和 D3D11 WARP 实际渲染，不加载游戏或框架 DLL。中文、英文、俄文四页均完成渲染，检查有限顶点与无页面滚动溢出；当前页恢复默认与保留其他字段断言通过。
- 已人工查看中文四页合成预览和俄文长文本工具栏，标签/控件对齐、按钮未截断、说明正常换行。框架实际主题、字体及运行时交互仍待用户游戏内确认；没有接管键鼠。
- 中文预览：`build/framework-preview-25112/overview.jpg`（1280×920，约 130 KB）。俄文工具栏：`build/framework-preview-15184/toolbar.jpg`。
- DLL 构建/部署 SHA256 一致：`75323ADBC9D1E07E63FD94A7193D3B9570628A4DB401D682ADF72355371FC41A`。
- 部署前核对 26 个既有配置/资源文件，部署后仅 8 个语言文件发生预期变化（追加三个新键）；主 INI 和其他 17 个文件哈希未变。
- 主 INI SHA256：`E4853932098343A24EF3204B89A2435CE2E75F6334AC7C499545F2C9F1E1A3A0`。
- 部署前 DLL 备份：`build/FavoriteWheel-before-framework-pages-20261106.dll`。校验记录：`build/framework-pages-verification.json`；插件物品记录：`build/sgt-item-records.json`。
