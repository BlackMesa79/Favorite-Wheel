# T-2 实现报告（Coder-C）

- Task ID：20261028-takeover-review
- 分支：`wf/20261028-takeover-review-C`
- 状态：DONE（实现完成；AC-13 通过，AC-12 前置逻辑用例通过，PowerShell 7 原生复测存在下述环境限制）
- 本次按委派消息覆盖计划中的部署步骤：所有构建禁用部署，最终仍保持 `deploy_dir = ""`。未启动游戏、未接管键鼠、未打包、未发布、未推送或合并。

## 改动摘要

| 文件:行号 | 改动 |
|---|---|
| `xmake.lua:18` | 新增 `favoritewheel.version` 规则，从目标继承的 `set_version` 派生字符串及 major/minor/patch 四个约定宏。 |
| `xmake.lua:37`、`xmake.lua:256` | `FavoriteWheel` 和 `RuntimeLayoutTests` 使用同一版本规则。版本唯一赋值仍在 `xmake.lua:2`，最终为 0.9.0；未修改部署默认值。 |
| `src/main.cpp:35`、`src/main.cpp:44` | 两条插件版本日志使用 `FAVORITEWHEEL_VERSION_STRING`。 |
| `tests/RuntimeLayoutTests.cpp:121` | 导出版本期望值使用整数版本宏，保留原测试行为。 |
| `scripts/package.ps1:2` | 新增 `AllowStaleDll` 开关。 |
| `scripts/package.ps1:11` | 在创建目录、复制文件之前校验 DLL FileVersion 前三段，版本不符始终失败；递归检查 src/include 文件及 xmake.lua 的 UTC 修改时间；只有新鲜度检查允许跳过。失败提示包含原因与重建建议。 |
| `scripts/package.ps1:27` | SHA256 计算和输出前移到校验成功后、打包前，后续校验继续复用该哈希。 |

## 构建前禁用部署的证据

首先执行 `xmake f -m release -y --deploy_dir=`。该命令在下载既有 MinHook 依赖时失败，未进入构建。随后使用 `--require=n -vD` 定位到 CommonLib 首次配置下载的 GitHub 连接失败。

从 `https://codeload.github.com/TsudaKageyu/minhook/tar.gz/refs/tags/v1.3.4` 下载相同依赖到 `build/takeover-review/minhook.tar.gz`，校验 SHA256 与 `extern/CommonLibVR/xmake.lua` 既有要求完全相同：

```text
1AEBEAE4CA898330C507860ACC2FCA2EB335FE446A3A2B8444C3BF8B2660A14E
```

解压至本 worktree 的 `build/.gens/commonlibsse-ng/windows/x64/release/minhook-v1.3.4`，然后 `xmake f -m release -y --deploy_dir= --require=n` 成功。未修改 extern 内容或引入新项目依赖。

本机 xmake 不支持 `xmake f --show`，采用直接读取 `.xmake/windows/x64/xmake.conf` 的方式。每次构建前分别保存证据到 `build/takeover-review/deploy-before-999.txt`、`deploy-before-tests-999.txt`、`deploy-before-090.txt`、`deploy-before-tests-090.txt`。内容均为：

```text
deploy_dir = "",
mode = "release",
```

最终配置再次检查一致。`xmake.lua` 的 `deploy_dir` 默认值仍为 `H:/Games/Dev Skyrim/mods/36 - FavoriteWheel`，本次没有访问或部署到该目录。

## AC-13：满足

1. 临时将唯一 `set_version` 改为 9.9.9，依次执行 `xmake build -j 2 FavoriteWheel`、`xmake build -j 2 RuntimeLayoutTests`，均退出 0。
2. 执行 `build/takeover-review/verify-version.ps1 -Version 9.9.9`：读取 DLL FileVersion，以 ASCII 字节检索验证两条日志字符串，记录 SHA256，并执行 `build/windows/x64/release/RuntimeLayoutTests.exe`。测试临时文件通过 TEMP/TMP 限定在本 worktree 的 `build/takeover-review/temp`。
3. 恢复 `set_version("0.9.0")`，依次重新构建同两个目标，均退出 0；执行相同验证脚本，参数为 0.9.0。
4. `rg -n '9\.9\.9' xmake.lua` 无匹配；最终 diff 中 `set_version` 无变化。测试读取真实 DLL 的 `SKSEPlugin_Version` 并与宏生成的期望值比较，两轮均通过。

实际输出摘要：

```text
9.9.9 FavoriteWheel: build ok, spent 429.75s
9.9.9 RuntimeLayoutTests: build ok, spent 12.719s
DLL FileVersion=9.9.9.0
DLL contains: FavoriteWheel 9.9.9 loaded;
DLL contains: FavoriteWheel 9.9.9 unsupported runtime
DLL SHA256: B532560FC5497AD9573901ADC026653AA6FC771E9468D59DBEBDC5B6D09E34E8
Runtime layouts, address-library formats 1/2/5 and DLL metadata passed
RuntimeLayoutTests exit=0

0.9.0 FavoriteWheel: build ok, spent 31.391s
0.9.0 RuntimeLayoutTests: build ok, spent 10.672s
DLL FileVersion=0.9.0.0
DLL contains: FavoriteWheel 0.9.0 loaded;
DLL contains: FavoriteWheel 0.9.0 unsupported runtime
DLL SHA256: 27D9E1E94DD557C7CDBCBF7B1F4EB12117810092D2F444D775CDFC20DE811A87
Runtime layouts, address-library formats 1/2/5 and DLL metadata passed
RuntimeLayoutTests exit=0
```

完整日志位于 `build/takeover-review/build-999.log`、`build-tests-999.log`、`verify-999.log`、`build-090.log`、`build-tests-090.log`、`verify-090.log`。两次测试构建出现既有 CommonLib `DECLARE_SPECIALIZATION` 宏重定义警告（C4005），未修改第三方代码。

## AC-12：前置逻辑用例满足；PowerShell 7 原生复测未执行

环境没有在 PATH 或已检查位置发现 `pwsh.exe`。生产脚本保留 `#Requires -Version 7.0`；未安装未获授权的新运行时。以下明确是 Windows PowerShell 5.1 下的前置逻辑验证，不等同于原脚本在 PowerShell 7 下的完整运行。

执行：

```powershell
./build/takeover-review/verify-package.ps1 -PowerShell (Get-Command powershell).Source -LegacyPreflight
```

测试脚本把当前 package.ps1、xmake.lua 和本 worktree 最终构建 DLL 复制到 `build/takeover-review/package-fixture/`；src/include 使用嵌套测试文件。仅对临时脚本副本做两处测试改造：移除 PowerShell 7 版本门槛；在 `$dist = Join-Path ...` 前插入成功标记并 `exit 0`。版本、新鲜度、哈希逻辑与生产脚本一致。未执行后续打包步骤。

正常情况将输入文件时间设为两小时前、DLL 设为一小时前；分别把 xmake/src/include 文件时间设为当前，验证递归覆盖；把 DLL 时间设为一天前，验证旧时间戳。旧版本通过对 DLL 副本中的 UTF-16 版本资源字符串做等长替换，伪造成 0.8.0.0，并用 `VersionInfo.FileVersion` 确认。正式构建 DLL 未修改。

实际输出（`build/takeover-review/package-results.txt`）：

```text
fresh exit=0 expectedReason=PREFLIGHT_PASSED
stale-xmake exit=1 expectedReason=DLL is stale
stale-src exit=1 expectedReason=DLL is stale
stale-include exit=1 expectedReason=DLL is stale
old-timestamp exit=1 expectedReason=DLL is stale
allow-stale exit=0 expectedReason=PREFLIGHT_PASSED
Forged DLL FileVersion=0.8.0.0
old-version exit=1 expectedReason=does not match project version '0.9.0'
old-version-allow-stale exit=1 expectedReason=does not match project version '0.9.0'
PASS: all package preflight cases; no dist created
```

正常分支输出：

```text
DLL SHA256: 27D9E1E94DD557C7CDBCBF7B1F4EB12117810092D2F444D775CDFC20DE811A87
PREFLIGHT_PASSED; packaging intentionally stopped
```

失败分支实际原因分别包含 `DLL FileVersion '0.8.0.0' does not match project version '0.9.0'` 和 `DLL is stale: '<fixture>/xmake.lua' is newer than ...`。各用例完整输出保留在 `build/takeover-review/package-*.log`。所有拒绝分支均未出现成功标记；临时副本及本 worktree 的 `dist` 均不存在。

若验收环境提供 PowerShell 7，可直接重新运行 `verify-package.ps1 -PowerShell <pwsh.exe绝对路径>`（不加 `-LegacyPreflight`），保持运行时门槛并复测相同用例。

## 范围与其他检查

- 代码仅修改允许的四个文件；额外受版本管理的文件只有本报告，系委派消息明确要求。
- 临时测试脚本、下载缓存、日志、构建产物和配置均位于忽略目录 `build/`、`.xmake/`，不进入提交。
- `git diff --check` 通过；PowerShell 解析器报告 package.ps1 语法错误数为 0。
- `git -C extern/CommonLibVR status --short` 无输出；未修改 extern 联接内容。
- 按此次明确委派进行本地提交，提交信息为 `T-2: Validate packaged DLL freshness and unify version definitions`。未推送、未合并。
- 未执行其他组的测试目标或游戏内验证；合并后的最终部署与全量回归由编排者完成。
