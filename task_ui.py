import sys


def _bootstrap():
    try:
        from tools.task_ui_qt import main
    except ModuleNotFoundError as exc:
        if exc.name == "PySide6":
            sys.stderr.write(
                "缺少 PySide6，无法启动新的 Qt 界面。\n"
                "请先执行：python -m pip install PySide6==6.11.1\n"
            )
            raise SystemExit(1) from exc
        raise
    main()


if __name__ == "__main__":
    _bootstrap()
