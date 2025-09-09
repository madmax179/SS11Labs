/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file ulaw_decode.c
 * @brief μ-law (G.711) decoder for the ElevenLabs UniMRCP TTS plugin.
 * @author Alexey Izosimov
 * @contact izosimov72@gmail.com | linkedin.com/in/izosimov72 | github.com/madmax179
 * @date 2025
 * @license Apache-2.0 — Copyright (c) 2025 Alexey Izosimov.
 */

#include "ulaw_decode.h"
#include <string.h>

/* μ-law to 16-bit PCM conversion table */
static int16_t ulaw_table[256];
static int ulaw_table_initialized = 0;

/* μ-law decoding algorithm constants */
#define ULAW_BIAS 0x84
#define ULAW_CLIP 32635

/**
 * Initialize μ-law conversion table
 */
void ulaw_decode_init(void)
{
    if (ulaw_table_initialized) {
        return;
    }
    
    int i;
    for (i = 0; i < 256; i++) {
        int8_t ulaw_val = (int8_t)i;
        int16_t pcm_val;
        
        /* Complement to obtain normal u-law value */
        ulaw_val = ~ulaw_val;
        
        /* Extract and negate the sign bit */
        int sign = (ulaw_val & 0x80) ? -1 : 1;
        
        /* Extract the exponent */
        int exponent = (ulaw_val >> 4) & 0x07;
        
        /* Extract the mantissa */
        int mantissa = ulaw_val & 0x0F;
        
        /* Add the bias and left shift the mantissa */
        mantissa = (mantissa << 3) + ULAW_BIAS;
        
        /* Shift based on the exponent */
        if (exponent != 0) {
            mantissa = mantissa + (1 << (exponent + 3));
        }
        
        /* Apply the sign */
        pcm_val = (int16_t)(sign * mantissa);
        
        /* Clamp to 16-bit range */
        if (pcm_val > ULAW_CLIP) {
            pcm_val = ULAW_CLIP;
        } else if (pcm_val < -ULAW_CLIP) {
            pcm_val = -ULAW_CLIP;
        }
        
        ulaw_table[i] = pcm_val;
    }
    
    ulaw_table_initialized = 1;
}

/**
 * Convert single μ-law byte to 16-bit PCM sample
 */
int16_t ulaw_byte_to_s16(uint8_t ulaw_byte)
{
    if (!ulaw_table_initialized) {
        ulaw_decode_init();
    }
    
    return ulaw_table[ulaw_byte];
}

/**
 * Convert μ-law encoded audio data to 16-bit PCM
 */
void ulaw_to_s16(const uint8_t* in, size_t n, int16_t* out)
{
    if (!ulaw_table_initialized) {
        ulaw_decode_init();
    }
    
    size_t i;
    for (i = 0; i < n; i++) {
        out[i] = ulaw_table[in[i]];
    }
}
