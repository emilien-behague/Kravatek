#!/usr/bin/env python3
"""Build in the Debian cloud workspace without installing system packages."""

import os
from pathlib import Path
import shutil
import subprocess

PROJECT = Path(__file__).resolve().parents[1]
TOOLS = Path('/workspace/.kravatek-tools')
JUCE_COMMIT = 'ea77444750de45e036e0279bb0291144bf8e3688'


def run(*command, env=None):
    print('+', ' '.join(str(x) for x in command), flush=True)
    subprocess.run([str(x) for x in command], check=True, env=env)


def main():
    for tool in ('python3', 'git', 'g++', 'pkg-config', 'dpkg-deb', 'uv'):
        if not shutil.which(tool):
            raise RuntimeError(f'Missing prerequisite: {tool}. No system install was attempted.')
    if 'VERSION_CODENAME=trixie' not in Path('/etc/os-release').read_text():
        raise RuntimeError('This local package extraction script targets Debian 13 (trixie).')
    TOOLS.mkdir(parents=True, exist_ok=True)
    tool_env = dict(os.environ, UV_CACHE_DIR=str(TOOLS / 'uv-cache'))
    python = TOOLS / 'python/bin/python'
    if not python.exists():
        run('uv', 'venv', TOOLS / 'python', env=tool_env)
    run('uv', 'pip', 'install', '--python', python, 'cmake==3.31.6', 'ninja==1.11.1.4', env=tool_env)

    apt = TOOLS / 'apt'
    for subdir in ('empty', 'lists/partial', 'archives/partial'):
        (apt / subdir).mkdir(parents=True, exist_ok=True)
    (apt / 'sources.list').write_text(
        'deb [signed-by=/usr/share/keyrings/debian-archive-keyring.pgp] '
        'https://deb.debian.org/debian trixie main\n')
    config = TOOLS / 'apt.conf'
    config.write_text('\n'.join([
        f'Dir::Etc::Parts "{apt}/empty";',
        f'Dir::Etc::main "{apt}/empty.conf";',
        f'Dir::Etc::sourcelist "{apt}/sources.list";',
        f'Dir::Etc::sourceparts "{apt}/empty";',
        f'Dir::State::lists "{apt}/lists";',
        f'Dir::Cache::archives "{apt}/archives";',
        f'Dir::Cache::pkgcache "{apt}/pkgcache.bin";',
        f'Dir::Cache::srcpkgcache "{apt}/srcpkgcache.bin";',
        'APT::Sandbox::User "agent";', 'Acquire::Languages "none";',
        'Acquire::Retries "2";', 'Debug::NoLocking "true";',
    ]) + '\n')
    apt_env = dict(os.environ, APT_CONFIG=str(config))
    run('/usr/bin/apt-get', 'update', env=apt_env)
    # Download-only: dpkg's installed-package database and /usr are never modified.
    run('/usr/bin/apt-get', '--download-only', '--assume-yes', '--no-install-recommends', 'install',
        'libasound2-dev', 'libfreetype-dev', 'libfontconfig-dev', 'libx11-dev',
        'libxext-dev', 'libxinerama-dev', 'libxrandr-dev', 'libxcursor-dev',
        'libxrender-dev', 'libxcomposite-dev', 'libgl1-mesa-dev', 'xvfb', 'xauth', env=apt_env)
    sysroot = TOOLS / 'sysroot'
    sysroot.mkdir(exist_ok=True)
    for package in sorted((apt / 'archives').glob('*.deb')):
        run('dpkg-deb', '-x', package, sysroot)
    # Development symlinks can refer to runtime libraries already on the machine.
    # Repair only broken links inside our extracted root, using the matching host file.
    for link in (sysroot / 'usr/lib/x86_64-linux-gnu').glob('*.so'):
        if link.is_symlink() and not link.exists():
            host = Path('/usr/lib/x86_64-linux-gnu') / os.readlink(link)
            if host.exists():
                link.unlink()
                link.symlink_to(host.resolve())

    juce = TOOLS / 'JUCE'
    if not juce.exists():
        run('git', 'clone', '--depth', '1', '--branch', '8.0.16-security',
            'https://github.com/juce-framework/JUCE.git', juce)
    actual = subprocess.check_output(['git', '-C', str(juce), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != JUCE_COMMIT:
        raise RuntimeError('Existing JUCE checkout has a different commit; it was left unchanged.')

    build_env = dict(os.environ,
        PKG_CONFIG_SYSROOT_DIR=str(sysroot),
        PKG_CONFIG_LIBDIR=f'{sysroot}/usr/lib/x86_64-linux-gnu/pkgconfig:'
                         f'{sysroot}/usr/share/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig')
    cmake = TOOLS / 'python/bin/cmake'
    build = PROJECT / 'build-linux'
    run(cmake, '-S', PROJECT, '-B', build, '-G', 'Ninja', '-U', 'pkgcfg_lib_*',
        f'-DCMAKE_MAKE_PROGRAM={TOOLS}/python/bin/ninja', '-DCMAKE_BUILD_TYPE=Release',
        f'-DKRAVATEK_JUCE_PATH={juce}', f'-DCMAKE_CXX_FLAGS=-I{sysroot}/usr/include',
        f'-DCMAKE_EXE_LINKER_FLAGS=-L{sysroot}/usr/lib/x86_64-linux-gnu', env=build_env)
    run(cmake, '--build', build, '--parallel', '3', env=build_env)
    run(TOOLS / 'python/bin/ctest', '--test-dir', build, '--output-on-failure', '--parallel', '3')


if __name__ == '__main__':
    main()
