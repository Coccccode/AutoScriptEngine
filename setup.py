from pathlib import Path

from pybind11.setup_helpers import Pybind11Extension, build_ext
from setuptools import setup


ROOT = Path(__file__).resolve().parent

sources = [
    "PyBridge.cpp",
    "AdbController.cpp",
    "CachedController.cpp",
    "CaptureService.cpp",
    "CommonMath.cpp",
    "Controller.cpp",
    "MouseController.cpp",
    "OcrApi.cpp",
    "OpencvAPI.cpp",
    "TaskManager.cpp",
    "TaskControl.cpp",
    "TaskRunner.cpp",
    "SharedFrameCache.cpp",
    "WindowsGraphicsCapture.cpp",
    "WindowCapture.cpp",
    "WindowConfig.cpp",
    "WindowController.cpp",
]

ext_modules = [
    Pybind11Extension(
        "learnopencv_py",
        sources=sources,
        include_dirs=[
            str(ROOT),
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
