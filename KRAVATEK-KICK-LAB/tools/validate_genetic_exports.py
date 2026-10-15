#!/usr/bin/env python3
"""Independent PCM24 reader and exact saved-sound recall check (standard library)."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess
import wave


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--renderer', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    renderer, output = args.renderer.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    records = []
    for rate in (44100, 48000):
        for mode in ('deep', 'hard-psy', 'schranz', 'experimental', 'mutant'):
            path = output / f'{mode}-{rate}.wav'
            subprocess.run([str(renderer), '--mode', mode, '--seed', '123456789',
                            '--novelty', '.75', '--sample-rate', str(rate),
                            '--output', str(path)], check=True)
            with wave.open(str(path), 'rb') as wav:
                assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (1, 3, rate)
                frames = wav.getnframes()
                raw = wav.readframes(frames)
                assert len(raw) == frames * 3 and frames > rate * .04
            samples = [int.from_bytes(raw[i:i + 3], 'little', signed=True) / 8388608
                       for i in range(0, len(raw), 3)]
            peak = max(abs(x) for x in samples)
            rms = math.sqrt(sum(x * x for x in samples) / frames)
            dc = sum(samples) / frames
            assert all(math.isfinite(x) for x in samples)
            assert 0 < peak <= 10 ** (-1 / 20) + 1 / 8388608
            assert abs(dc) < 2 / 8388608
            assert samples[0] == samples[-1] == 0
            recalled = output / 'recalled.wav'
            subprocess.run([str(renderer), '--input', str(path.with_suffix('.krava.json')),
                            '--output', str(recalled)], check=True)
            assert recalled.read_bytes() == path.read_bytes(), 'Saved sound changed WAV bytes'
            records.append(dict(mode=mode, rate=rate, frames=frames,
                                peakDb=20 * math.log10(peak), rmsDb=20 * math.log10(rms),
                                dc=dc, exactRecall=True,
                                sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    (output / 'validation.json').write_text(json.dumps(records, indent=2) + '\n')
    print(f'PASS: {len(records)} PCM24 exports and byte-identical JSON recalls')


if __name__ == '__main__':
    main()
