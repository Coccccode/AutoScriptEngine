# AutoScriptEngine

一个以 C++ 为核心、Python UI 为操作层的自动化项目。当前主 UI 已迁移到 `PySide6`，保留旧 `Tk/customtkinter` 版本作为参考实现。

## 目录结构

```text
learnOpencv/
├── src/                 C++ 源文件
├── include/             C++ 头文件
├── configs/             运行配置与多实例配置
├── tasks/               任务流程 JSON
├── tools/               Python 工具脚本与 UI 实现
├── resource/            模板图与资源文件
├── vendor/              第三方运行时目录（如 PaddleOCR）
├── artifacts/           运行期输出（截图、导出结果等）
├── build/               Python 扩展构建产物
├── x64/                 Visual Studio 构建产物
├── task_ui.py           Qt UI 启动入口
├── setup.py             pybind11 扩展构建脚本
├── learnOpencv.sln      Visual Studio 解决方案
└── learnOpencv.vcxproj  Visual Studio 工程
```

## UI 说明

- `task_ui.py`: 当前默认入口，启动新的 `PySide6` 界面
- `tools/task_ui_qt.py`: Qt 主实现
- `tools/task_ui.py`: 旧的 Tk/customtkinter 版本，保留作参考与回退

## 安装与启动

```bash
python -m pip install -r requirements.txt
python setup.py build_ext --inplace
python task_ui.py
```

如果网络环境直连 PyPI 不稳定，可以使用镜像安装 `PySide6`：

```bash
python -m pip install PySide6==6.11.1 -i http://pypi.tuna.tsinghua.edu.cn/simple --trusted-host pypi.tuna.tsinghua.edu.cn
```

## 当前 Qt 版本已覆盖的核心能力

- 多实例切换
- 设备刷新与连接/断开
- WGC / ADB 预览
- 任务文件选择
- C++ 任务启动与停止
- 日志输出
- 实例级配置保存

## 约定

- 新增 C++ 源文件放 `src/`
- 新增头文件放 `include/`
- 新增实例配置放 `configs/`
- 新增任务流程放 `tasks/`
- 新增 Python 工具放 `tools/`
- 第三方运行目录优先放 `vendor/`
- 运行时输出统一放 `artifacts/`
