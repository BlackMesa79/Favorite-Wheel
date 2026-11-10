# I4 规则离线对照

构建：`xmake build -j 2 I4RuleAudit`（release 模式）。

用法：`xmake run I4RuleAudit <mods目录> <profile目录> <I4日志> <输出目录> [Skyrim.ccc]`。
本项目本机输出目录使用 `build/i4-phase2`。

读取 modlist.txt 的启用模组（优先级从高到低，首个同名松散 JSON 生效），读取 plugins.txt 的勾选插件；加入基础五个主文件和可选 Skyrim.ccc 中隐式启用的 Creation Club 插件。存在 loadorder.txt 时按其顺序加载，否则使用上述清单顺序。不会将未勾选的普通插件自动启用。Skyrim.ccc 应取自此 MO2 实例实际使用的游戏目录；缺省时不猜测 CC 插件。

按顺序调用生产版 Load，逐文件统计后再次整批加载并测量 Resolve。输出 audit.json（来源、计数、警告、性能）和 comparison.md（逐文件对照）。退出码：0 为计数一致，1 为差异，2 为输入/输出错误。

工具不读取 Interface、SWF、贴图或 BSA。iconSource 存在性明确假定为真，不能据此宣称游戏资源验证通过。对已启用插件提供零前缀占位，仅用于规则计数；不是实际运行时 ID。缺失插件按 I4 语义警告并返回零 ID，不因此删除整条规则。

性能输入是固定混合类型的合成条目，规则来自真实 JSON，不代表游戏背包采集。原配置、模组和日志均只读。
