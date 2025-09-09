/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file elevenlabs_synth.h
 * @brief Public interfaces and configuration for the ElevenLabs UniMRCP TTS plugin.
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

 #ifndef ELEVENLABS_SYNTH_H
 #define ELEVENLABS_SYNTH_H
 
 #include "mrcp_synth_engine.h"
 #include "apt_consumer_task.h"
 #include "apt_log.h"
 #include "apr_thread_mutex.h"
 #include "apr_thread_cond.h"
 #include "apr_thread_proc.h"
 #include "curl/curl.h"
 
 #define ELEVENLABS_SYNTH_ENGINE_TASK_NAME "ElevenLabs Synth Engine"
 
 /* Plugin identifier for logging */
 #define ELEVENLABS_SYNTH_LOG_SOURCE   elevenlabs_synth_log_source
 #define ELEVENLABS_SYNTH_LOG_SOURCE_TAG "ELEVENLABS_SYNTH"
 #define ELEVENLABS_SYNTH_LOG_MARK ELEVENLABS_SYNTH_LOG_SOURCE, __FILE__, __LINE__
 
 /* Default configuration values */
 #define DEFAULT_MODEL_ID "eleven_multilingual_v2"
 #define DEFAULT_OUTPUT_FORMAT "ulaw_8000"
 #define DEFAULT_CHUNK_MS 20
 #define DEFAULT_CONNECT_TIMEOUT_MS 5000
 #define DEFAULT_READ_TIMEOUT_MS 15000
 #define DEFAULT_FALLBACK_ULAW_TO_PCM TRUE
 #define DEFAULT_OPTIMIZE_STREAMING_LATENCY 0
 #define DEFAULT_CACHE_ENABLED FALSE
 #define DEFAULT_CACHE_DIR "./data/11labs"
 
 /* Audio format constants */
 #define SAMPLE_RATE 8000
 #define CHANNELS 1
 #define BITS_PER_SAMPLE 16
 #define ELEVENLABS_BYTES_PER_SAMPLE (BITS_PER_SAMPLE / 8)
 
 /* ElevenLabs API defaults */
 #define ELEVENLABS_DEFAULT_BASE_URL "https://api.elevenlabs.io/v1/text-to-speech"
 #define ELEVENLABS_API_KEY_HEADER "xi-api-key"
 #define ELEVENLABS_CONTENT_TYPE "application/json"
 
 extern APR_DECLARE_DATA apt_log_source_t* elevenlabs_synth_log_source;

 /* Forward declarations */
 typedef struct elevenlabs_synth_engine_t elevenlabs_synth_engine_t;
 typedef struct elevenlabs_synth_channel_t elevenlabs_synth_channel_t;
 typedef struct elevenlabs_synth_msg_t elevenlabs_synth_msg_t;
 typedef struct elevenlabs_http_client_t elevenlabs_http_client_t;
 typedef struct audio_buffer_t audio_buffer_t;
 
 /* Configuration structure */
 typedef struct {
     char *api_key;
     char *voice_id;
     char *model_id;
     char *output_format;
    char *base_url;                  /* API base URL, e.g. https://api.elevenlabs.io/v1/text-to-speech */
     uint32_t chunk_ms;
     uint32_t connect_timeout_ms;
     uint32_t read_timeout_ms;
     apt_bool_t fallback_ulaw_to_pcm;
    int optimize_streaming_latency; /* 0..4 as per ElevenLabs docs */
    /* Caching */
    apt_bool_t cache_enabled;        /* Enable/disable local audio caching */
    char *cache_dir;                 /* Cache directory path */
 } elevenlabs_config_t;
 
 /* Audio buffer structure for frame accumulation */
 typedef struct audio_buffer_t {
     uint8_t *buffer;
     apr_size_t size;
     apr_size_t capacity;
     apr_thread_mutex_t *mutex;
     apr_pool_t *pool;  /* Add pool for memory allocation */
 } audio_buffer_t;
 
 /* HTTP client structure */
 typedef struct elevenlabs_http_client_t {
     CURL *curl;
     char *url;
     char *post_data;
     audio_buffer_t *audio_buffer;
     apt_bool_t stopped;
     apr_thread_mutex_t *mutex;
     apr_thread_cond_t *cond;
     apr_pool_t *pool;
     const elevenlabs_config_t *config;
     const char *request_voice_id;  /* Voice ID for current request, overrides config */
    apr_thread_t *thread;           /* Background HTTP thread */
    struct curl_slist *headers;     /* HTTP headers for current request */
    apr_time_t start_time;          /* For measuring time-to-first-byte */
    apt_bool_t first_chunk_logged;  /* Whether first-chunk latency was logged */
    /* Caching state */
    apt_bool_t cache_playback_mode; /* If TRUE, read from local cache instead of HTTP */
    char *cache_key;                /* Deterministic cache key */
    char *cache_path_tmp;           /* Temporary path while writing (e.g., .part) */
    char *cache_path_final;         /* Final cache file path (e.g., .wav) */
    apr_file_t *cache_fp;           /* Open file while caching */
    apr_size_t cache_data_bytes;    /* Number of audio payload bytes written (for WAV header) */
 } elevenlabs_http_client_t;
 
 /* ElevenLabs synthesizer engine */
 struct elevenlabs_synth_engine_t {
     apt_consumer_task_t *task;
     elevenlabs_config_t config;
     apr_pool_t *pool;
 };
 
 /* ElevenLabs synthesizer channel */
struct elevenlabs_synth_channel_t {
     /** Back pointer to engine */
     elevenlabs_synth_engine_t *elevenlabs_engine;
     /** Engine channel base */
     mrcp_engine_channel_t *channel;
     
     /** Active (in-progress) speak request */
     mrcp_message_t *speak_request;
     /** Pending stop response */
     mrcp_message_t *stop_response;
     
     /** HTTP client for ElevenLabs API */
     elevenlabs_http_client_t *http_client;
     
     /** Audio buffer for frame accumulation */
     audio_buffer_t *audio_buffer;
     
     /** Frame size in bytes */
     apr_size_t frame_size;
     
    /** Channel-level mutex for state changes */
    apr_thread_mutex_t *mutex;
     
     /** Is synthesis in progress */
     apt_bool_t synthesizing;
     /** Counter for sending IN-PROGRESS events */
     int progress_counter;
}; /* Message types for task communication */
 typedef enum {
     ELEVENLABS_SYNTH_MSG_OPEN_CHANNEL,
     ELEVENLABS_SYNTH_MSG_CLOSE_CHANNEL,
     ELEVENLABS_SYNTH_MSG_REQUEST_PROCESS
 } elevenlabs_synth_msg_type_e;
 
 /* Task message structure */
 struct elevenlabs_synth_msg_t {
     elevenlabs_synth_msg_type_e type;
     mrcp_engine_channel_t *channel;
     mrcp_message_t *request; /* MRCP request message */
 };
 
 /* Engine methods */
 apt_bool_t elevenlabs_synth_engine_destroy(mrcp_engine_t *engine);
 apt_bool_t elevenlabs_synth_engine_open(mrcp_engine_t *engine);
 apt_bool_t elevenlabs_synth_engine_close(mrcp_engine_t *engine);
 mrcp_engine_channel_t* elevenlabs_synth_engine_channel_create(mrcp_engine_t *engine, apr_pool_t *pool);
 
 /* Channel methods */
 apt_bool_t elevenlabs_synth_channel_destroy(mrcp_engine_channel_t *channel);
 apt_bool_t elevenlabs_channel_open(mrcp_engine_channel_t *channel);
 apt_bool_t elevenlabs_channel_close(mrcp_engine_channel_t *channel);
 apt_bool_t elevenlabs_channel_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *request);
 
 /* Stream methods */
 apt_bool_t elevenlabs_synth_stream_destroy(mpf_audio_stream_t *stream);
 apt_bool_t elevenlabs_synth_stream_open(mpf_audio_stream_t *stream, mpf_codec_t *codec);
 apt_bool_t elevenlabs_synth_stream_close(mpf_audio_stream_t *stream);
 apt_bool_t elevenlabs_synth_stream_read(mpf_audio_stream_t *stream, mpf_frame_t *frame);
 
 /* Message processing */
 apt_bool_t elevenlabs_synth_msg_process(apt_task_t *task, apt_task_msg_t *msg);
 
 /* HTTP client utilities */
 elevenlabs_http_client_t* elevenlabs_http_client_create(apr_pool_t *pool);
 apt_bool_t elevenlabs_http_client_stop(elevenlabs_http_client_t *client);
 apt_bool_t elevenlabs_http_client_start_synthesis(elevenlabs_http_client_t *client, 
                                                   const char *text, 
                                                   elevenlabs_synth_channel_t *channel);

 /* Caching helpers (implemented in elevenlabs_http.c) */
 apt_bool_t elevenlabs_cache_compute_key(apr_pool_t *pool,
                                         const char *voice_id,
                                         const char *model_id,
                                         const char *output_format,
                                         const char *text,
                                         char **out_key_hex);
 apt_bool_t elevenlabs_cache_ensure_dir(apr_pool_t *pool, const char *dir);
 
 /* Audio buffer utilities */
 audio_buffer_t* audio_buffer_create(apr_pool_t *pool, apr_size_t capacity);
 apt_bool_t audio_buffer_write(audio_buffer_t *buffer, const uint8_t *data, apr_size_t size);
 void audio_buffer_destroy(audio_buffer_t *buffer);
 
 #endif /* ELEVENLABS_SYNTH_H */
