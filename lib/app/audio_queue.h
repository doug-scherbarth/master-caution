// lib/app/audio_queue.h
//
// Priority queue of WAV playback requests.
//
// Semantics:
//   - Plays one WAV at a time (HAL constraint and spec §5.2 — sequential).
//   - When idle, the highest-severity queued entry plays next; ties play
//     in FIFO order (oldest first).
//   - NO PREEMPTION: a higher-severity push during playback is queued and
//     played after the current one finishes. Pilot always hears every
//     annunciation in full.
//   - Duplicate suppression: pushing a wav_id that is already queued OR
//     currently playing is silently dropped. Prevents re-trigger storms
//     from stacking the same sound.
//   - Bounded capacity. Pushes that would overflow are dropped and
//     counted for diagnostic logging.

#ifndef AUDIO_QUEUE_H
#define AUDIO_QUEUE_H

#include "types.h"

void audio_queue_init(void);

// Called by alarm_engine when an alarm transitions INACTIVE → PENDING_ACK.
// No-op if wav_id is already pending or playing, or if the queue is full.
void audio_queue_push(uint8_t wav_id, severity_t severity);

// Cooperative scheduler hook — checks HAL busy state and dispatches.
void audio_queue_tick(uint32_t now_ms);

// Diagnostic counters. Reset only at init.
uint32_t audio_queue_dropped_full_count(void);
uint32_t audio_queue_dropped_dup_count(void);

#endif // AUDIO_QUEUE_H

