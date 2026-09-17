#!/usr/bin/env python3
"""Build the final Mason Block soundtrack with local neural resident voices."""

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

import numpy as np
import soundfile as sf
from kokoro import KPipeline

SAMPLE_RATE = 24_000
VOICE_POOL = [
    "af_heart", "am_adam", "af_bella", "am_michael",
    "af_sarah", "am_echo", "af_nicole", "am_eric",
]
_PIPELINE = None


def run(command: list[str]) -> None:
    subprocess.run(command, check=True)


def video_duration(path: Path) -> float:
    result = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "default=noprint_wrappers=1:nokey=1", str(path)], check=True, capture_output=True, text=True)
    return max(0.1, float(result.stdout.strip()))


def build_segments(plan: dict) -> list[dict]:
    cursor = 0.0
    result = []
    for shot in plan.get("shots", []):
        seconds = max(0.01, float(shot.get("seconds", 0.35)))
        result.append({"start": cursor, "end": cursor + seconds, "minute": int(shot.get("minute", 720)), "person_id": int(shot.get("person_id", -1)), "secondary_person_id": int(shot.get("secondary_person_id", -1)), "dialogue_primary": shot.get("dialogue_primary", "") or "", "dialogue_secondary": shot.get("dialogue_secondary", "") or "", "caption": shot.get("caption", "") or ""})
        cursor += seconds
    return result


def is_night(minute: int) -> bool:
    return minute >= 19 * 60 or minute < 6 * 60


def write_ambient(path: Path, duration: float, seed: int, segments: list[dict]) -> None:
    rng = random.Random(seed ^ 0x71A9C3)
    total_samples = int(math.ceil(duration * SAMPLE_RATE))
    segment_index = 0
    traffic_events = []
    t = 4.0 + rng.random() * 7.0
    while t < duration:
        traffic_events.append((t, 2.5 + rng.random() * 4.0, 0.45 + rng.random() * 0.4)); t += 12.0 + rng.random() * 26.0
    rustle_events = []
    t = 3.0 + rng.random() * 11.0
    while t < duration:
        rustle_events.append((t, 0.35 + rng.random() * 0.75)); t += 18.0 + rng.random() * 31.0
    traffic_index = rustle_index = 0
    air_lp = road_lp = road_lp_slow = rustle_lp = 0.0
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1); wav.setsampwidth(2); wav.setframerate(SAMPLE_RATE)
        chunk = bytearray()
        for i in range(total_samples):
            now = i / SAMPLE_RATE
            while segment_index + 1 < len(segments) and now >= segments[segment_index]["end"]: segment_index += 1
            minute = segments[segment_index]["minute"] if segments else 720
            night = is_night(minute)
            white = rng.random() * 2.0 - 1.0; air_lp += 0.0038 * (white - air_lp); sample = air_lp * (0.018 if night else 0.022)
            road_white = rng.random() * 2.0 - 1.0; road_lp += 0.018 * (road_white - road_lp); road_lp_slow += 0.0025 * (road_lp - road_lp_slow)
            sample += (road_lp_slow * 0.030 + road_lp * 0.006) * (0.60 if night else 1.0)
            while traffic_index + 1 < len(traffic_events) and now > traffic_events[traffic_index][0] + traffic_events[traffic_index][1]: traffic_index += 1
            if traffic_events:
                start, span, strength = traffic_events[min(traffic_index, len(traffic_events)-1)]
                if start <= now <= start + span:
                    envelope = math.sin(math.pi * ((now-start)/span)) ** 2; sample += (road_lp_slow * 0.14 + road_lp * 0.035) * envelope * strength
            while rustle_index + 1 < len(rustle_events) and now > rustle_events[rustle_index][0] + rustle_events[rustle_index][1]: rustle_index += 1
            if rustle_events:
                start, span = rustle_events[min(rustle_index, len(rustle_events)-1)]
                if start <= now <= start + span:
                    edge = math.sin(math.pi * ((now-start)/span)) ** 2; rustle_white = rng.random()*2.0-1.0; rustle_lp += 0.30*(rustle_white-rustle_lp); sample += (rustle_white-rustle_lp)*edge*0.020
            sample = max(-0.80, min(0.80, sample)); chunk.extend(struct.pack("<h", int(sample*32767.0)))
            if len(chunk) >= 131072: wav.writeframesraw(chunk); chunk.clear()
        if chunk: wav.writeframesraw(chunk)


def pipeline():
    global _PIPELINE
    if _PIPELINE is None:
        _PIPELINE = KPipeline(lang_code="a", device="cpu")
    return _PIPELINE


def voice_settings(person_id: int) -> tuple[str, float]:
    pid = abs(person_id)
    voice = VOICE_POOL[pid % len(VOICE_POOL)]
    speeds = [0.92, 0.96, 0.99, 1.02, 1.05]
    return voice, speeds[(pid * 7) % len(speeds)]


def synthesize_line(text: str, person_id: int, destination: Path) -> float:
    """Generate a resident's deterministic neural voice locally with Kokoro."""
    voice, speed = voice_settings(person_id)
    chunks = []
    for _graphemes, _phonemes, audio in pipeline()(text, voice=voice, speed=speed):
        if audio is not None:
            chunks.append(np.asarray(audio, dtype=np.float32))
    if not chunks:
        raise RuntimeError(f"Kokoro returned no audio for resident {person_id} using {voice}")
    audio = np.concatenate(chunks)
    raw = destination.with_suffix(".kokoro.wav")
    sf.write(raw, audio, SAMPLE_RATE, subtype="PCM_16")
    run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(raw), "-af", "highpass=f=85,lowpass=f=7800,acompressor=threshold=0.10:ratio=2.2:attack=12:release=180,volume=0.92", "-ar", str(SAMPLE_RATE), "-ac", "1", "-c:a", "pcm_s16le", str(destination)])
    raw.unlink(missing_ok=True)
    with wave.open(str(destination), "rb") as wav:
        return wav.getnframes() / float(wav.getframerate())


def build_dialogue_clips(segments: list[dict], temp: Path, duration: float):
    clips=[]; transcript=[]; clip_number=0
    for segment in segments:
        first=segment["dialogue_primary"].strip(); second=segment["dialogue_secondary"].strip()
        if not first and not second: continue
        start=segment["start"]+1.05; scene_end=min(segment["end"]-0.35, duration-0.20)
        if first and start < scene_end:
            path=temp/f"voice_{clip_number:04d}.wav"; duration_a=synthesize_line(first,segment["person_id"],path); clips.append((path,max(0,int(start*1000)))); transcript.append(f"{start:08.2f}s P{segment['person_id']}: {first}"); clip_number+=1; start+=duration_a+0.35
        if second and start < scene_end:
            path=temp/f"voice_{clip_number:04d}.wav"; synthesize_line(second,segment["secondary_person_id"],path); clips.append((path,max(0,int(start*1000)))); transcript.append(f"{start:08.2f}s P{segment['secondary_person_id']}: {second}"); clip_number+=1
    return clips, transcript


def mix_final(video: Path, ambient: Path, clips, output: Path) -> None:
    command=["ffmpeg","-y","-loglevel","error","-i",str(video),"-i",str(ambient)]
    for clip,_ in clips: command.extend(["-i",str(clip)])
    if not clips:
        command.extend(["-map","0:v:0","-map","1:a:0","-c:v","copy","-c:a","aac","-b:a","144k","-shortest","-movflags","+faststart",str(output)]); run(command); return
    filters=["[1:a]volume=0.70[amb]"]; labels=[]
    for index,(_,delay_ms) in enumerate(clips,start=2):
        label=f"v{index}"; filters.append(f"[{index}:a]adelay={delay_ms}|{delay_ms},volume=1.0[{label}]"); labels.append(f"[{label}]")
    filters.append("".join(labels)+f"amix=inputs={len(labels)}:duration=longest:normalize=0[speech]")
    filters.append("[amb][speech]sidechaincompress=threshold=0.025:ratio=4:attack=25:release=420[ducked]")
    filters.append("[ducked][speech]amix=inputs=2:duration=longest:normalize=0,alimiter=limit=0.92[mix]")
    command.extend(["-filter_complex",";".join(filters),"-map","0:v:0","-map","[mix]","-c:v","copy","-c:a","aac","-b:a","160k","-shortest","-movflags","+faststart",str(output)]); run(command)


def main() -> int:
    if len(sys.argv)!=4: print("usage: postprocess_episode_audio.py episode.json silent.mp4 final.mp4",file=sys.stderr); return 2
    plan_path=Path(sys.argv[1]); video_path=Path(sys.argv[2]); output_path=Path(sys.argv[3]); plan=json.loads(plan_path.read_text()); segments=build_segments(plan); duration=video_duration(video_path); seed=int(plan.get("seed",0))
    if shutil.which("ffmpeg") is None or shutil.which("ffprobe") is None: raise RuntimeError("ffmpeg/ffprobe are required for episode audio")
    output_path.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="mason-audio-") as temp_name:
        temp=Path(temp_name); ambient=temp/"neighborhood-ambience.wav"; write_ambient(ambient,duration,seed,segments); clips,transcript=build_dialogue_clips(segments,temp,duration); mix_final(video_path,ambient,clips,output_path)
    (output_path.parent/"dialogue-transcript.txt").write_text("MASON BLOCK DIALOGUE TRACK\nNaturalistic overheard resident dialogue generated locally with Kokoro neural TTS.\nVoice identity is deterministic by resident ID.\n\n"+("\n".join(transcript) if transcript else "No directed conversations occurred in this run.\n")+"\n")
    print(f"final soundtrack: {output_path}"); print(f"spoken lines: {len(clips)}"); return 0

if __name__ == "__main__": raise SystemExit(main())
