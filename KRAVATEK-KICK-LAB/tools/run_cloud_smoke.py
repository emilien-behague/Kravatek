#!/usr/bin/env python3
"""Run the actual JUCE GUI under a private X server and a silent ALSA device."""
import json
import argparse
import os
from pathlib import Path
import select
import struct
import subprocess

PROJECT = Path(__file__).resolve().parents[1]
TOOLS = Path('/workspace/.kravatek-tools')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--offline-audio', action='store_true', help='Exercise the test-only Playback host without a device')
    args = parser.parse_args()
    output = PROJECT / ('out/cloud-smoke-offline' if args.offline_audio else 'out/cloud-smoke')
    output.mkdir(parents=True, exist_ok=True)
    # Never accept reports from a previous run.
    report_path = output / 'gui-smoke.json'
    if report_path.exists():
        report_path.unlink()
    failure_path = output / 'gui-failure.txt'
    if failure_path.exists():
        failure_path.unlink()
    read_fd, write_fd = os.pipe()
    server = None
    try:
        with (output / 'xvfb.log').open('w') as log:
            server = subprocess.Popen([
                str(TOOLS / 'sysroot/usr/bin/Xvfb'), '-displayfd', str(write_fd),
                '-screen', '0', '1440x1024x24', '-nolisten', 'tcp', '-ac',
            ], pass_fds=(write_fd,), stdout=log, stderr=log)
            os.close(write_fd)
            write_fd = -1
            if not select.select([read_fd], [], [], 15)[0]:
                raise RuntimeError('Virtual display did not start; see out/cloud-smoke/xvfb.log')
            display = os.read(read_fd, 64).decode().strip()
            if not display.isdigit():
                raise RuntimeError('Invalid display handshake; see xvfb.log')
            runtime_env = dict(os.environ,
                DISPLAY=f':{display}', ALSA_CONFIG_PATH=str(PROJECT / 'tools/cloud-asound.conf'),
                XDG_CACHE_HOME=str(TOOLS / 'cache'), XDG_CONFIG_HOME=str(TOOLS / 'config'))
            command = [str(PROJECT / 'build-linux/KravaKickLab'), '--smoke-test', str(output)]
            if args.offline_audio:
                command.append('--offline-audio-test')
            result = subprocess.run(command, cwd=PROJECT, env=runtime_env,
                capture_output=True, text=True, timeout=180)
            (output / 'application.log').write_text(result.stdout + result.stderr)
            if result.returncode and failure_path.exists():
                print(failure_path.read_text())
            result.check_returncode()
            report = json.loads(report_path.read_text())
            if report.get('result') != 'passed' or report.get('theme') != 'krava-universal-004':
                raise RuntimeError('GUI test did not pass')
            if (report.get('applicationVersion') != '0.4.1'
                or report.get('styleDropdownActions') != 18
                or report.get('styleSelectionAuditions') != 17
                or not report.get('styleSelectionChangesAudio')
                or not report.get('playingBufferFollowsSelectionAndGeneration')):
                raise RuntimeError('Style selection / playback regression was not verified')
            # A screenshot must be one complete PNG, including on repeated runs.
            # FileOutputStream otherwise appends to an existing screenshot by default.
            png = (output / 'instrument-004.png').read_bytes()
            if not png.startswith(b'\x89PNG\r\n\x1a\n'):
                raise RuntimeError('Screenshot is not PNG')
            offset = 8
            while offset + 12 <= len(png):
                length = struct.unpack('>I', png[offset:offset + 4])[0]
                kind = png[offset + 4:offset + 8]
                offset += length + 12
                if kind == b'IEND':
                    break
            if offset != len(png) or kind != b'IEND':
                raise RuntimeError('Screenshot contains stale or appended image data')
            if args.offline_audio:
                if (report.get('previewAudioBackend') != 'offline-playback-test'
                    or report.get('audioDeviceActive')
                    or report.get('offlinePlaybackBlocks', 0) <= 0):
                    raise RuntimeError('Offline Playback test host did not execute')
            elif (report.get('previewAudioBackend') != 'device'
                or not report.get('audioDeviceActive') or report.get('audioCallbacks', 0) <= 0):
                raise RuntimeError('Silent audio device did not execute real JUCE audio callbacks')
            print(json.dumps(report, indent=2))
            print('GUI, seed, sliders and WAV export verified. Test backend only; physical listening untested.')
    finally:
        if write_fd >= 0:
            os.close(write_fd)
        os.close(read_fd)
        if server is not None:
            server.terminate()
            try:
                server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait()


if __name__ == '__main__':
    main()
