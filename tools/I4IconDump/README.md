# I4IconDump 本地对照工具

只生成离线验证产物，不启动游戏，不调用 SKSE，不修改安装的 SWF。第三方素材与衍生产物留在 `build/i4-phase1/`，不得打包或提交。第三轮产物统一放 `build/i4-phase1/round3/`。

## 同坐标网格验收（第三轮）

```powershell
xmake build -j 2 I4IconDump
# <库名> 分别为 boobies、handy。第二轮 CSV 提供完整源文件/标签枚举及旧指标。
python tools/I4IconDump/grid_reference.py build/i4-phase1/round2/<库名> build/i4-phase1/round3/<库名> --java build/tools/jre/zulu17.70.113-ca-jre17.0.21.0.3-win_x64/bin/java.exe --ffdec build/tools/ffdec/ffdec.jar
build/windows/x64/release/I4IconDump.exe --grid-batch build/i4-phase1/round3/<库名>/grid-manifest.txt build/i4-phase1/round3/<库名>/renders
python tools/I4IconDump/grid_compare.py build/i4-phase1/round3/<库名> --previous build/i4-phase1/round2/<库名>
python tools/I4IconDump/test_compare.py
python tools/I4IconDump/test_grid.py
```

`reference.prepare(source, frame_label)` 独立读取 SWF 声明边界与放置矩阵，排除空形状倒置/零面积 RECT，递归取 sprite 第一帧。它保留定义及目标帧之前的主时间轴放置/移除操作，合并为标签 `grid` 的单帧本地副本；重复标签取首次出现，与生产解析器一致；形状、颜色变换与放置矩阵不变。每帧声明几何边界外扩12像素，并按半次幂分组选择 zoom，使内容最长边约328–464像素。参照生成不读取 SwfRaster 的像素或几何输出。FFDec 按 zoom 分组串行批量导出，禁止并行导出，512 MB堆、两个处理器、headless；Java 用户目录及临时目录限定在本轮 build 子目录。

FFDec 26.4.0 的主帧 PNG 尺寸为 `ceil((Xmax-Xmin)*zoom/20)` × `ceil((Ymax-Ymin)*zoom/20)`；变换为 `((x-Xmin)*zoom/20, (y-Ymin)*zoom/20)`，没有额外边距或半像素偏移。外扩12像素是脚本的舞台策略，不是 FFDec 内部行为。显式关闭 MSAA，使用 Java2D 默认抗锯齿，PNG 保留透明背景。逐帧 `grid-references.json` 保存 RECT、zoom、尺寸、源标签、副本和参照路径，诊断清单由同一数据生成。

`include/SwfRasterDiagnostics.h` 仅在 `WHEEL_SWF_DIAGNOSTICS` 下声明 `RenderViewport`。其视口原点以 twips 表示，缩放以像素/twip 表示，画布宽高显式指定。它复用生产路径的显示列表、几何、填充、描边及着色循环，只替换最终视口；公共 `include/SwfRaster.h` 签名不变。第三轮同时修复共用描边路径：完整实例变换后宽度不足1像素（含零宽发丝线）的描边，在设备坐标中以1像素绘制。`--grid-batch` 输出 Original、Mask 和五类负对照，失败写入 `grid-renders.csv`，不会伪造透明成功图。

主度量不裁切、不缩放、不模糊、不按包围盒对齐：

1. 两张 PNG 必须与元数据尺寸相同，直接读取连续 alpha，软 IoU＝Σmin / Σmax。
2. **验收使用 `soft_iou_unshifted`（零位移分数）**。另外横纵搜索 -1、0、1 px 得到 `soft_iou` 与 `shift_x/y`，只用于观察网格是否错位，不参与验收。`binary128_iou` 仅辅助观察。
3. 单图阈值0.98、非空参照库级通过率98%（常量见 `grid_compare.py` 的 `METRIC` / `THRESHOLD`）。缺参照、实现失败和空/非空分歧独立记录，不视作通过；双方空帧记 `empty_ok`，不进入非空分母。
4. `iou` 与 `round2_soft_iou` 从第二轮 CSV 对应的 JSON 逐标签复制，明确是旧256自适应输出的历史指标，不参与验收。

负对照全部在矢量渲染阶段注入：随机另一非空标签用目标的同一视口渲染；水平镜像和0.9缩放以正常帧几何中心进行；平移为最终视口横移4像素；删除路径从末条起每五条跳过一条，共 ceil(N/5) 条，保持正常视口。随机种子为 `0x4924`，实际错配映射写入元数据。

`self_difference = 1 - soft_iou(正常输出alpha, 缺陷输出alpha)`，使用零位移原网格。逐组同时报告所有样本的误收率和 `self_difference > 0.02` 的可见缺陷误收率，后者必须≤1%。对称、近对称、相同图案及删去极小面积的样本完整保留，`grid-accepted-negatives.csv/json` 列出所有误收及可见差异。负对照与正例使用同一度量和阈值。第三轮曾用 ±1 px 与 0.97，放过了部分可见镜像和4 px平移；收紧为零位移与0.98后重新判定。2%可见门槛并不保证每个可见缺陷都会被拒绝；校准失败必须如实报告，不能改度量或挑选样本。`ac4_accepted` 要求正例、参照完整性和校准同时通过。

`grid-comparisons/` 覆盖全部非空参照，低分优先，左 FFDec、右 SwfRaster；只有拼图显示使用缩略图，所有分数在原生网格计算。`test_grid.py --fixtures <本轮目录>` 可生成已知坐标矩形，FFDec 导出后以 `--measure` 核对尺寸及像素边缘，误差必须≤0.5 px。

## 生产输出与历史工具

`I4IconDump <SWF目录> <输出目录> [--negative-controls]` 保留原用法：枚举所有主时间轴标签，保持换行等原文，输出256×256 Original/Mask及耗时。生产接口仍然自适应几何边界、90%居中。PNG 编码将RGBA转换为BGRA后交给WIC。

`reference.py` 不传 `frame_label` 时保持第二轮全文件扩展舞台导出；`compare.py` 保留第二轮包围盒归一化比较，用于历史复现。该路径会消除全局平移/缩放，不再作为AC-4主度量。第三轮执行证据、人工复核和最终验收状态见 `build/i4-phase1/round3/REPORT.md`。
