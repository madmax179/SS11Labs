/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file elevenlabs_utils.h
 * @brief Utility helpers for the ElevenLabs UniMRCP TTS plugin.
 * @author Alexey Izosimov
 * @contact izosimov72@gmail.com | linkedin.com/in/izosimov72 | github.com/madmax179
 * @date 2025
 * @license Apache-2.0 — Copyright (c) 2025 Alexey Izosimov.
 */

/* elevenlabs_utils.h */
#ifndef ELEVENLABS_UTILS_H
#define ELEVENLABS_UTILS_H

#include "elevenlabs_synth.h"

/* Audio buffer utilities */
audio_buffer_t* audio_buffer_create(apr_pool_t *pool, apr_size_t capacity);
void audio_buffer_destroy(audio_buffer_t *buffer);
apt_bool_t audio_buffer_write(audio_buffer_t *buffer, const uint8_t *data, apr_size_t size);

#endif