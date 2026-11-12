import librosa
import numpy
import yaml


with open("melConfig.yaml") as f:
    cfg = yaml.safe_load(f)


def fix_length(audio, target):  # sikre at alle lydbider er samme længde
    if len(audio) > target:
        return audio[:target]
    else:
        return numpy.pad(audio, (0, (target - len(audio))))


def mel_transform(audio, cfg):
    fixedAudio = fix_length(audio, cfg["target"])

    mel = librosa.feature.melspectrogram(   # convert til mel
        y=fixedAudio,
        sr=cfg["sample_rate"],
        n_fft=cfg["n_fft"],
        hop_length=cfg["hop_length"],
        n_mels=cfg["n_mels"],
    )

    mel_db = librosa.power_to_db(mel)  # power skala til dB skala

    mel_norm = (mel_db - mel_db.mean()) / (mel_db.std() + 1e-8)

    return mel_norm


if __name__ == "__main__":   # kører kun når filen startes direkte, ikke når den importeres
    audio = numpy.random.randn(15000).astype("float32")
    resultat = mel_transform(audio, cfg)
    print(resultat.shape)