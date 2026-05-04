#!/usr/bin/env python3
"""Generate alarm WAV files using gTTS for Teensy SD card playback.

Outputs 16-bit PCM 44100 Hz stereo WAV files named 0.WAV–12.WAV.
Copy all .WAV files to the root of a FAT32-formatted microSD card.

Usage:
    python3 tools/gen_alarm_wav.py
"""

import os
import subprocess
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

OUT_DIR = os.path.join(os.path.dirname(__file__), "wav_files")
os.makedirs(OUT_DIR, exist_ok=True)

for wav_id, text in ALARMS:
    mp3_path = os.path.join(OUT_DIR, f"{wav_id}.mp3")
    wav_path = os.path.join(OUT_DIR, f"{wav_id}.WAV")

    print(f"[{wav_id:2d}] {text}")
    gTTS(text=text, lang="en", slow=False).save(mp3_path)

    subprocess.run(
        ["ffmpeg", "-y", "-i", mp3_path,
         "-ar", "44100", "-ac", "2", "-sample_fmt", "s16",
         wav_path],
        check=True, capture_output=True,
    )
    os.remove(mp3_path)
    print(f"      → {wav_path}")

print(f"\nDone. Copy the contents of {OUT_DIR}/ to the root of your SD card.")
