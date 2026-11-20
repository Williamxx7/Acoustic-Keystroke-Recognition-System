import librosa
import sys
from melTransformer import mel_transform, cfg   # genbruger din transformer + config


def find_onsets(audio, sr): # Find positionerne (i samples) hvor der skete et klik.
    return librosa.onset.onset_detect(y=audio, sr=sr, units="samples")


def cut_window(audio, onset_sample, cfg): # Klip et vindue omkring et onset
    pre = int(cfg["sample_rate"] * cfg["pre_ms"] / 1000)
    start = max(0, onset_sample - pre)
    end = start + cfg["target"]
    return audio[start:end], start, end


def segment_file(path, cfg): # Find onsets fra en WAV fil og lav et spectrogram for hver onset . retuner en liste med speectrogram og dens start og stop
    audio, sr = librosa.load(path, sr=cfg["sample_rate"])

    onsets = find_onsets(audio, sr)

    segments = []
    for onset in onsets:
        clip, start, end = cut_window(audio, onset, cfg)
        spec = mel_transform(clip, cfg)
        segments.append((spec, start, end))

    return onsets, segments



if __name__ == "__main__":
    # kør:  py segmentering.py sti/til/fil.wav
    # (uden argument bruges test-filen clicks_test.wav)
    path = sys.argv[1] if len(sys.argv) > 1 else "clicks_test.wav"

    onsets, segments = segment_file(path, cfg)

    print(f"Fil: {path}")
    print(f"Antal onsets fundet: {len(onsets)}")
    print(f"Onset-positioner (samples): {onsets}")
    if segments:
        print(f"Form på første spektrogram: {segments[0][0].shape}")
        print(f"Alle spektrogrammer samme form: "f"{len({s.shape for s, _, _ in segments}) == 1}")