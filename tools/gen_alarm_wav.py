#!/usr/bin/env python3
"""Generate alarm WAV files for Teensy SD card playback.

Alarm voices (0.WAV–12.WAV): gTTS text-to-speech via Google, converted to
16-bit PCM 44100 Hz stereo with ffmpeg.

Startup tones (13.WAV–15.WAV): pure sine waves generated locally (no network),
used by the power-up self-test sequence.

Copy all .WAV files to the root of a FAT32-formatted microSD card.

Usage:
    python3 tools/gen_alarm_wav.py
"""

import math
import os
import struct
import subprocess
import wave
from gtts import gTTS

ALARMS = [
    (0,  "Carbon monoxide"),
    (1,  "Left magneto failure"),
    (2,  "Right magneto failure"),
    (3,  "Oil pressure low"),
    (4,  "Cylinder head temperature high"),
    (5,  "Exhaust gas temperature high"),
    (6,  "Primary alternator failure"),
    (7,  "Secondary alternator failure"),
    (8,  "Pitot heat failure"),
    (9,  "Oil temperature high"),
    (10, "Fuel pressure low"),
    (11, "Flaps overspeed"),
    (12, "Boost pump on"),
]

# Startup self-test tones — IDs must match WAV_TONE_* in startup.c.
STARTUP_TONES = [
    (13, 440.0),    # WAV_TONE_LO
    (14, 880.0),    # WAV_TONE_MID
    (15, 1320.0),   # WAV_TONE_HI
]

OUT_DIR = os.path.join(os.path.dirname(__file__), "wav_files")
os.makedirs(OUT_DIR, exist_ok=True)

# --- Alarm voice files ---------------------------------------------

print("Alarm voices:")
for wav_id, text in ALARMS:
    mp3_path = os.path.join(OUT_DIR, f"{wav_id}.mp3")
    wav_path = os.path.join(OUT_DIR, f"{wav_id}.WAV")

    print(f"  [{wav_id:2d}] {text}")
    gTTS(text=text, lang="en", slow=False).save(mp3_path)

    subprocess.run(
        ["ffmpeg", "-y", "-i", mp3_path,
         "-ar", "44100", "-ac", "2", "-sample_fmt", "s16",
         wav_path],
        check=True, capture_output=True,
    )
    os.remove(mp3_path)

# --- Startup sine tones --------------------------------------------

def write_sine_wav(path, freq_hz, duration_s=0.5, sample_rate=44100, amplitude=0.7):
    n_frames = int(sample_rate * duration_s)
    with wave.open(path, 'w') as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(sample_rate)
        for i in range(n_frames):
            v = int(amplitude * 32767 * math.sin(2 * math.pi * freq_hz * i / sample_rate))
            w.writeframes(struct.pack('<hh', v, v))

print("\nStartup tones:")
for wav_id, freq in STARTUP_TONES:
    wav_path = os.path.join(OUT_DIR, f"{wav_id}.WAV")
    print(f"  [{wav_id:2d}] {freq:.0f} Hz")
    write_sine_wav(wav_path, freq)

print(f"\nDone. Copy the contents of {OUT_DIR}/ to the root of your SD card.")
