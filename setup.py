from pathlib import Path

from pybind11.setup_helpers import Pybind11Extension, build_ext
from setuptools import setup


ROOT = Path(__file__).resolve().parent
SRC = ROOT / "src"
INCLUDE = ROOT / "include"

sources = [
    str(SRC / "PyBridge.cpp"),
    str(SRC / "AdbController.cpp"),
    str(SRC / "CachedController.cpp"),
    str(SRC / "CaptureService.cpp"),
    str(SRC / "CommonMath.cpp"),
    str(SRC / "Controller.cpp"),
    str(SRC / "MouseController.cpp"),
    str(SRC / "OcrApi.cpp"),
    str(SRC / "OpencvAPI.cpp"),
    str(SRC / "TaskManager.cpp"),
    str(SRC / "TaskControl.cpp"),
    str(SRC / "TaskRunner.cpp"),
    str(SRC / "SharedFrameCache.cpp"),
    str(SRC / "WindowsGraphicsCapture.cpp"),
    str(SRC / "WindowCapture.cpp"),
    str(SRC / "WindowConfig.cpp"),
    str(SRC / "WindowController.cpp"),
]

ext_modules = [
    Pybind11Extension(
        "learnopencv_py",
        sources=sources,
        include_dirs=[
            str(ROOT),
            str(INCLUDE),
            r"D:\Environment\opencv\build\include",
            r"D:\Environment\opencv\build\include\opencv2",
            r"D:\Environment\jsoncpp\include",
        ],
        library_dirs=[
            r"D:\Environment\opencv\build\x64\vc15\lib",
            r"D:\Environment\jsoncpp\lib",
        ],
        libraries=[
            "opencv_world450",
            "jsoncpp_static",
            "user32",
            "gdi32",
            "d3d11",
            "dxgi",
            "windowsapp",
            "Ws2_32",
        ],
        cxx_std=17,
        extra_compile_args=["/EHsc", "/utf-8"],
    )
]

setup(
    name="learnopencv_py",
    version="0.1.0",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
)
