/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file ulaw_decode.h
 * @brief μ-law (G.711) decoder used by the ElevenLabs UniMRCP TTS plugin.
 * @author Alexey Izosimov
 * @contact izosimov72@gmail.com | linkedin.com/in/izosimov72 | github.com/madmax179
 * @date 2025
 * @license Apache-2.0 — Copyright (c) 2025 Alexey Izosimov.
 */

/*
 * Copyright 2025 ElevenLabs TTS Plugin for UniMRCP
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ULAW_DECODE_H
#define ULAW_DECODE_H

#include <stdint.h>
#include <stddef.h>

/**
 * Convert μ-law encoded audio data to 16-bit PCM
 * 
 * @param in Input μ-law buffer
 * @param n Number of bytes to convert
 * @param out Output 16-bit PCM buffer (must be at least n * 2 bytes)
 */
void ulaw_to_s16(const uint8_t* in, size_t n, int16_t* out);

/**
 * Convert single μ-law byte to 16-bit PCM sample
 * 
 * @param ulaw_byte μ-law encoded byte
 * @return 16-bit PCM sample
 */
int16_t ulaw_byte_to_s16(uint8_t ulaw_byte);

/**
 * Initialize μ-law conversion tables
 * This function should be called once before using the decoder
 */
void ulaw_decode_init(void);

#endif /* ULAW_DECODE_H */
