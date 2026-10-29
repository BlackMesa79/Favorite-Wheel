# T-3 实现报告

- task-id：20261028-takeover-review
- 组别：Coder-B
- 状态：DONE（实现及现有测试通过；AC-3、AC-4 专项验收由 Tester 执行）

## 改动摘要

- `include/CombatCycleModel.h:7`：显式引入 `<locale>`；第 50、60 行分别在 Encode、Decode 的流上设置 `std::locale::classic()`，避免全局 locale 影响记录文本。原有版本、大小、成员、快捷键及尾部严格校验保持不变。
- `include/InventoryOrderModel.h:39`：新增 `inline void RemoveDuplicateKeys(State& state)`；每组独立按 `Token(key)` 去重，保留首次出现的键和原有相对顺序，不修改 mode。沿用已有标准库容器，无新增第三方依赖。
- `src/InventoryOrder.cpp:46`：唯一生产调用点为 `Ordering::Load`；全部 FormID 解析和无效项清理完成后调用 `RemoveDuplicateKeys(loaded)`，再持锁发布状态。排序 Decode 仍拒绝原始文本中的重复键。
- SKSE 记录 ID、版本、文本格式、字段和上限均不变。

## 验收自查

- AC-3：实现满足方案；现有 CombatCycleTests 通过。未编写自定义全局 locale 专项测试，该验收交由 Tester。
- AC-4：实现满足每组保留首项、顺序稳定及 mode 不变的契约；现有 InventoryOrderTests 通过。新增函数专项验收交由 Tester。

## 测试与命令

1. 首次 `xmake build CombatCycleTests` 在配置阶段失败：MinHook 下载发生 `curl: (56) Recv failure: Connection was reset`。
2. `xmake f -m release -y --require=n` 及带 `-vD` 的诊断配置仍因 GitHub 连接失败退出；确认下载由 CommonLib 的 `skse_patch_safety` 配置触发。
3. `xmake f -m release -y --require=n --skse_patch_safety=n`：成功。仅为无 CommonLib 依赖的两个头文件测试临时关闭该选项；测试后已将本 worktree `.xmake/windows/x64/xmake.conf` 中该值恢复为 `true`。`deploy_dir` 保持 `H:/Games/Dev Skyrim/mods/36 - FavoriteWheel`，没有执行部署。
4. `xmake build CombatCycleTests`：成功；`build/windows/x64/release/CombatCycleTests.exe`：通过，退出码 0。
5. `xmake build InventoryOrderTests`：成功；`build/windows/x64/release/InventoryOrderTests.exe`：通过，退出码 0。
6. 检索 tests/ 和 include/ 的引用链，仅上述两个测试目标引用这两个模型头文件，没有其他相关测试目标。
7. `git diff --check`：通过。

构建、运行及配置诊断日志保存在 `build/takeover-review/code-B-*.log`。未构建 FavoriteWheel，未编译游戏侧 Load 集成目标，未启动游戏或接管键鼠；不声称完成游戏内验证。

## 范围与注意事项

- 源码仅修改获准的三个文件；未修改 tests/，未编写 AC 测试。
- 额外新增本报告，为用户明确要求。生成的测试产物、日志和本地 xmake 配置均未纳入提交。
- 未手工修改 extern/；xmake 配置输出包含 CommonLib 资源模板生成提示，随后 `git -C extern/CommonLibVR status --short` 为空，未发现受跟踪内容变更。
- 恢复配置后，未来再次配置可能仍需网络获取 MinHook；本次两个纯逻辑测试的成功结果不依赖该库。
- 本分支仅提交本次三个源码文件及报告，不推送、不合并。
