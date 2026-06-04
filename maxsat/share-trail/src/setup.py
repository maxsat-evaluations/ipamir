from setuptools import Extension, setup
import os

cgss2_dir = os.path.abspath(".")
enable_lto = 1
enable_march_native = 1

safe_compile_args = [
    "-std=c++17",
    "-O3",
    "-DNDEBUG",
    "-fvisibility=hidden",
    "-ffunction-sections",
    "-fdata-sections",
    "-fno-semantic-interposition",
    "-fomit-frame-pointer",
    "-ffp-contract=fast",
]
safe_link_args = []

if enable_lto:
    safe_compile_args.append("-flto=auto")
    safe_link_args.append("-flto=auto")

if enable_march_native:
    safe_compile_args.append("-march=native")

ext_modules = [
    Extension(
        "cgss2_native",
        ["cgss2_pybind.cpp"],
        include_dirs=[cgss2_dir],
        extra_objects=[
            os.path.join(cgss2_dir, "lib/libcgss2.a"),
            os.path.join(cgss2_dir, "lib/libglucose41.a"),
        ],
        language="c++",
        extra_compile_args=safe_compile_args,
        extra_link_args=safe_link_args,
    )
]

setup(
    name="cgss2_native",
    version="0.0.1",
    ext_modules=ext_modules,
)
