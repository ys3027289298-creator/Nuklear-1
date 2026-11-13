# 属性面板表格化改造 — 验收报告

## 改动位置（分散的小改）

库改动（拆分源码与单头文件各一份；`python3 src/build.py ...` 重新打包的
结果与 `nuklear.h` 逐字节一致）：

- `src/nuklear_list_view.c` 与 `nuklear.h`：新增
  `nk_list_view_begin_heights()`，在原 `nk_list_view_begin()` 之外接受
  每行像素高度数组；原接口签名与行为不变，内部直接委托新接口。
- `src/nuklear.h` 与 `nuklear.h`：新接口声明。

演示入口（复用仓库自带 GLFW + OpenGL2 后端，无新依赖）：

- `demo/property_table/property_table.c`：1000 行属性数据的表格化属性面板。
- `demo/property_table/Makefile`：与其它 demo 同构的构建入口。

## 表格能力对照

| 要求 | 实现 | 证据 |
| --- | --- | --- |
| 列宽固定 | Value=140px、Unit=80px 静态列，实测恒为 140/80 | `table_metrics.json` 的 `body_column_widths` |
| 列宽自适应 | Name 为 variable（下限 150px）、Type 为 dynamic，平分剩余宽度 | 实测 334/330，差值来自引擎亚像素 FRAC 分配 |
| 表头 | 独立表头行，深色背景加分隔线，与表体共用同一列模板 | `video/01_table_top.png` |
| 对齐 | Name 左对齐、Value/Unit 居中、Type 右对齐 | 全部截图与录屏 |
| 选中行 | 点击整行选中，初始选中第 7 行并蓝色高亮（`selected=7`） | `video/01_table_top.png` |
| 行高自适应 | 普通行 26px，每 137 行出现 52px 高行，高度经数组传入 | `video/02_table_tall_row.png` 中 property_137 |
| 滚动容器范围正确 | 虚拟列表内容高 30182px、视口 574px，滚动钳制在 29638 | `table_scroll.json` |

## 验收数据

### 布局尺寸（`table_metrics.json`，8/8 检查通过）

- 窗口 1000×720；面板内容区 918×618；表头高 30px；普通行高 26px。
- 内容高度实测 30182px，与按行高数组累加的期望值完全相等。
- 每帧只构建可见行：19–20 行 / 共 1000 行。

### 滚动范围（`table_scroll.json`）

- 自动滚轮扫到表底：末帧 `scroll_y=29638`，首可见行 981，可见 19 行。
- 钳制值与「内容高 − 视口高」相差一个容器顶部内边距（30px），
  `reached_bottom=1`，到达后无法继续下滚。

### 千行单帧耗时

- 单轮（118 帧）表格逻辑最大约 **0.08–0.32 ms**，平均约 **0.04–0.05 ms**。
- 5 轮独立采样（`table_passes.csv`、`table_passes_summary.json`）的
  最坏单帧 **0.4194 ms**，给出工程上界 **< 0.5 ms/帧**。
- 上界来源：每帧仅构建约 20 个可见行控件，另加一次 1000 项高度数组的
  O(n) 区间定位（1000 次整数比较），控件构建量不随总行数增长。

## 复现方式

```sh
./output/run_validation.sh
```

需要 `DISPLAY`（本机 `:0`，Xwayland + Mesa 软件 GL 可运行），全程无键盘输入，
程序跑完固定帧数自行退出。

## 录屏（分段、带标题、段间停顿）

`video/property_table_demo.mp4`（约 12.6s，30fps）分四段，段尾冻结 1.5s：

1. Layout: columns, header, alignment
2. Selected row and auto row height
3. Scrolling in the scroll container
4. 1000 rows: only visible rows built

静态截图：`video/01_table_top.png`、`video/02_table_tall_row.png`、
`video/03_table_bottom.png`。
