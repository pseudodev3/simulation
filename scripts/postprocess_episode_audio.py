#!/usr/bin/env python3
"""Build the final Mason Block soundtrack.

The renderer intentionally emits a silent picture track. This script creates a quieter,
noise-based neighborhood ambience and uses eSpeak NG for intelligible resident dialogue.
The speech backend is deliberately swappable: later we can replace eSpeak with Piper or
another local TTS without touching the simulation/director.
"""

from __future__ import annotations

import json
import math
import os
import random
import shutil
import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

SAMPLE_RATE = 24_000


def run(command: list[str]) -> None:
    subprocess.run(command, check=True)


def video_duration(path: Path) -> float:
    result = subprocess.run(
        [
            "ffprobe", "-v", "error", "-show_entries", "format=duration",
            "-of", "default=noprint_wrappers=1:nokey=1", str(path),
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    return max(0.1, float(result.stdout.strip()))


def build_segments(plan: dict) -> list[dict]:
    cursor = 0.0
    result: list[dict] = []
    for shot in plan.get("shots", []):
        seconds = max(0.01, float(shot.get("seconds", 0.35)))
        result.append(
            {
                "start": cursor,
                "end": cursor + seconds,
                "minute": int(shot.get("minute", 720)),
                "person_id": int(shot.get("person_id", -1)),
                "secondary_person_id": int(shot.get("secondary_person_id", -1)),
                "dialogue_primary": shot.get("dialogue_primary", "") or "",
                "dialogue_secondary": shot.get("dialogue_secondary", "") or "",
                "caption": shot.get("caption", "") or "",
            }
        )
        cursor += seconds
    return result


def is_night(minute: int) -> bool:
    return minute >= 19 * 60 or minute < 6 * 60


def write_ambient(path: Path, duration: float, seed: int, segments: list[dict]) -> None:
    """Write organic-ish ambience using filtered noise instead of exposed sine tones.

    The previous renderer used pure oscillators for birds, crickets and fake voices. Pure
    oscillators read as UI beeps/squeals. Here the bed is filtered noise with short noisy
    chirps and distant traffic swells, so it behaves more like an environment.
    """

    rng = random.Random(seed ^ 0x71A9C3)
    total_samples = int(math.ceil(duration * SAMPLE_RATE))
    segment_index = 0

    # Sparse events keep the track calm. Times are deterministic for the episode seed.
    bird_events: list[float] = []
    t = 1.5 + rng.random() * 2.5
    while t < duration:
        bird_events.append(t)
        t += 3.8 + rng.random() * 7.5

    traffic_events: list[tuple[float, float]] = []
    t = 5.0 + rng.random() * 6.0
    while t < duration:
        traffic_events.append((t, 2.2 + rng.random() * 2.8))
        t += 13.0 + rng.random() * 24.0

    bird_index = 0
    traffic_index = 0
    wind_lp = 0.0
    traffic_lp = 0.0
    insect_lp = 0.0

    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(SAMPLE_RATE)

        chunk = bytearray()
        for i in range(total_samples):
            now = i / SAMPLE_RATE
            while segment_index + 1 < len(segments) and now >= segments[segment_index]["end"]:
                segment_index += 1
            minute = segments[segment_index]["minute"] if segments else 720
            night = is_night(minute)

            # Low, slowly moving air/room tone.
            white = rng.random() * 2.0 - 1.0
            wind_lp += 0.006 * (white - wind_lp)
            sample = wind_lp * (0.030 if night else 0.024)

            # A very soft second filtered layer keeps silence from feeling digital.
            white2 = rng.random() * 2.0 - 1.0
            traffic_lp += 0.025 * (white2 - traffic_lp)
            sample += traffic_lp * 0.009

            # Distant vehicle pass: low filtered noise with a broad bell envelope.
            while traffic_index + 1 < len(traffic_events) and now > traffic_events[traffic_index][0] + traffic_events[traffic_index][1]:
                traffic_index += 1
            if traffic_events:
                start, span = traffic_events[min(traffic_index, len(traffic_events) - 1)]
                if start <= now <= start + span:
                    x = (now - start) / span
                    envelope = math.sin(math.pi * x) ** 2
                    sample += traffic_lp * envelope * 0.085

            if night:
                # Cricket texture: shaped high-frequency noise, not a single 4 kHz oscillator.
                cycle = (now + (seed % 19) * 0.031) % 1.42
                if cycle < 0.16:
                    insect_noise = rng.random() * 2.0 - 1.0
                    insect_lp += 0.42 * (insect_noise - insect_lp)
                    edge = math.sin(math.pi * cycle / 0.16)
                    sample += (insect_noise - insect_lp) * edge * 0.026
            else:
                # Bird chirps are short, layered sweeps with a little noise in them.
                while bird_index + 1 < len(bird_events) and now > bird_events[bird_index] + 0.20:
                    bird_index += 1
                if bird_events:
                    start = bird_events[min(bird_index, len(bird_events) - 1)]
                    local = now - start
                    if 0.0 <= local <= 0.18:
                        x = local / 0.18
                        envelope = math.sin(math.pi * x) ** 2
                        f = 1150.0 + 980.0 * x + 130.0 * math.sin(x * math.pi * 3.0)
                        phase = 2.0 * math.pi * f * now
                        rough = rng.random() * 2.0 - 1.0
                        sample += (math.sin(phase) * 0.55 + math.sin(phase * 1.47) * 0.22 + rough * 0.23) * envelope * 0.026

            # Very occasional leaf/cloth rustle.
            if rng.random() < 0.000018:
                sample += (rng.random() * 2.0 - 1.0) * 0.05

            sample = max(-0.85, min(0.85, sample))
            value = int(sample * 32767.0)
            chunk.extend(struct.pack("<h", value))
            if len(chunk) >= 131072:
                wav.writeframesraw(chunk)
                chunk.clear()
        if chunk:
            wav.writeframesraw(chunk)


def voice_settings(person_id: int) -> tuple[int, int]:
    # Stable voice identity per resident. This is intentionally subtle rather than cartoonish.
    speed = 150 + (abs(person_id) * 7) % 24
    pitch = 38 + (abs(person_id) * 11) % 30
    return speed, pitch


def synthesize_line(text: str, person_id: int, destination: Path) -> float:
    speed, pitch = voice_settings(person_id)
    raw = destination.with_suffix(".raw.wav")
    run([
        "espeak-ng", "-v", "en-us", "-s", str(speed), "-p", str(pitch),
        "-a", "118", "-w", str(raw), text,
    ])
    run([
        "ffmpeg", "-y", "-loglevel", "error", "-i", str(raw),
        "-ar", str(SAMPLE_RATE), "-ac", "1", "-c:a", "pcm_s16le", str(destination),
    ])
    raw.unlink(missing_ok=True)
    with wave.open(str(destination), "rb") as wav:
        return wav.getnframes() / float(wav.getframerate())


def build_dialogue_clips(segments: list[dict], temp: Path, duration: float) -> tuple[list[tuple[Path, int]], list[str]]:
    clips: list[tuple[Path, int]] = []
    transcript: list[str] = []
    clip_number = 0

    for segment in segments:
        first = segment["dialogue_primary"].strip()
        second = segment["dialogue_secondary"].strip()
        if not first and not second:
            continue

        start = segment["start"] + 0.75
        if first:
            path = temp / f"voice_{clip_number:04d}.wav"
            duration_a = synthesize_line(first, segment["person_id"], path)
            clips.append((path, max(0, int(start * 1000))))
            transcript.append(f"{start:08.2f}s P{segment['person_id']}: {first}")
            clip_number += 1
            start += duration_a + 0.28

        if second and start < min(segment["end"] - 0.15, duration - 0.15):
            path = temp / f"voice_{clip_number:04d}.wav"
            synthesize_line(second, segment["secondary_person_id"], path)
            clips.append((path, max(0, int(start * 1000))))
            transcript.append(f"{start:08.2f}s P{segment['secondary_person_id']}: {second}")
            clip_number += 1

    return clips, transcript


def mix_final(video: Path, ambient: Path, clips: list[tuple[Path, int]], output: Path) -> None:
    command: list[str] = ["ffmpeg", "-y", "-loglevel", "error", "-i", str(video), "-i", str(ambient)]
    for clip, _ in clips:
        command.extend(["-i", str(clip)])

    filters = ["[1:a]volume=0.78[amb]"]
    labels = ["[amb]"]
    for index, (_, delay_ms) in enumerate(clips, start=2):
        label = f"v{index}"
        filters.append(f"[{index}:a]adelay={delay_ms},volume=1.18[{label}]")
        labels.append(f"[{label}]")

    filters.append("".join(labels) + f"amix=inputs={len(labels)}:duration=longest:normalize=0,alimiter=limit=0.92[mix]")
    command.extend([
        "-filter_complex", ";".join(filters),
        "-map", "0:v:0", "-map", "[mix]",
        "-c:v", "copy", "-c:a", "aac", "-b:a", "144k",
        "-shortest", "-movflags", "+faststart", str(output),
    ])
    run(command)


def main() -> int:
    if len(sys.argv) != 4:
        print("usage: postprocess_episode_audio.py episode.json silent.mp4 final.mp4", file=sys.stderr)
        return 2

    plan_path = Path(sys.argv[1])
    video_path = Path(sys.argv[2])
    output_path = Path(sys.argv[3])
    plan = json.loads(plan_path.read_text())
    segments = build_segments(plan)
    duration = video_duration(video_path)
    seed = int(plan.get("seed", 0))

    if shutil.which("ffmpeg") is None or shutil.which("ffprobe") is None:
        raise RuntimeError("ffmpeg/ffprobe are required for episode audio")
    if shutil.which("espeak-ng") is None:
        raise RuntimeError("espeak-ng is required for resident voices")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="mason-audio-") as temp_name:
        temp = Path(temp_name)
        ambient = temp / "neighborhood-ambience.wav"
        write_ambient(ambient, duration, seed, segments)
        clips, transcript = build_dialogue_clips(segments, temp, duration)
        mix_final(video_path, ambient, clips, output_path)

    transcript_path = output_path.parent / "dialogue-transcript.txt"
    transcript_path.write_text(
        "MASON BLOCK DIALOGUE TRACK\n"
        "Synthetic resident voices are generated locally with eSpeak NG.\n"
        "Voice identity is deterministic by resident ID.\n\n"
        + ("\n".join(transcript) if transcript else "No directed conversations occurred in this run.\n")
        + "\n"
    )
    print(f"final soundtrack: {output_path}")
    print(f"spoken lines: {len(clips)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
