import json
import random
import sys
from pathlib import Path

import numpy as np
import torch
from torch import nn
from torch.utils.data import DataLoader, TensorDataset

LABELS = "abcdefghijklmnopqrstuvwxyz0123456789"


def load_data(folder, config):
    manifest = folder / "manifest.json"
    # JSON-liste: [{"path": "a.npy", "label": "a", "session_id": "s1"}, ...]
    rows = json.loads(manifest.read_text())
    samples = []

    for row in rows:
        mel = np.load(manifest.parent / row["path"], allow_pickle=False)
        label = row["label"]
        session = row["session_id"]

        if mel.dtype != np.float32 or mel.ndim != 2 or not np.isfinite(mel).all():
            raise ValueError("Hvert sample skal være et finite float32 2D-array")
        if label not in LABELS or len(label) != 1:
            raise ValueError(f"Ukendt label: {label}")
        if not isinstance(session, str) or not session:
            raise ValueError("session_id skal være en ikke-tom streng")

        samples.append((mel, LABELS.index(label), session))

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

    return loaders


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
        nn.Linear(64, 36),
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
    config = json.loads((folder / "config.json").read_text())
    if config["epochs"] < 1:
        raise ValueError("epochs skal være mindst 1")
    torch.manual_seed(config["seed"])
    torch.set_num_threads(1)

    train_loader, validation_loader = load_data(folder, config)
    model = make_model()
    optimizer = torch.optim.AdamW(model.parameters(), lr=config["learning_rate"])

    best_loss = float("inf")
    best_accuracy = 0.0
    best_epoch = 0

    for epoch in range(1, config["epochs"] + 1):
        train_loss, train_accuracy = run_epoch(model, train_loader, optimizer)
        val_loss, val_accuracy = run_epoch(model, validation_loader)

        if val_loss < best_loss:
            best_loss = val_loss
            best_accuracy = val_accuracy
            best_epoch = epoch

        print(
            f"Epoch {epoch}: train loss={train_loss:.4f}, accuracy={train_accuracy:.1%} | "
            f"validation loss={val_loss:.4f}, accuracy={val_accuracy:.1%}"
        )

    print(
        f"Laveste validation loss: {best_loss:.4f}, "
        f"accuracy={best_accuracy:.1%}, epoch={best_epoch}"
    )


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Brug: python train.py /sti/til/mappen")
    main(Path(sys.argv[1]))
