#!/usr/bin/env python3
import glob
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EMSDK = os.environ.get("EMSDK", r"D:\emsdk")
OUT = os.path.join(ROOT, "web", "build", "mw.js")

SOURCES = [
    "src/main.cpp",
]

FLAGS = [
    "-std=c++20",
    "-O2",
    "-sWASM=1",
    "-sENVIRONMENT=web",
    "-sMIN_WEBGL_VERSION=2",
    "-sMAX_WEBGL_VERSION=2",
    "-sMODULARIZE=1",
    "-sEXPORT_ES6=1",
    "-sEXPORT_NAME=createMw",
    "-sALLOW_MEMORY_GROWTH=1",
    "-sDEFAULT_TO_CXX=1",
    "-sINITIAL_MEMORY=134217728",
    "-sSTACK_SIZE=1048576",
    "-sASSERTIONS=0",
    "-I",
    os.path.join(EMSDK, "upstream", "emscripten", "third_party"),
    "--preload-file",
    os.path.join(ROOT, "web", "assets", "city.mwb") + "@/assets/city.mwb",
    "--preload-file",
    os.path.join(ROOT, "web", "assets", "citytex.bin") + "@/assets/citytex.bin",
    "-lGL",
]


def find_toolchain():
    emcc = os.path.join(EMSDK, "upstream", "emscripten", "emcc.py")
    pythons = sorted(glob.glob(os.path.join(EMSDK, "python", "*", "python.exe")))
    nodes = sorted(glob.glob(os.path.join(EMSDK, "node", "*", "node.exe")))
    if not os.path.isfile(emcc) or not pythons:
        return None

    env = os.environ.copy()
    env["EMSDK"] = EMSDK
    config = os.path.join(EMSDK, ".emscripten")
    if os.path.isfile(config):
        env["EM_CONFIG"] = config
    paths = [os.path.join(EMSDK, "upstream", "emscripten")]
    if nodes:
        env["EMSDK_NODE"] = nodes[-1]
        paths.append(os.path.dirname(nodes[-1]))
    env["PATH"] = os.pathsep.join(paths + [env.get("PATH", "")])
    env.pop("MSYSTEM", None)
    env.pop("MSYSCON", None)
    return [pythons[-1], emcc], env


def main() -> int:
    toolchain = find_toolchain()
    if toolchain is None:
        print(f"emsdk nao encontrado ou incompleto em {EMSDK}", file=sys.stderr)
        return 1
    launcher, env = toolchain

    os.makedirs(os.path.dirname(OUT), exist_ok=True)

    debug = "--debug" in sys.argv
    flags = list(FLAGS)
    if debug:
        flags = [f for f in flags if f != "-O2"]
        flags += ["-O0", "-gsource-map", "-sASSERTIONS=2", "-sSAFE_HEAP=1"]

    cmd = [*launcher, *SOURCES, "-o", OUT, *flags]
    shown = " ".join(f'"{a}"' if " " in a else a for a in cmd)

    print(f"build: {shown[:160]}...")
    code = subprocess.call(cmd, cwd=ROOT, env=env)

    if code != 0:
        return code

    for path in (OUT, OUT[:-3] + ".wasm"):
        if os.path.isfile(path):
            print(f"  {os.path.relpath(path, ROOT):24s} {os.path.getsize(path) / 1024:8.1f} KB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
