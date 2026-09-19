import os
import json

workspace = 'E:/kimodo.cpp/kimodo-studio'
src_dir = os.path.join(workspace, 'src')

includes = [
    f'-I{workspace}/src',
    f'-I{workspace}/build/windows-vs2022/_deps/raylib-src/src',
    f'-I{workspace}/build/windows-ninja-gcc/_deps/raylib-src/src',
    f'-I{workspace}/build/windows-vs2022/_deps/imgui-src',
    f'-I{workspace}/build/windows-ninja-gcc/_deps/imgui-src',
    f'-I{workspace}/build/windows-vs2022/_deps/rlimgui-src',
    f'-I{workspace}/build/windows-ninja-gcc/_deps/rlimgui-src',
    f'-I{workspace}/build/windows-vs2022/_deps/nativefiledialog-src/src/include',
    f'-I{workspace}/build/windows-ninja-gcc/_deps/nativefiledialog-src/src/include',
    '-IE:/kimodo.cpp/include'
]

defines = [
    '-DWIN32',
    '-D_WIN32',
    '-D_CRT_SECURE_NO_WARNINGS',
    f'-DKIMODO_STUDIO_SOURCE_DIR=\\"{workspace}\\"',
    '-DKIMODO_ROOT_DIR=\\"E:/kimodo.cpp\\"',
    '-DKIMODO_HAVE_BACKEND=1',
    '-DKIMODO_STUDIO_VERSION=\\"0.1.0\\"',
    '-DKIMODO_STUDIO_GIT_HASH=\\"dev\\"',
    '-DKIMODO_STUDIO_BUILD_DATE=\\"dev\\"'
]

entries = []
for root, _, files in os.walk(src_dir):
    for f in files:
        if f.endswith(('.cpp', '.c', '.h', '.hpp', '.inl')):
            file_path = os.path.join(root, f).replace('\\', '/')
            command = 'clang++ -std=c++23 ' + ' '.join(defines) + ' ' + ' '.join(includes) + f' -c "{file_path}"'
            entries.append({
                'directory': workspace,
                'command': command,
                'file': file_path
            })

out_path = os.path.join(workspace, 'compile_commands.json')
with open(out_path, 'w', encoding='utf-8') as out:
    json.dump(entries, out, indent=2)

print(f'Generated {len(entries)} compile commands entries at {out_path}')
