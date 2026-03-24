```{=latex}
\begin{titlepage}
\centering
\vspace*{2.8cm}
{\Huge\bfseries xyzTrick 软件说明书\par}
\vspace{0.9cm}
{\Large 面向 XYZ / CHG / GaussianView 剪贴板与文件桥接的结构查看、转换与轻量插件宿主\par}
\vspace{2.2cm}
\begin{tabular}{rl}
产品名称： & xyzTrick \\
内部组件名： & XYZ Monitor / xyz\_monitor \\
版本： & 2.1.0 \\
文档类型： & 软件说明书 \\
语言： & 中文 \\
\end{tabular}
\vfill
{\large Bane Dysta\par}
\vspace*{1.2cm}
\end{titlepage}

\tableofcontents
\newpage
```

# 概述

## 产品定位

xyzTrick 是面向 Windows 桌面环境的分子结构桥接工具，用于在 XYZ / CHG 文本、GaussianView 剪贴板、结构文件与日志查看器之间建立快速转换通道。程序以系统托盘常驻与全局热键为主工作模式，适合在文献阅读、结果检查、坐标整理、快速可视化与轻量后处理场景中使用。

本程序不承担量子化学计算任务本身，也不生成 Gaussian、ORCA 等程序的正式输入文件。对于 XYZ / CHG 文本或结构文件，程序会将其转换为**供 GaussianView 打开的伪 Gaussian 日志文件**，并在固定延时后自动清理该临时工件。

## 适用场景

xyzTrick 适用于以下场景：

- 从网页、论文、笔记或文本编辑器中复制 XYZ 坐标，并立即在 GaussianView 中查看。
- 将 GaussianView 中已经复制的分子结构重新写回标准 XYZ 文本，用于粘贴到其他程序或文档。
- 直接双击或命令行传入 `.xyz`、`.trj`、`.chg`、`.log`、`.out` 文件，按文件类型自动交给适当的查看流程。
- 在多帧轨迹、优化路径或带注释的 XYZ 文件中保留几何步与部分收敛信息，用于快速浏览。
- 通过插件方式扩展与剪贴板有关的轻量工具链，例如基于剪贴板的结构优化或预处理。

## 核心能力

- 以全局热键触发 **XYZ/CHG -> GaussianView** 与 **GaussianView -> XYZ** 两条主流程。
- 支持标准 XYZ、多帧 XYZ、简化 XYZ 与 CHG（Element X Y Z Charge）文本。
- 支持 UTF-8、UTF-16、UTF-32 与系统 ANSI 文本文件读取，并统一内部换行处理。
- 支持对 `.log` / `.out` 做快速类型识别，并按 Gaussian、ORCA、其他日志分别调用对应查看器。
- 支持通过 `config.ini` 调整热键、路径、内存限制、列映射、日志行为与插件定义。
- 支持通过系统托盘菜单、图形化设置窗口与命令行单文件参数三种方式进入程序功能。

## 产品名称与内部标识

当前发布名称为 **xyzTrick**。同时，程序内部资源、窗口标题、日志文件名、托盘提示与若干代码常量仍使用 **XYZ Monitor** / `xyz_monitor` 作为内部组件名。当前版本中，两套命名在以下位置同时存在：

| 位置 | 当前显示或使用的名称 |
| --- | --- |
| 发布名、安装器、可执行文件目标名 | `xyzTrick` / `xyzTrick.exe` |
| 程序窗口标题、托盘提示、日志文件名、资源说明 | `XYZ Monitor` / `xyz_monitor` |
| 应用日志默认文件名 | `xyz_monitor.log` |

本说明书统一以 **xyzTrick** 作为产品名称；当界面文本、配置项、日志文件名或资源字符串必须保留程序实际显示时，按当前实现写出对应的内部名称。

## 核心对象

| 对象 | 说明 |
| --- | --- |
| 驻留实例 | 无文件参数启动的主进程。负责托盘、热键、设置窗口、插件热键与两条剪贴板主流程。 |
| 文件参数实例 | 通过 `xyzTrick.exe <file>` 启动的单次处理进程。只处理一个文件参数，不注册托盘与热键。 |
| 临时日志文件 | 从 XYZ / CHG 数据生成的伪 Gaussian `.log` 文件，用于交给 GaussianView 打开。 |
| Gaussian 剪贴板文件 | 由 `gaussian_clipboard_path` 指定的外部文件，作为反向转换的数据来源。 |
| 插件项 | `config.ini` 中除 `[main]` 外的配置分区。每个分区对应一个可执行命令与可选热键。 |

# 系统模型与运行方式

## 两种运行模式

xyzTrick 提供两种互补的运行模式。

### 驻留模式

不带文件参数直接启动 `xyzTrick.exe` 时，程序进入驻留模式：

1. 读取与初始化 `config.ini`。
2. 初始化日志输出。
3. 创建隐藏窗口与系统托盘图标。
4. 注册主热键、反向热键与插件热键。
5. 进入消息循环，等待热键、托盘菜单与设置窗口交互。

此模式是日常使用的主入口。

### 文件参数模式

以 `xyzTrick.exe <file>` 形式启动时，程序进入文件参数模式：

1. 仅处理 `argv[1]` 指定的单个路径。
2. 不创建托盘图标，不注册全局热键。
3. 按扩展名与内容类型执行一次性处理后退出。

当前版本不提供多文件批处理参数，也不解析其他命令行选项。

## 典型发布目录布局

典型发布目录如下：

```text
xyzTrick/
  xyzTrick.exe
  config.ini
  plugins/
    clipxtb.exe
  logs/
    xyz_monitor.log
  temp/
```

说明如下：

- `xyzTrick.exe` 为主程序。
- `config.ini` 与主程序位于同一目录；程序固定从该位置读取配置。
- `plugins/` 保存插件可执行文件；是否存在具体插件取决于发布包内容。
- `logs/` 与 `temp/` 可由程序在运行时自动创建。
- `logs/xyz_monitor.log` 为默认应用日志文件。
- `temp/` 为默认临时日志目录；若 `temp_dir` 另行指定，则按配置路径写入。

## 主流程概览

### XYZ / CHG / 文件 -> GaussianView

标准处理链路如下：

1. 从剪贴板文本或指定文件读取结构文本。
2. 按配置执行字符数上限检查。
3. 按 XYZ、简化 XYZ 或 CHG 规则解析结构。
4. 生成伪 Gaussian 日志文本。
5. 写入临时 `.log` 文件。
6. 调用 GaussianView 打开该文件。
7. 按 `wait_seconds` 固定延时删除临时文件。

### GaussianView -> XYZ

标准处理链路如下：

1. 从 `gaussian_clipboard_path` 指定的文件读取 Gaussian 剪贴板内容。
2. 解析原子序号与坐标。
3. 生成标准 XYZ 文本。
4. 同时写入 `CF_UNICODETEXT` 与 `CF_TEXT` 剪贴板格式。

## 自动清理模型

xyzTrick 的自动清理针对**程序生成的临时 `.log` 文件**，清理行为具有以下特征：

- 删除时机由固定秒数 `wait_seconds` 控制。
- 清理动作在单独线程中执行。
- 清理依据是“启动 GaussianView 后延时 N 秒”，而不是“GaussianView 关闭后立即删除”。
- 若启动 GaussianView 失败，程序会立即尝试删除刚生成的临时文件。

# 安装、部署与构建

## 运行环境

当前版本面向 Windows 桌面环境，使用 Win32 API 实现托盘、热键、剪贴板与进程调用功能。发布与安装器配置面向 **64 位 Windows**。

运行 xyzTrick 需要以下外部条件：

- 可用的 Windows 图形桌面环境。
- 可注册全局热键的用户会话。
- GaussianView 可执行文件（用于 XYZ / CHG / 结构文件的正向打开）。
- 有效的 Gaussian 剪贴板文件路径（用于反向转换）。
- 针对 `.log` / `.out` 的查看器程序（可使用默认值，也可按程序类型分别指定）。

## 安装器行为

当前安装器配置包含以下行为：

- 安装目标应用名为 `xyzTrick`。
- 主程序文件名为 `xyzTrick.exe`。
- 默认建立 `.xyz` 文件关联。
- 安装器可同时复制 `config.ini`、`plugins/clipxtb.exe` 与 `external/OfakeG.exe` 等随附文件。

需要注意：

- `.trj`、`.chg`、`.log`、`.out` 虽然可由程序处理，但安装器当前只为 `.xyz` 写入关联。
- 其他扩展名如需双击打开，应在 Windows 中手工建立“打开方式”关联，或通过命令行显式传入路径。

## 首次启动行为

当程序目录下不存在 `config.ini` 时，首次启动会自动创建默认配置文件，然后继续按该文件重新加载配置。默认配置会写入主程序目录，并包含主热键、反向热键、临时目录、日志目录、列映射与日志查看器等基础设置。

## 源码构建

当前源码仓库使用 MinGW-w64 交叉构建 Windows 可执行文件。主构建入口为顶层 `Makefile`。

### 构建依赖

- `make`
- `x86_64-w64-mingw32-g++`
- `x86_64-w64-mingw32-windres`
- Windows 目标运行库与常用 Win32 库

### 常用构建命令

```bash
make
make debug
make no-res
make clean
make install
```

说明如下：

| 命令 | 作用 |
| --- | --- |
| `make` | 构建 `xyzTrick.exe` 与资源对象。 |
| `make debug` | 以调试参数重新构建。 |
| `make no-res` | 在资源文件不可用时构建无资源版本。 |
| `make clean` | 清理 `build/` 与目标文件。 |
| `make install` | 将主程序复制到 `bin/`，并附带配置与目录骨架。 |

### 构建输出

主程序目标名为：

```text
xyzTrick.exe
```

当前源码中，示例插件 `clipxtb` 采用独立编译方式生成，不由主 `make` 目标自动编译到发布目录；安装器与 CI 工作流会将其作为额外构件打包。

# 配置文件

## `config.ini` 位置与读取规则

xyzTrick 只从**主程序所在目录**读取 `config.ini`。当前版本不搜索用户目录、系统目录或其他备用位置。

读取规则如下：

1. 确定主程序目录。
2. 使用 `<程序目录>\config.ini` 作为配置路径。
3. 若文件不存在，则写出默认配置文件。
4. 加载 `[main]` 与其余插件分区。

图形化设置窗口与“Apply”操作也始终写回主程序目录下的 `config.ini`。

## 配置文件语法

当前版本使用简化 INI 风格，正式语法如下：

- 分区头：`[main]`、`[plugin_name]`
- 键值对：`key=value`
- 前后空白会被修整
- 以 `#` 开头的整行视为注释
- 未识别的键会被忽略
- 非 `[main]` 的所有分区都按“插件分区”处理
- 行内注释不会自动剥离

典型结构如下：

```ini
[main]
hotkey=CTRL+ALT+X
hotkey_reverse=CTRL+ALT+G
gview_path=gview.exe
gaussian_clipboard_path=Clipboard.frg
temp_dir=temp
log_file=logs\xyz_monitor.log
log_level=INFO
log_to_console=true
log_to_file=true
wait_seconds=5
max_memory_mb=500
max_clipboard_chars=0
element_column=1
xyz_columns=2,3,4
try_parse_chg_format=false
orca_log_viewer=notepad.exe
gaussian_log_viewer=gview.exe
other_log_viewer=notepad.exe

[clipxtb]
cmd=plugins\clipxtb.exe
hotkey=CTRL+ALT+D
```

## 路径解析规则

配置中的路径按以下规则解释：

### 文件与目录路径

以下字段按“文件/目录路径”解析：

- `gaussian_clipboard_path`
- `temp_dir`
- `log_file`

处理顺序如下：

1. 展开 Windows 风格环境变量，如 `%GAUSS_EXEDIR%`。
2. 若结果为绝对路径，则直接使用。
3. 若结果为相对路径，则相对于 `config.ini` 所在目录解析。

### 可执行文件路径

以下字段按“可执行程序路径”解析：

- `gview_path`
- `orca_log_viewer`
- `gaussian_log_viewer`
- `other_log_viewer`

处理顺序如下：

1. 展开 Windows 风格环境变量。
2. 若为绝对路径，则直接使用。
3. 若包含相对目录前缀或父路径，则相对于 `config.ini` 所在目录解析。
4. 若仅为文件名，如 `gview.exe` 或 `notepad.exe`，则保留原值，由 Windows 按当前环境与 `PATH` 搜索。

## 主配置项总览

下表给出当前版本**实际读取**的主配置项。未列出的键不会改变程序行为。

| 配置项 | 默认值 | 作用 | 图形界面 |
| --- | --- | --- | --- |
| `hotkey` | `CTRL+ALT+X` | 主热键，触发 XYZ / CHG / 剪贴板文本到 GaussianView 的正向流程。 | 是 |
| `hotkey_reverse` | `CTRL+ALT+G` | 反向热键，触发 Gaussian 剪贴板文件到 XYZ 的反向流程。 | 是 |
| `gview_path` | `gview.exe` | GaussianView 可执行文件路径。 | 是 |
| `gaussian_clipboard_path` | `Clipboard.frg` | Gaussian 剪贴板文件路径。 | 是 |
| `temp_dir` | `<程序目录>\temp` | 临时伪 Gaussian 日志文件目录。 | 否 |
| `log_file` | `<程序目录>\logs\xyz_monitor.log` | 应用日志文件路径。 | 否 |
| `log_level` | `INFO` | 应用日志级别。支持 `DEBUG`、`INFO`、`WARNING`、`ERROR`。 | 否 |
| `log_to_console` | `true` | 是否写标准输出/标准错误。 | 否 |
| `log_to_file` | `true` | 是否写 `log_file` 指定的日志文件。 | 否 |
| `wait_seconds` | `5` | 打开 GaussianView 后删除临时日志文件的固定等待秒数。 | 否 |
| `max_memory_mb` | `500` | 处理剪贴板文本与结构文件时采用的内存预算，用于推导默认字符上限。最小值为 `50`。 | 否 |
| `max_clipboard_chars` | `0` | 最大字符数。`0` 表示按 `max_memory_mb` 自动计算。 | 否 |
| `element_column` | `1` | 元素列，1 基索引。 | 是 |
| `xyz_columns` | `2,3,4` | X/Y/Z 坐标列，1 基索引。 | 是 |
| `try_parse_chg_format` | `false` | 是否在剪贴板文本与非 `.chg` 文件中尝试自动识别 CHG。 | 是 |
| `orca_log_viewer` | `notepad.exe` | ORCA 日志查看器。 | 否 |
| `gaussian_log_viewer` | `gview.exe` | Gaussian 日志查看器。 | 否 |
| `other_log_viewer` | `notepad.exe` | 其他日志查看器。 | 否 |

## 特殊配置项说明

### `max_clipboard_chars`

`max_clipboard_chars` 的行为如下：

- 显式设置为正整数时，程序直接采用该值作为文本长度上限。
- 设置为 `0` 时，程序按 `max_memory_mb` 自动计算：

```text
max_clipboard_chars = clamp(max_memory_mb * 1024 * 1024 / 8, 10000, 100000000)
```

即：

- 估算按 **8 字节/字符** 计算
- 自动下限为 `10000`
- 自动上限为 `100000000`

### `wait_seconds`

当前版本对 `wait_seconds` 采用“直接取整型秒数”策略，不检测 GaussianView 是否仍在运行。该参数的语义是“**启动查看器后等待 N 秒再尝试删除临时文件**”。

### `element_column` 与 `xyz_columns`

这两个设置控制 XYZ / 简化 XYZ 文本的列解释方式：

- 索引从 `1` 开始。
- 只影响 XYZ / 简化 XYZ 解析，不影响 Gaussian 剪贴板文件解析。
- 程序会检查坐标列能否解析为数值；元素列本身在解析阶段不做元素合法性校验。

### `gaussian_clipboard_path`

反向转换严格依赖该文件路径。若路径为空、文件不存在或内容不符合 Gaussian 剪贴板格式，则反向转换失败。

### 日志查看器配置

`orca_log_viewer`、`gaussian_log_viewer`、`other_log_viewer` 只用于**打开 `.log` / `.out` 文件**，不参与 XYZ / CHG 的正向打开。对正向流程，真正被调用的是 `gview_path`。

## 当前版本已忽略的配置键

当前版本只读取上表所列键。若配置文件中存在其他键，例如：

```ini
try_parse_atomic_number=true
```

该键不会改变程序行为。当前版本**不会**将 XYZ / CHG 文本中的元素列按原子序数自动解释。

## 插件分区

除 `[main]` 外，所有分区都被视为插件分区。插件分区当前只识别以下键：

| 键 | 是否必需 | 说明 |
| --- | --- | --- |
| `cmd` | 必需 | 插件启动命令行。 |
| `hotkey` | 可选 | 插件热键。留空时仅可从托盘菜单或设置窗口的“Run”按钮执行。 |

当前版本不支持插件分区中的 `enabled=`、自定义参数表或嵌套配置。若要停用插件，应删除对应分区或移除其热键与菜单来源。

# 输入模型与支持格式

## 输入来源总览

| 来源 | 入口 | 当前支持 |
| --- | --- | --- |
| 剪贴板文本 | 主热键 `hotkey` | 标准 XYZ、简化 XYZ、在启用时自动识别的 CHG |
| Gaussian 剪贴板文件 | 反向热键 `hotkey_reverse` | Gaussian 剪贴板坐标格式 |
| 结构文件 | `xyzTrick.exe <file>`、文件关联、打开方式 | `.xyz`、`.trj`、`.chg` |
| 日志文件 | `xyzTrick.exe <file>`、打开方式 | `.log`、`.out` |

## 文本文件读取与编码处理

对于文件输入，xyzTrick 采用统一的文本读取流程：

1. 以二进制方式读取原始字节。
2. 单次读取上限为 **100 MB**；超出部分不会继续读取。
3. 基于 BOM、空字节分布与 UTF-8 合法性检测编码。
4. 将内容转换为 UTF-8。
5. 将内部换行统一为 `LF`。

当前自动检测的主要结论包括：

- UTF-8（含 BOM 与不含 BOM）
- UTF-16 LE / BE
- UTF-32 LE / BE
- 未能判定为上述格式时，按系统 ANSI 代码页读取

程序枚举中虽定义了 GBK、Big5、Shift-JIS、EUC-KR 等名称，但当前自动检测逻辑主要在“Unicode 族”与“系统 ANSI”之间做判断。

## XYZ 格式

### 标准 XYZ

当前版本将以下文本识别为标准 XYZ：

- 第一个非空行为原子数整数
- 第二行为注释行，可以为空
- 随后的原子坐标行数量与首行原子数一致

示例：

```text
3
Water
O  0.000000  0.000000  0.000000
H  0.758602  0.000000  0.504284
H -0.758602  0.000000  0.504284
```

标准 XYZ 的当前行为如下：

- 支持空注释行。
- 支持多帧连续块；相邻帧前允许存在空白行。
- 每一帧的注释行都会单独保留并尝试解析优化信息。
- 每一帧的坐标行数量必须与帧头原子数一致。

### 简化 XYZ

若内容不以原子数开头，但所有非空行都满足“元素列 + 坐标列可解析”的条件，则按简化 XYZ 处理。此时：

- 整个文本视为单帧。
- 注释固定记为 `Simplified XYZ format`。
- 不存在独立的原子数行与注释行。

示例：

```text
C  0.000000  0.000000  0.000000
H  0.000000  0.000000  1.089000
H  1.026719  0.000000 -0.363000
H -0.513360 -0.889165 -0.363000
H -0.513360  0.889165 -0.363000
```

### 坐标列解释

XYZ 与简化 XYZ 的坐标解析遵循 `element_column` 与 `xyz_columns`：

- 坐标列必须是可解析的数值。
- 元素列内容在解析阶段不做元素表校验。
- 为保证生成的伪 Gaussian 日志中原子序数正确，元素列应填写标准元素符号，如 `H`、`C`、`Cl`。
- 当前版本额外识别 `Tv` 作为晶胞平移向量的特殊标识，其内部原子序数映射为 `-2`。
- 若元素列写入数字字符串而不是元素符号，程序不会自动按原子序数解释，生成的伪 Gaussian 日志中对应原子序数可能为 `0`。

## CHG 格式

CHG 的当前正式格式为：

```text
Element X Y Z Charge
```

每行至少需要五列：

1. 元素符号
2. X
3. Y
4. Z
5. 电荷

示例：

```text
O  0.000000  0.000000  0.000000  -0.834
H  0.758602  0.000000  0.504284   0.417
H -0.758602  0.000000  0.504284   0.417
```

当前行为如下：

- 空行与以 `#` 开头的行会被跳过。
- 第一列必须以字母开头；当前版本不支持第一列写原子序数。
- 第五列电荷会被保存到原子对象中。
- 在生成伪 Gaussian 日志时，若存在电荷数据，程序会在日志尾部写出 Mulliken 电荷表。
- 对于剪贴板文本与非 `.chg` 文件，只有 `try_parse_chg_format=true` 时才会自动识别 CHG。
- 对于扩展名明确为 `.chg` 的文件，不受 `try_parse_chg_format` 开关影响，始终按 CHG 处理。

## Gaussian 剪贴板文件格式

反向转换从 `gaussian_clipboard_path` 指定的文件读取内容。当前解析器采用以下格式：

1. 第 1 行：头部，内容忽略。
2. 第 2 行：原子数整数。
3. 第 3 行起：逐行原子数据。

每个原子行的当前正式解析格式为：

```text
atomic_number x y z [label]
```

其中：

- `atomic_number` 必需
- `x y z` 必需
- `label` 可选，当前版本读取后不参与输出

反向输出到剪贴板的 XYZ 文本固定写成：

```text
<atom_count>
Converted from Gaussian clipboard
<coordinates...>
```

电荷与自旋信息不会从该流程中回写到 XYZ 第二行。

## 优化信息注释解析

在标准 XYZ 的注释行中，程序当前会尝试提取以下标记：

| 标记 | 含义 |
| --- | --- |
| `E=` | 能量 |
| `MaxF=` | 最大受力 |
| `RMSF=` | 方均根受力 |
| `MaxD=` | 最大位移 |
| `RMSD=` | 方均根位移 |

当前实现按正则表达式在原注释行中直接搜索这些字样。为确保正确识别，应采用与上表一致的写法。

示例：

```text
Frame 2  E=-100.125  MaxF=0.0012  RMSF=0.0008  MaxD=0.0031  RMSD=0.0017
```

# 功能说明

## XYZ / CHG 到 GaussianView

### 剪贴板正向流程

主热键触发后，程序按以下顺序执行：

1. 优先读取 `CF_UNICODETEXT`。
2. 若剪贴板中不存在 Unicode 文本，则退回读取 `CF_TEXT`。
3. 若剪贴板为空、非文本或长度超过 `max_clipboard_chars`，则终止处理。
4. 当 `try_parse_chg_format=true` 且文本满足 CHG 检测条件时，按 CHG 解析。
5. 否则，若文本满足 XYZ 检测条件，则按 XYZ 解析。
6. 将解析结果写成伪 Gaussian 日志。
7. 在 `temp_dir` 中创建唯一临时 `.log` 文件。
8. 调用 `gview_path` 启动 GaussianView。
9. 在独立线程中等待 `wait_seconds` 秒，然后删除该临时文件。

当前版本中，正向流程只读取**剪贴板文本**，不会从剪贴板文件列表或资源管理器复制的文件对象中取结构内容。

### 文件正向流程

对于 `.xyz`、`.trj`、`.chg` 文件，程序流程与剪贴板正向流程基本一致，只是输入来源改为文件内容。`.trj` 与 `.xyz` 在当前版本中共用同一解析器。

### 伪 Gaussian 日志的用途边界

xyzTrick 生成的 `.log` 文件用于满足 GaussianView 的可视化输入需求。该日志具备如下特征：

- 头部包含伪造的 Gaussian 标识行。
- 每一帧写出一组 `Standard orientation` 坐标表。
- 可选写出 `SCF Done:` 能量与收敛表。
- 如存在 CHG 电荷，则在尾部写出 Mulliken 电荷汇总。
- 结尾写出 `Normal termination of Gaussian`。

该文件的目标是**可视化与浏览**，而不是还原真实 Gaussian 计算历史。它不应作为计算结果归档、任务恢复或正式日志再解析的替代物。

## GaussianView 到 XYZ

反向热键触发后，程序按以下顺序执行：

1. 读取 `gaussian_clipboard_path` 指向的文件。
2. 解析第二行为原子数。
3. 从后续各行读取原子序数与坐标。
4. 将原子序数映射为元素符号。
5. 生成标准 XYZ 文本。
6. 以 Unicode 与 ANSI 两种文本格式写回剪贴板。

当前成功通知会显示转换得到的原子数量。若文件不存在、格式不符、原子数解析失败或无可用原子，反向流程会终止并给出错误或警告通知。

## 文件打开模式

当前版本支持以下文件扩展名：

| 扩展名 | 行为 |
| --- | --- |
| `.xyz` | 按 XYZ / 简化 XYZ / 可选 CHG 自动解析，转换为伪 Gaussian 日志后交给 GaussianView 打开。 |
| `.trj` | 与 `.xyz` 相同，适用于多帧轨迹文本。 |
| `.chg` | 直接按 CHG 解析，转换为伪 Gaussian 日志后交给 GaussianView 打开。 |
| `.log` | 识别日志类型后，用对应日志查看器直接打开，不做结构转换。 |
| `.out` | 与 `.log` 相同。 |

不属于上述范围的扩展名会被判定为不支持格式。

## 日志文件分类与查看器选择

对于 `.log` 与 `.out`，程序只读取文件前 5 行进行快速识别：

| 识别结果 | 检测标志 | 打开程序 |
| --- | --- | --- |
| ORCA | 前 5 行中包含 `* O   R   C   A *` | `orca_log_viewer` |
| Gaussian | 前 5 行中包含 `Entering Gaussian System` | `gaussian_log_viewer` |
| Other | 未匹配上述标志 | `other_log_viewer` |

识别时不区分大小写。若查看器路径为空或程序启动失败，则日志打开失败。

## 多帧与优化信息生成规则

### 多帧输出

对于多帧 XYZ，程序会在伪 Gaussian 日志中顺序写出每一帧的标准坐标表，并为每一帧写入：

- `Step number`
- 收敛表头
- 最大受力 / 方均根受力 / 最大位移 / 方均根位移

### 能量与收敛数据的补用规则

每帧的能量与优化数据遵循以下规则：

- 当前帧注释行中存在 `E=` 时，写当前帧能量。
- 当前帧无 `E=` 而上一帧存在时，复用上一帧能量。
- 若当前帧与上一帧都没有能量，则写默认值 `-100.000000000`。
- 收敛表数据优先使用当前帧的 `MaxF/RMSF/MaxD/RMSD`。
- 若当前帧缺少这组数据而上一帧存在，则复用上一帧收敛值。
- 若两帧都缺失，则写占位值 `1.000000` 并标记为 `NO`。

### 固定阈值

伪 Gaussian 日志中的阈值为当前版本内置常量：

| 项目 | 阈值 |
| --- | --- |
| Maximum Force | `0.00045` |
| RMS Force | `0.00030` |
| Maximum Displacement | `0.00180` |
| RMS Displacement | `0.00120` |

这些阈值当前不可通过配置文件修改。

## CHG 电荷输出

当任一帧原子包含非零 `charge` 数据时，日志尾部会追加 Mulliken 电荷段：

- 仅使用**最后一帧**的电荷数据输出。
- 自旋密度固定写为 `0.0`。
- 最后写出 `Sum of Mulliken charges` 总和。

这使 CHG 文本在 GaussianView 中可以通过伪日志尾部携带一份简化电荷表。

## 临时文件命名与删除

正向流程生成的临时文件命名如下：

```text
molecule_<milliseconds>_<tickcount>_<threadid>.log
```

当前规则如下：

- 当 `temp_dir` 非空时，使用 `temp_dir`。
- 当 `temp_dir` 为空时，退回系统临时目录。
- 每次触发都会创建新文件名，避免同秒重复触发覆盖旧文件。
- 删除动作只针对该次流程创建的临时 `.log` 文件。

# 热键、托盘与设置窗口

## 托盘行为

驻留模式下，程序会在系统托盘创建图标并提供以下交互：

- **双击托盘图标**：打开设置窗口。
- **右键托盘图标**：弹出托盘菜单。

当前托盘菜单包含以下项目：

| 菜单项 | 作用 |
| --- | --- |
| `XYZ Monitor v2.1.0 - by Author: Bane Dysta` | 打开设置窗口并切换到 About 选项卡。 |
| `Plugins` | 列出已启用插件，并显示插件热键文本（如有）。 |
| `Reload Configuration` | 重新从磁盘加载 `config.ini`，并重新注册插件热键；当主热键文本发生变化时，会重新注册主热键。 |
| `Exit` | 退出程序。 |

## 热键语法

热键字符串当前采用如下写法：

```text
MODIFIER+MODIFIER+KEY
```

### 支持的修饰键

- `CTRL`
- `ALT`
- `SHIFT`
- `WIN`

### 支持的主键

- 单个字母或数字：`A`、`X`、`7` 等
- 功能键：`F1` 到 `F24`
- 命名键：`TAB`、`SPACE`、`ENTER`、`RETURN`、`ESC`、`ESCAPE`、`BACKSPACE`、`BS`、`DELETE`、`DEL`、`INSERT`、`INS`、`HOME`、`END`、`PAGEUP`、`PGUP`、`PAGEDOWN`、`PGDN`、`LEFT`、`RIGHT`、`UP`、`DOWN`

热键解析不区分大小写。当前版本不支持将符号键直接作为主键，例如 `;`、`/`、`-`、`=` 等。

### 注册结果

- 主热键注册失败会导致程序启动失败。
- 反向热键注册失败会记录错误，但不阻止程序继续运行。
- 插件热键注册失败只影响对应插件，不阻止主程序运行。

## 设置窗口

设置窗口由四个选项卡组成：

| 选项卡 | 当前内容 |
| --- | --- |
| `General` | 主热键、反向热键、GaussianView 路径、Gaussian Clipboard 路径、Open Log File 按钮 |
| `Control` | Element 列、X/Y/Z 列、CHG 自动解析开关 |
| `About` | 程序版本、作者、说明文字、GitHub 与论坛入口 |
| `Plugins` | 已安装插件列表、名称、命令、热键、Add / Remove / Run 按钮 |

### `General` 选项卡

当前界面可直接修改以下字段：

- `Primary Hotkey (XYZ->GView)`
- `Reverse Hotkey (GView->XYZ)`
- `GView Executable Path`
- `Gaussian Clipboard Path`

`Open Log File` 按钮会打开 `log_file` 指定的应用日志文件：

- 若日志目录不存在，会先创建目录。
- 若日志文件不存在，会先创建空文件。
- 文件使用 Windows 默认关联程序打开，而不是使用 `orca_log_viewer` / `gaussian_log_viewer` / `other_log_viewer`。

### `Control` 选项卡

当前界面可直接修改以下字段：

- `Element Column`
- `XYZ Columns`（X、Y、Z 三个独立输入框）
- `Try Parse CHG Format (Element X Y Z Charge)`

### `Plugins` 选项卡

当前界面支持以下操作：

- 查看已加载插件名称列表
- 添加插件
- 删除插件
- 直接运行选中的插件

添加插件时，`Plugin Name` 与 `Command` 为必填项，`Hotkey` 为可选项。

## `OK`、`Apply` 与 `Cancel`

- `OK`：验证输入，通过后保存配置并立即应用，然后隐藏设置窗口。
- `Apply`：验证输入，通过后保存配置并立即应用，窗口保持打开。
- `Cancel`：放弃未保存修改，恢复当前内存中的配置副本并隐藏窗口。

当前图形界面**不暴露**以下配置项：

- `temp_dir`
- `log_file`
- `log_level`
- `log_to_console`
- `log_to_file`
- `wait_seconds`
- `max_memory_mb`
- `max_clipboard_chars`
- `orca_log_viewer`
- `gaussian_log_viewer`
- `other_log_viewer`

这些项需通过手工编辑 `config.ini` 后，再使用托盘菜单的 `Reload Configuration` 生效。

# 插件系统

## 插件模型

插件系统用于将任意外部命令挂接到 xyzTrick 的托盘菜单与全局热键体系中。插件本身不需要遵循特定 API；当前版本只负责：

- 从配置文件加载插件定义
- 在托盘菜单中列出插件
- 为插件注册可选热键
- 以独立进程方式启动插件命令

## 插件定义方式

插件通过 `config.ini` 中的非 `[main]` 分区定义。例如：

```ini
[clipxtb]
cmd=plugins\clipxtb.exe
hotkey=CTRL+ALT+D
```

此配置会生成一个名为 `clipxtb` 的插件：

- 托盘菜单中显示为 `clipxtb (CTRL+ALT+D)`
- 按下 `CTRL+ALT+D` 时调用该插件
- 在设置窗口的 `Plugins` 选项卡中可见、可删除、可运行

## 插件执行模型

当前版本使用 `CreateProcessA` 直接启动 `cmd=` 所给出的命令行文本，执行模型具有以下特征：

- 不附加额外的 shell 解释层。
- 不自动对插件命令做“相对于 `config.ini` 的路径归一化”。
- 不自动追加工作目录、参数模板或环境变量。
- 若命令行需要使用重定向、管道或 shell 内建命令，应显式写成 `cmd.exe /c ...`。
- 若命令中使用相对路径，应确保该路径在当前进程工作目录下可解析，或改为绝对路径。

## 插件热键

插件热键与主热键使用同一套语法解析规则。插件热键 ID 在运行时动态分配，不要求在配置中显式指定编号。

插件热键注册失败时：

- 对应插件仍保留在托盘菜单与设置窗口中。
- 该插件热键不可用。
- 主程序继续运行。

## 插件管理行为

### 添加同名插件

若在设置窗口中以现有插件名称再次执行“Add Plugin”，当前版本会更新同名插件的 `cmd` 与 `hotkey`，而不是创建重名副本。

### 删除插件

删除插件会：

1. 从内存配置中移除该项。
2. 将变更写回 `config.ini`。
3. 重新注册插件热键。

### 运行插件

插件可通过以下方式运行：

- 托盘菜单中的插件项
- 插件热键
- 设置窗口 `Plugins` 选项卡中的 `Run` 按钮

## 随附示例插件 `clipxtb`

当前源码仓库与安装器配置包含 `clipxtb` 示例插件。该插件的定位是：

- 从剪贴板文本或资源管理器复制的 `.xyz` 文件中读取结构
- 必要时要求输入电荷与自旋多重度
- 调用 `xtb` 执行优化
- 将优化后 XYZ 文本重新写回剪贴板

`clipxtb` 使用独立的 `xtbclip.ini` 管理其内部参数，当前与 xyzTrick 主配置项分离。xyzTrick 只负责启动该插件，不解释其私有配置。

# 生成物与输出

## 主程序工件

| 工件 | 类型 | 说明 |
| --- | --- | --- |
| `config.ini` | 文本 | 主配置文件；首次启动可自动生成。 |
| `logs\xyz_monitor.log` | 文本 | 应用日志。默认位于程序目录下的 `logs/`。 |
| `temp\molecule_*.log` | 文本 | 伪 Gaussian 临时日志文件；由正向流程生成并延时删除。 |
| 剪贴板文本 | 虚拟输出 | 反向流程将标准 XYZ 同时写入 Unicode 与 ANSI 文本格式。 |

## 应用日志内容

应用日志记录以下类别的信息：

- 程序启动与停止
- 配置加载与重载
- 热键注册结果
- 剪贴板读取与写回结果
- 文件读取、编码检测与类型识别结果
- 插件执行结果
- 错误与警告

日志级别说明如下：

| 级别 | 含义 |
| --- | --- |
| `DEBUG` | 详细调试信息 |
| `INFO` | 常规运行信息 |
| `WARNING` | 非致命异常与降级行为 |
| `ERROR` | 当前流程失败或严重错误 |

# 示例

## 默认配置示例

```ini
[main]
hotkey=CTRL+ALT+X
hotkey_reverse=CTRL+ALT+G
gview_path=gview.exe
gaussian_clipboard_path=Clipboard.frg
temp_dir=temp
log_file=logs\xyz_monitor.log
log_level=INFO
log_to_console=true
log_to_file=true
wait_seconds=5
max_memory_mb=500
max_clipboard_chars=0
element_column=1
xyz_columns=2,3,4
try_parse_chg_format=false
orca_log_viewer=notepad.exe
gaussian_log_viewer=gview.exe
other_log_viewer=notepad.exe
```

## 标准 XYZ 示例

```text
5
Methane
C  0.000000  0.000000  0.000000
H  0.000000  0.000000  1.089000
H  1.026719  0.000000 -0.363000
H -0.513360 -0.889165 -0.363000
H -0.513360  0.889165 -0.363000
```

在默认列映射下，复制上述文本并触发主热键后，程序会生成临时伪 Gaussian 日志并调用 GaussianView 打开。

## 多帧 XYZ 示例

```text
3
Frame 1 E=-40.123 MaxF=0.0045 RMSF=0.0023 MaxD=0.0100 RMSD=0.0050
O  0.000000  0.000000  0.000000
H  0.758602  0.000000  0.504284
H -0.758602  0.000000  0.504284

3
Frame 2 E=-40.128 MaxF=0.0003 RMSF=0.0002 MaxD=0.0008 RMSD=0.0004
O  0.000000  0.000000  0.010000
H  0.758602  0.000000  0.514284
H -0.758602  0.000000  0.514284
```

该文本会在伪 Gaussian 日志中生成两个连续步，并带有 `Step number` 与收敛表。

## CHG 示例

```text
C   0.000000   0.000000   0.000000  -0.120000
H   0.000000   0.000000   1.089000   0.030000
H   1.026719   0.000000  -0.363000   0.030000
H  -0.513360  -0.889165  -0.363000   0.030000
H  -0.513360   0.889165  -0.363000   0.030000
```

当 `try_parse_chg_format=true` 时，复制上述文本并触发主热键，程序会生成带 Mulliken 电荷尾段的伪 Gaussian 日志。

## 文件模式示例

```bash
xyzTrick.exe D:\data\sample.xyz
xyzTrick.exe D:\data\trajectory.trj
xyzTrick.exe D:\data\orca.out
```

处理结果如下：

- `sample.xyz`：解析结构并交给 GaussianView 打开
- `trajectory.trj`：按多帧 XYZ 解析并交给 GaussianView 打开
- `orca.out`：识别为 ORCA 日志后，使用 `orca_log_viewer` 打开

## 插件示例

```ini
[clipxtb]
cmd=plugins\clipxtb.exe
hotkey=CTRL+ALT+D
```

启用后，插件会在托盘菜单与设置窗口中出现；按下 `CTRL+ALT+D` 时，程序尝试执行 `plugins\clipxtb.exe`。

# 当前行为与限制

## 平台与运行模式

- 当前版本面向 Windows 图形桌面。
- 命令行只接受单个文件参数；多余参数不会参与批处理。
- 驻留模式与文件参数模式互相独立，文件参数模式不创建托盘与热键。

## 输入与格式

- 主热键正向流程只读取剪贴板文本，不读取剪贴板中的文件列表。
- `.xyz` / `.trj` 的元素列应写元素符号；当前版本不会自动把数值元素列解释为原子序数。
- `try_parse_atomic_number` 等未识别配置键不会生效。
- CHG 自动识别仅在 `try_parse_chg_format=true` 时对剪贴板文本与非 `.chg` 文件启用。
- 日志类型识别只检查文件前 5 行。
- 文件读取存在 100 MB 原始读取上限；超大文件不会完整载入。

## 路径与配置

- `config.ini` 位置固定在主程序目录。
- 图形界面不覆盖所有配置项；文件级参数需手工编辑后再重载。
- 插件命令不做额外路径归一化；复杂命令行应显式使用 `cmd.exe /c`。

## 正向与反向转换

- 正向流程生成的是供查看使用的伪 Gaussian 日志，不等同于真实 Gaussian 输出。
- `wait_seconds` 控制的是固定延时删除，而不是等待 GaussianView 退出。
- 反向流程依赖外部剪贴板文件路径，无法直接从 GaussianView 进程内存抓取结构。
- 反向写回的 XYZ 第二行固定为说明文字，不保留 Gaussian 剪贴板中的电荷/自旋信息。

## 文件关联与查看器

- 当前安装器默认只注册 `.xyz` 文件关联。
- `.trj`、`.chg`、`.log`、`.out` 需要手工建立系统“打开方式”，或通过命令行显式传入。
- `.log` / `.out` 的打开依赖对应查看器可执行文件存在且可启动。

# 更新历史

## 2.1.0

- 修复插件菜单命令 ID 与托盘基础命令冲突的问题。
- 剪贴板读写优先采用 Unicode 文本，提高中文与非 ASCII 文本兼容性。
- 修复标准 XYZ 空注释行与中间空行处理。
- 扩展热键解析，新增对 `Tab`、`Space`、方向键、`Insert/Delete/Home/End/PageUp/PageDown`、`F1-F24` 的支持。
- 布尔值解析改为大小写不敏感，并支持 `true/false/yes/no/on/off/1/0`。
- 重载配置与设置保存后，同步重新注册插件热键。
- 修复文件大小与实际读取字节数的边界判断问题。

## 2.0.3

- 新增对 `.log` 文件的接受与打开能力。
- 打开日志文件时，按 Gaussian、ORCA、其他日志区分查看器。

## 2.0.2

- 新增 CHG 格式支持。
- 新增元素列与坐标列位置配置。
