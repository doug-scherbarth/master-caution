// test/test_alarm_engine/audio_queue_stub.c
//
// Recording stub. Replaces the real audio_queue during host unit tests so
// the alarm engine can be exercised without any audio backend. Tests check
// the recorded fields to assert what the engine called.

#include "audio_queue.h"

int        audio_queue_calls          = 0;
uint8_t    audio_queue_last_wav_id    = 0xFF;
severity_t audio_queue_last_severity  = SEV_NONE;

void audio_queue_init(void) {}

void audio_queue_push(uint8_t wav_id, severity_t sev) {
    audio_queue_calls++;
    audio_queue_last_wav_id   = wav_id;
    audio_queue_last_severity = sev;
}

void audio_queue_tick(uint32_t now_ms) { (void)now_ms; }

void audio_queue_stub_reset(void) {
    audio_queue_calls         = 0;
    audio_queue_last_wav_id   = 0xFF;
    audio_queue_last_severity = SEV_NONE;
}
