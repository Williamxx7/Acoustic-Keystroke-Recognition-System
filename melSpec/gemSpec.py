import sys
from pathlib import Path

import numpy

from Segmentering import segment_file
from melTransformer import cfg


def gem_spektrogrammer(wav_path, out_dir="spektrogrammer"):
    out = Path(out_dir)
    out.mkdir(exist_ok=True)

    onsets, segments = segment_file(wav_path, cfg)

    for i, (spec, start, end) in enumerate(segments, start=1):
        navn = f"sample_{i:04d}.npy"     
        numpy.save(out / navn, spec)

    print(f"Gemte {len(segments)} spektrogrammer i '{out}/'")


if __name__ == "__main__":
    wav = sys.argv[1] if len(sys.argv) > 1 else "clicks_test.wav"
    gem_spektrogrammer(wav)