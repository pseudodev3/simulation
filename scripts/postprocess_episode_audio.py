#!/usr/bin/env python3
"""Build the final Mason Block soundtrack.

The C++ renderer emits a silent picture track. This pass adds two things only:

1. a restrained neighborhood ambience built from filtered noise and distant traffic
2. short neural resident dialogue for directed two-person scenes

There are deliberately no exposed sine-wave birds, crickets or fake oscillator voices.
Those sounded like UI beeps/squeals. Spoken lines use edge-tts so the result is closer to
natural overheard conversation. Each resident keeps a stable voice identity by person ID.
"""

from __future__ import annotations

import json
import math
import random
import shutil
import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

SAMPLE_RATE = 24_000

# Neutral US-English neural voices. Voice assignment is deterministic by resident ID rather
# than inferred from names or demographics.
VOICE_POOL = [
    "en-US-AvaNeural",
    "en-US-AndrewNeural",
    "en-US-EmmaNeural",
    "en-US-BrianNeural",
    "en-US-JennyNeural",
    "en-US-GuyNeural",
]


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
    """Write a calm, non-tonal neighborhood bed.

    Everything is filtered/noise-based so there are no pitched synthetic chirps. The intent
    is presence rather than a sound-effects demo: distant road wash, air, occasional rustle,
    and a softer nighttime floor.
    """

    rng = random.Random(seed ^ 0x71A9C3)
    total_samples = int(math.ceil(duration * SAMPLE_RATE))
    segment_index = 0

    traffic_events: list[tuple[float, float, float]] = []
    t = 4.0 + rng.random() * 7.0
    while t < duration:
        traffic_events.append((t, 2.5 + rng.random() * 4.0, 0.45 + rng.random() * 0.4))
        t += 12.0 + rng.random() * 26.0

    rustle_events: list[tuple[float, float]] = []
    t = 3.0 + rng.random() * 11.0
    while t < duration:
        rustle_events.append((t, 0.35 + rng.random() * 0.75))
        t += 18.0 + rng.random() * 31.0

    traffic_index = 0
    rustle_index = 0
    air_lp = 0.0
    road_lp = 0.0
    road_lp_slow = 0.0
    rustle_lp = 0.0

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

            white = rng.random() * 2.0 - 1.0
            air_lp += 0.0038 * (white - air_lp)
            sample = air_lp * (0.018 if night else 0.022)

            # Distant city/road wash: two low-passed noise bands, quieter at night.
            road_white = rng.random() * 2.0 - 1.0
            road_lp += 0.018 * (road_white - road_lp)
            road_lp_slow += 0.0025 * (road_lp - road_lp_slow)
            sample += (road_lp_slow * 0.030 + road_lp * 0.006) * (0.60 if night else 1.0)

            while traffic_index + 1 < len(traffic_events) and now > traffic_events[traffic_index][0] + traffic_events[traffic_index][1]:
                traffic_index += 1
            if traffic_events:
                start, span, strength = traffic_events[min(traffic_index, len(traffic_events) - 1)]
                if start <= now <= start + span:
                    x = (now - start) / span
                    envelope = math.sin(math.pi * x) ** 2
                    sample += (road_lp_slow * 0.14 + road_lp * 0.035) * envelope * strength

            while rustle_index + 1 < len(rustle_events) and now > rustle_events[rustle_index][0] + rustle_events[rustle_index][1]:
                rustle_index += 1
            if rustle_events:
                start, span = rustle_events[min(rustle_index, len(rustle_events) - 1)]
                if start <= now <= start + span:
                    x = (now - start) / span
                    edge = math.sin(math.pi * x) ** 2
                    rustle_white = rng.random() * 2.0 - 1.0
                    rustle_lp += 0.30 * (rustle_white - rustle_lp)
                    sample += (rustle_white - rustle_lp) * edge * 0.020

            sample = max(-0.80, min(0.80, sample))
            chunk.extend(struct.pack("<h", int(sample * 32767.0)))
            if len(chunk) >= 131072:
                wav.writeframesraw(chunk)
                chunk.clear()
        if chunk:
            wav.writeframesraw(chunk)


def voice_settings(person_id: int) -> tuple[str, str, str]:
    """Stable but subtle neural voice identity for a resident."""
    index = abs(person_id) % len(VOICE_POOL)
    voice = VOICE_POOL[index]
    rate_values = ["-8%", "-5%", "-2%", "+1%", "+3%"]
    pitch_values = ["-3Hz", "-1Hz", "+0Hz", "+1Hz", "+2Hz"]
    rate = rate_values[(abs(person_id) * 7) % len(rate_values)]
    pitch = pitch_values[(abs(person_id) * 11) % len(pitch_values)]
    return voice, rate, pitch


def synthesize_line(text: str, person_id: int, destination: Path) -> float:
    voice, rate, pitch = voice_settings(person_id)
    raw_mp3 = destination.with_suffix(".edge.mp3")

    # edge-tts/argparse treats a separate negative value (for example ``--rate -8%``)
    # as another command-line option. Keep the value attached to the option so both
    # negative and positive resident prosody settings are parsed reliably.
    run([
        "edge-tts",
        "--voice", voice,
        f"--rate={rate}",
        f"--pitch={pitch}",
        "--volume=-6%",
        "--text", text,
        "--write-media", str(raw_mp3),
    ])

    # Gentle EQ/compression keeps close dialogue intelligible without sounding like a
    # narrator in a studio booth. No reverb is added; the neighborhood bed provides space.
    run([
        "ffmpeg", "-y", "-loglevel", "error", "-i", str(raw_mp3),
        "-af", "highpass=f=85,lowpass=f=7800,acompressor=threshold=0.10:ratio=2.2:attack=12:release=180,volume=0.92",
        "-ar", str(SAMPLE_RATE), "-ac", "1", "-c:a", "pcm_s16le", str(destination),
    ])
    raw_mp3.unlink(missing_ok=True)

    with wave.open(str(destination), "rb") as wav:
        return wav.getnframes() / float(wav.getframerate())


def build_dialogue_clips(
    segments: list[dict], temp: Path, duration: float
) -> tuple[list[tuple[Path, int]], list[str]]:
    clips: list[tuple[Path, int]] = []
    transcript: list[str] = []
    clip_number = 0

    for segment in segments:
        first = segment["dialogue_primary"].strip()
        second = segment["dialogue_secondary"].strip()
        if not first and not second:
            continue

        # The camera arrives first, then we overhear the conversation already in progress.
        start = segment["start"] + 1.05
        scene_end = min(segment["end"] - 0.35, duration - 0.20)

        if first and start < scene_end:
            path = temp / f"voice_{clip_number:04d}.wav"
            duration_a = synthesize_line(first, segment["person_id"], path)
            clips.append((path, max(0, int(start * 1000))))
            transcript.append(f"{start:08.2f}s P{segment['person_id']}: {first}")
            clip_number += 1
            start += duration_a + 0.35

        if second and start < scene_end:
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

    if not clips:
        command.extend([
            "-map", "0:v:0", "-map", "1:a:0",
            "-c:v", "copy", "-c:a", "aac", "-b:a", "144k",
            "-shortest", "-movflags", "+faststart", str(output),
        ])
        run(command)
        return

    filters = ["[1:a]volume=0.70[amb]"]
    voice_labels: list[str] = []
    for index, (_, delay_ms) in enumerate(clips, start=2):
        label = f"v{index}"
        filters.append(f"[{index}:a]adelay={delay_ms}|{delay_ms},volume=1.0[{label}]")
        voice_labels.append(f"[{label}]")

    filters.append(
        "".join(voice_labels)
        + f"amix=inputs={len(voice_labels)}:duration=longest:normalize=0[speech]"
    )
    # Dialogue gently ducks the environmental bed, like a camera getting closer to a real
    # conversation, then ambience returns after the exchange.
    filters.append("[amb][speech]sidechaincompress=threshold=0.025:ratio=4:attack=25:release=420[ducked]")
    filters.append("[ducked][speech]amix=inputs=2:duration=longest:normalize=0,alimiter=limit=0.92[mix]")

    command.extend([
        "-filter_complex", ";".join(filters),
        "-map", "0:v:0", "-map", "[mix]",
        "-c:v", "copy", "-c:a", "aac", "-b:a", "160k",
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
    if shutil.which("edge-tts") is None:
        raise RuntimeError("edge-tts is required for natural resident voices")

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
        "Naturalistic overheard resident dialogue generated with neural TTS.\n"
        "Voice identity is deterministic by resident ID.\n\n"
        + ("\n".join(transcript) if transcript else "No directed conversations occurred in this run.\n")
        + "\n"
    )
    print(f"final soundtrack: {output_path}")
    print(f"spoken lines: {len(clips)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
