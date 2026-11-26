import json
import random
import sys
from pathlib import Path

import numpy as np
import torch
from torch import nn
from torch.utils.data import DataLoader, TensorDataset

LABELS = "abcdefghijklmnopqrstuvwxyzæøå0123456789"


def load_data(folder, config):
    samples = []
    session_ids = set()

    for session_folder in sorted(folder.glob("session_*")):
        if not session_folder.is_dir():
            continue
        if not (session_folder / "audio.wav").is_file():
            raise ValueError(f"{session_folder}: mangler audio.wav")

        with (session_folder / "info.jsonl").open() as file:
            records = [json.loads(line) for line in file if line.strip()]

        session_info = next((row for row in records if row.get("type") == "session"), None)
        if session_info is None:
            raise ValueError(f"{session_folder}: mangler sessionmetadata i info.jsonl")
        session_id = session_info["session_id"]
        if not isinstance(session_id, str) or not session_id or session_id in session_ids:
            raise ValueError(f"{session_folder}: ugyldigt eller gentaget session_id")
        session_ids.add(session_id)

        keydowns = [row for row in records
                    if row.get("type") == "key" and row.get("event") == "key_down"]
        try:
            files = sorted(
                (session_folder / "spektrogrammer").glob("sample_*.npy"),
                key=lambda file: int(file.stem.split("_")[1]),
            )
        except ValueError as error:
            raise ValueError(f"{session_folder}: ugyldigt sample-nummer") from error

        if len(files) != len(keydowns):
            raise ValueError(
                f"{session_folder}: {len(files)} spektrogrammer, men {len(keydowns)} keydowns"
            )
        numbers = [int(file.stem.split("_")[1]) for file in files]
        if numbers != list(range(1, len(files) + 1)):
            raise ValueError(f"{session_folder}: sample-numrene skal starte ved 1 uden huller")

        for file, keydown in zip(files, keydowns):
            label = keydown["key"].lower()
            if len(label) != 1 or label not in LABELS:
                continue

            mel = np.load(file, allow_pickle=False)
            if mel.dtype != np.float32 or mel.ndim != 2 or not np.isfinite(mel).all():
                raise ValueError(f"{file}: forventede et finite float32 2D-array")
            if mel.shape[0] != config["spectrogram"]["n_mels"]:
                raise ValueError(f"{file}: antal mel-bånd passer ikke med config")

            samples.append((mel, LABELS.index(label), session_id))

    sessions = sorted({session for _, _, session in samples})
    shapes = {mel.shape for mel, _, _ in samples}

    if len(sessions) < 2:
        raise ValueError("Der skal være mindst to forskellige sessioner")
    if len(shapes) != 1 or min(next(iter(shapes))) < 8:
        raise ValueError("Alle arrays skal have samme størrelse, mindst 8 x 8")

    random.Random(config["seed"]).shuffle(sessions)
    fraction = config["validation_fraction"]
    if not 0 < fraction < 1:
        raise ValueError("validation_fraction skal være mellem 0 og 1")
    validation_count = min(len(sessions) - 1, max(1, round(len(sessions) * fraction)))
    train_sessions = sessions[validation_count:]
    validation_sessions = sessions[:validation_count]
    print(f"Validation-sessioner: {validation_sessions}")

    loaders = []
    for index, group in enumerate([train_sessions, validation_sessions]):
        selected = [(mel, label) for mel, label, session in samples if session in group]
        spectrograms = np.stack([mel for mel, _ in selected])
        labels = [label for _, label in selected]

        x = torch.from_numpy(spectrograms).unsqueeze(1)
        y = torch.tensor(labels, dtype=torch.long)
        dataset = TensorDataset(x, y)
        loader = DataLoader(dataset, batch_size=config["batch_size"], shuffle=(index == 0))
        loaders.append(loader)

    return loaders[0], loaders[1], {"train": train_sessions, "validation": validation_sessions}


def make_model():
    layers = []

    for in_channels, out_channels in [(1, 16), (16, 32), (32, 64)]:
        layers.extend([
            nn.Conv2d(in_channels, out_channels, kernel_size=3, padding=1),
            nn.ReLU(),
            nn.MaxPool2d(2),
        ])

    return nn.Sequential(
        *layers,
        nn.AdaptiveAvgPool2d(1),
        nn.Flatten(),
        nn.Linear(64, len(LABELS)),
    )


def run_epoch(model, loader, optimizer=None):
    training = optimizer is not None
    model.train(training)
    total_loss = 0.0
    correct = 0
    sample_count = 0

    with torch.set_grad_enabled(training):
        for x, y in loader:
            logits = model(x)
            loss = nn.functional.cross_entropy(logits, y)

            if training:
                optimizer.zero_grad()
                loss.backward()
                optimizer.step()

            total_loss += loss.item() * len(y)
            correct += (logits.argmax(1) == y).sum().item()
            sample_count += len(y)

    return total_loss / sample_count, correct / sample_count


def main(folder):
    config_path = folder / "config.json"
    if not config_path.is_file():
        config_path = Path(__file__).with_name("config.json")
    config = json.loads(config_path.read_text())
    if config["epochs"] < 1:
        raise ValueError("epochs skal være mindst 1")
    torch.manual_seed(config["seed"])
    torch.set_num_threads(1)

    train_loader, validation_loader, session_split = load_data(folder, config)
    model = make_model()
    optimizer = torch.optim.AdamW(model.parameters(), lr=config["learning_rate"])

    best_loss = float("inf")
    best_accuracy = 0.0
    best_epoch = 0
    checkpoint_path = folder / "best_model.pt"

    for epoch in range(1, config["epochs"] + 1):
        train_loss, train_accuracy = run_epoch(model, train_loader, optimizer)
        val_loss, val_accuracy = run_epoch(model, validation_loader)

        if val_loss < best_loss:
            best_loss = val_loss
            best_accuracy = val_accuracy
            best_epoch = epoch
            torch.save({
                "model_state_dict": model.state_dict(),
                "labels": LABELS,
                "config": config,
                "input_shape": list(train_loader.dataset.tensors[0].shape[1:]),
                "session_split": session_split,
                "epoch": epoch,
                "validation_loss": val_loss,
                "validation_accuracy": val_accuracy,
            }, checkpoint_path)

        print(
            f"Epoch {epoch}: train loss={train_loss:.4f}, accuracy={train_accuracy:.1%} | "
            f"validation loss={val_loss:.4f}, accuracy={val_accuracy:.1%}"
        )

    print(
        f"Laveste validation loss: {best_loss:.4f}, "
        f"accuracy={best_accuracy:.1%}, epoch={best_epoch}"
    )
    print(f"Gemt model: {checkpoint_path}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Brug: python train.py /sti/til/recordings")
    main(Path(sys.argv[1]))
