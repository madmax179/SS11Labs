/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file elevenlabs_http.c
 * @brief HTTP client and streaming for the ElevenLabs UniMRCP TTS plugin.
 * @author Alexey Izosimov
 * @contact izosimov72@gmail.com | linkedin.com/in/izosimov72 | github.com/madmax179
 * @date 2025
 * @license Apache-2.0 — Copyright (c) 2025 Alexey Izosimov.
 */

#include "elevenlabs_synth.h"
#include "ulaw_decode.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "apr_sha1.h"
#include "apr_file_io.h"


/* Callback function for libcurl to receive data */
static size_t write_callback(void *contents, size_t size, size_t nmemb,
                             void *userp) {
  elevenlabs_http_client_t *client = (elevenlabs_http_client_t *)userp;
  size_t total_size = size * nmemb;

  if (client->stopped) {
    return 0; /* Stop receiving data */
  }

  if (!client->first_chunk_logged) {
    client->first_chunk_logged = TRUE;
    apr_time_t now = apr_time_now();
    apr_interval_time_t diff_ms = (now - client->start_time) / 1000;
    apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_INFO,
            "TTFB (first audio chunk): %ld ms", (long)diff_ms);
  }

  /* Prepare data for MPF and cache (may convert μ-law -> PCM) */
  const uint8_t *out_ptr = (const uint8_t *)contents;
  apr_size_t out_len = total_size;
  if (client->config && client->config->output_format &&
      strcasecmp(client->config->output_format, "ulaw_8000") == 0 &&
      client->config->fallback_ulaw_to_pcm) {
    /* Convert μ-law to PCM 16-bit */
    int16_t *pcm_buffer = apr_palloc(client->pool, total_size * 2);
    ulaw_to_s16((uint8_t *)contents, total_size, pcm_buffer);
    out_ptr = (const uint8_t*)pcm_buffer;
    out_len = total_size * 2;
  }

  /* Write to audio buffer */
  if (!audio_buffer_write(client->audio_buffer, out_ptr, out_len)) {
    apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_ERROR,
            "Failed to write data to audio buffer");
    return 0;
  }

  /* If caching, write the same data that MPF consumes (so future cache hits need no decode) */
  if (client->cache_fp) {
    apr_size_t to_write = (apr_size_t)out_len;
    apr_file_write(client->cache_fp, out_ptr, &to_write);
    client->cache_data_bytes += to_write;
  }

  apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_DEBUG,
          "Received %zu bytes from ElevenLabs API", total_size);

  return total_size;
}

/* Callback function for libcurl to handle headers */
static size_t header_callback(char *buffer, size_t size, size_t nitems,
                              void *userdata) {
  (void)userdata; // Mark as unused if not needed
  size_t total_size = size * nitems;

  /* Log important headers */
  if (strncmp(buffer, "HTTP/", 5) == 0) {
    apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_INFO,
            "ElevenLabs API response: %.*s", (int)total_size, buffer);
  }

  return total_size;
}
// static size_t header_callback(char *buffer, size_t size, size_t nitems, void
// *userdata)
// {
//     elevenlabs_http_client_t *client = (elevenlabs_http_client_t*)userdata;
//     size_t total_size = size * nitems;

//     /* Log important headers */
//     if (strncmp(buffer, "HTTP/", 5) == 0) {
//         apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_INFO,
//                "ElevenLabs API response: %.*s", (int)total_size, buffer);
//     }

//     return total_size;
// }

/**
 * Create HTTP client for ElevenLabs API
 */
elevenlabs_http_client_t *elevenlabs_http_client_create(apr_pool_t *pool) {
  elevenlabs_http_client_t *client =
      apr_palloc(pool, sizeof(elevenlabs_http_client_t));
  if (!client) {
    return NULL;
  }

  /* Initialize libcurl */
  curl_global_init(CURL_GLOBAL_DEFAULT);

  client->curl = curl_easy_init();
  if (!client->curl) {
    apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_ERROR,
            "Failed to initialize libcurl");
    return NULL;
  }

  client->pool = pool;
  client->stopped = FALSE;
  client->url = NULL;
  client->post_data = NULL;
  client->audio_buffer = NULL;
  client->request_voice_id = NULL;
  client->thread = NULL;
  client->headers = NULL;
  client->first_chunk_logged = FALSE;
  client->start_time = 0;

  /* Create mutex and condition variable for thread safety */
  apr_thread_mutex_create(&client->mutex, APR_THREAD_MUTEX_DEFAULT, pool);
  apr_thread_cond_create(&client->cond, pool);

  /* Set basic curl options */
  curl_easy_setopt(client->curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(client->curl, CURLOPT_WRITEDATA, client);
  curl_easy_setopt(client->curl, CURLOPT_HEADERFUNCTION, header_callback);
  curl_easy_setopt(client->curl, CURLOPT_HEADERDATA, client);
  curl_easy_setopt(client->curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(client->curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(client->curl, CURLOPT_SSL_VERIFYHOST, 2L);
  curl_easy_setopt(client->curl, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);

  return client;
}

/**
 * Destroy HTTP client
 */
void elevenlabs_http_client_destroy(elevenlabs_http_client_t *client) {
  if (client) {
    if (client->thread) {
      apr_status_t rv = APR_SUCCESS;
      apr_thread_join(&rv, client->thread);
      client->thread = NULL;
    }
    if (client->curl) {
      curl_easy_cleanup(client->curl);
      client->curl = NULL;
    }
    if (client->headers) {
      curl_slist_free_all(client->headers);
      client->headers = NULL;
    }

    if (client->mutex) {
      apr_thread_mutex_destroy(client->mutex);
      client->mutex = NULL;
    }

    if (client->cond) {
      apr_thread_cond_destroy(client->cond);
      client->cond = NULL;
    }
  }
}

/**
 * Start text-to-speech synthesis via ElevenLabs API
 */

/* Background thread function to run curl perform */
static void* APR_THREAD_FUNC elevenlabs_http_thread(apr_thread_t *thd, void *data)
{
  elevenlabs_http_client_t *client = (elevenlabs_http_client_t*)data;
  CURLcode res = curl_easy_perform(client->curl);
  long http_code = 0;
  curl_easy_getinfo(client->curl, CURLINFO_RESPONSE_CODE, &http_code);

  if (res != CURLE_OK) {
    if (res == CURLE_OPERATION_TIMEDOUT) {
      apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_ERROR,
              "ElevenLabs API request timed out: %s", curl_easy_strerror(res));
    } else if (res == CURLE_ABORTED_BY_CALLBACK) {
      apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_INFO,
              "ElevenLabs API request was stopped");
    } else {
      apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_ERROR,
              "ElevenLabs API request failed: %s", curl_easy_strerror(res));
    }
  } else if (http_code != 200) {
    apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_ERROR,
            "ElevenLabs API returned HTTP %ld", http_code);
  } else {
    apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_INFO,
            "ElevenLabs API synthesis completed successfully");
  }

  /* Finalize or discard cache file if we were caching */
  if (client->cache_fp) {
  apt_bool_t ok = (res == CURLE_OK && http_code == 200 && client->cache_data_bytes > 0);
    if (ok) {
      /* If WAV wrapper, go back and write header */
      if (client->cache_path_final && strstr(client->cache_path_final, ".wav")) {
        /* Write minimal WAV header: RIFF, fmt, data; supports PCM S16LE and G.711 A/Mu-law */
        apr_off_t pos = 0;
        apr_file_seek(client->cache_fp, APR_SET, &pos);
        /* Determine format and sample rate from output_format */
        int is_pcm = 0, is_ulaw = 0, is_alaw = 0;
        unsigned sr = 8000;
        if (client->config && client->config->output_format) {
          const char *fmt = client->config->output_format;
          if (!strncasecmp(fmt, "pcm_", 4)) is_pcm = 1;
          else if (!strncasecmp(fmt, "ulaw_", 5)) is_ulaw = 1;
          else if (!strncasecmp(fmt, "alaw_", 5)) is_alaw = 1;
          const char *us = strrchr(fmt, '_');
          if (us && *(us+1)) sr = (unsigned)atoi(us+1);
        }
        /* If fallback to PCM is enabled for ulaw/alaw, header must be PCM */
        if (!is_pcm && (is_ulaw || is_alaw) && client->config && client->config->fallback_ulaw_to_pcm) {
          is_pcm = 1; is_ulaw = 0; is_alaw = 0;
        }
        /* WAV fields */
        uint8_t hdr[44];
        memset(hdr, 0, sizeof(hdr));
        /* RIFF */
        memcpy(hdr, "RIFF", 4);
        uint32_t riff_size = 36 + (uint32_t)client->cache_data_bytes;
        memcpy(hdr+4, &riff_size, 4);
        memcpy(hdr+8, "WAVE", 4);
        /* fmt chunk */
        memcpy(hdr+12, "fmt ", 4);
        uint32_t fmt_size = 16; memcpy(hdr+16, &fmt_size, 4);
        uint16_t audio_format = is_pcm ? 1 : (is_ulaw ? 7 : (is_alaw ? 6 : 1));
        memcpy(hdr+20, &audio_format, 2);
        uint16_t num_channels = 1; memcpy(hdr+22, &num_channels, 2);
        uint32_t sample_rate = sr; memcpy(hdr+24, &sample_rate, 4);
        uint16_t bits_per_sample = is_pcm ? 16 : 8;
        uint32_t byte_rate = sample_rate * num_channels * (is_pcm ? 2 : 1);
        uint16_t block_align = num_channels * (is_pcm ? 2 : 1);
        memcpy(hdr+28, &byte_rate, 4);
        memcpy(hdr+32, &block_align, 2);
        memcpy(hdr+34, &bits_per_sample, 2);
        /* data chunk */
        memcpy(hdr+36, "data", 4);
        uint32_t data_size = (uint32_t)client->cache_data_bytes;
        memcpy(hdr+40, &data_size, 4);
        apr_size_t wr = sizeof(hdr);
        apr_file_write(client->cache_fp, hdr, &wr);
      }
      apr_file_close(client->cache_fp);
      client->cache_fp = NULL;
      /* Atomically move .part to final */
      if (client->cache_path_tmp && client->cache_path_final) {
        apr_file_remove(client->cache_path_final, client->pool); /* ignore errors */
        apr_file_rename(client->cache_path_tmp, client->cache_path_final, client->pool);
        apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_INFO, "Cached audio saved: %s", client->cache_path_final);
      }
    } else {
      /* Failure or aborted; do not keep partial cache */
      apr_file_close(client->cache_fp);
      client->cache_fp = NULL;
      if (client->cache_path_tmp) {
        apr_file_remove(client->cache_path_tmp, client->pool);
        apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_INFO, "Discarded partial cache: %s", client->cache_path_tmp);
      }
    }
  }

  /* Mark stopped to allow stream_read to complete when buffer drains */
  apr_thread_mutex_lock(client->mutex);
  client->stopped = TRUE;
  apr_thread_mutex_unlock(client->mutex);
  return NULL;
}

apt_bool_t
elevenlabs_http_client_start_synthesis(elevenlabs_http_client_t *client,
                                       const char *text,
                                       elevenlabs_synth_channel_t *channel) {
  if (!client || !text || !channel) {
    return FALSE;
  }

  apr_thread_mutex_lock(client->mutex);

  /* Reset stopped flag */
  client->stopped = FALSE;

  /* Store config reference */
  const elevenlabs_config_t *config = &channel->elevenlabs_engine->config;
  client->config = config;

  /* Build deterministic cache key and paths when caching enabled */
  client->cache_playback_mode = FALSE;
  client->cache_fp = NULL;
  client->cache_data_bytes = 0;
  client->cache_path_tmp = NULL;
  client->cache_path_final = NULL;
  client->cache_key = NULL;

  const char *voice_id = client->request_voice_id ? client->request_voice_id : config->voice_id;
  if (config->cache_enabled && config->cache_dir) {
    char *key_hex = NULL;
    if (elevenlabs_cache_compute_key(client->pool, voice_id, config->model_id, config->output_format, text, &key_hex)) {
      client->cache_key = key_hex;
      const char *ext = NULL;
      /* Store as WAV when pcm_*, otherwise use mp3 extension if output_format starts with mp3 */
      if (config->output_format && strncasecmp(config->output_format, "pcm_", 4) == 0)
        ext = ".wav";
      else if (config->output_format && strncasecmp(config->output_format, "mp3", 3) == 0)
        ext = ".mp3";
      else if (config->output_format && (strncasecmp(config->output_format, "ulaw_", 5) == 0 || strncasecmp(config->output_format, "alaw_", 5) == 0))
        ext = ".wav"; /* wrap as WAV if later needed */
      else
        ext = ".bin";

      client->cache_path_final = apr_psprintf(client->pool, "%s/%s%s", config->cache_dir, key_hex, ext);
      client->cache_path_tmp   = apr_psprintf(client->pool, "%s/%s%s.part", config->cache_dir, key_hex, ext);

      /* If file exists, switch to cache playback mode by loading into buffer and skipping HTTP entirely */
      apr_finfo_t finfo; memset(&finfo, 0, sizeof(finfo));
    if (apr_stat(&finfo, client->cache_path_final, APR_FINFO_SIZE, client->pool) == APR_SUCCESS && finfo.size > 0) {
        apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_INFO, "Cache hit: %s", client->cache_path_final);
        /* Read file and push to buffer in chunks to avoid huge allocations */
        apr_file_t *fp = NULL;
        if (apr_file_open(&fp, client->cache_path_final, APR_FOPEN_READ | APR_FOPEN_BUFFERED, APR_OS_DEFAULT, client->pool) == APR_SUCCESS) {
          /* If WAV, skip 44-byte header */
          apr_off_t offset = 0;
          if (strstr(client->cache_path_final, ".wav")) {
            offset = 44;
            apr_file_seek(fp, APR_SET, &offset);
          }
          uint8_t chunk[4096];
          apr_size_t rd = sizeof(chunk);
          while (apr_file_read(fp, chunk, &rd) == APR_SUCCESS && rd > 0) {
            audio_buffer_write(client->audio_buffer, chunk, rd);
            rd = sizeof(chunk);
          }
          apr_file_close(fp);
          /* Mark stopped to indicate EOF, and skip HTTP */
          client->stopped = TRUE;
      /* Release mutex before returning from cache-playback path */
      apr_thread_mutex_unlock(client->mutex);
      return TRUE;
        }
      } else {
        /* Ensure cache directory exists */
        apr_status_t rv = apr_dir_make_recursive(config->cache_dir, APR_FPROT_OS_DEFAULT, client->pool);
        if (rv != APR_SUCCESS && !APR_STATUS_IS_EEXIST(rv)) {
          apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_WARNING, "Failed to create cache dir: %s", config->cache_dir);
        }
      }
    }
  }

  /* Build URL */
  /* Use the streaming endpoint for lower latency */
  /* Use the streaming endpoint for lower latency */
  client->url = apr_psprintf(client->pool,
                             "%s/%s/stream?output_format=%s&optimize_streaming_latency=%d",
                             config->base_url, voice_id,
                             config->output_format,
                             config->optimize_streaming_latency);

  /* Build POST data */
  client->post_data =
      apr_psprintf(client->pool, "{\"text\":\"%s\",\"model_id\":\"%s\"}", text,
                   config->model_id);

  apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_INFO,
          "Starting synthesis with URL: %s", client->url);
  apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_DEBUG, "POST data: %s",
          client->post_data);

  /* Set curl options for this request */
  curl_easy_setopt(client->curl, CURLOPT_URL, client->url);
  curl_easy_setopt(client->curl, CURLOPT_POST, 1L);
  curl_easy_setopt(client->curl, CURLOPT_POSTFIELDS, client->post_data);
  curl_easy_setopt(client->curl, CURLOPT_POSTFIELDSIZE,
                   strlen(client->post_data));

  /* Set headers */
  if (client->headers) {
    curl_slist_free_all(client->headers);
    client->headers = NULL;
  }
  client->headers = curl_slist_append(client->headers, "Content-Type: application/json");
  /* Some ElevenLabs setups prefer explicit Accept for binary */
  client->headers = curl_slist_append(client->headers, "Accept: */*");
  client->headers = curl_slist_append(client->headers, apr_psprintf(client->pool, "%s: %s",
                                                    ELEVENLABS_API_KEY_HEADER,
                                                    config->api_key));
  curl_easy_setopt(client->curl, CURLOPT_HTTPHEADER, client->headers);
  /* Disable Expect: 100-continue to avoid extra RTT */
  client->headers = curl_slist_append(client->headers, "Expect:");
  curl_easy_setopt(client->curl, CURLOPT_HTTPHEADER, client->headers);

  /* Set timeouts */
  curl_easy_setopt(client->curl, CURLOPT_CONNECTTIMEOUT_MS,
                   config->connect_timeout_ms);
  curl_easy_setopt(client->curl, CURLOPT_TIMEOUT_MS, config->read_timeout_ms);
  curl_easy_setopt(client->curl, CURLOPT_NOSIGNAL, 1L);

  /* Set buffer size for better streaming performance */
  curl_easy_setopt(client->curl, CURLOPT_BUFFERSIZE, 1024);

  /* Prepare cache file for write-through if enabled and no hit occurred */
  if (config->cache_enabled && client->cache_path_tmp && !client->stopped) {
    if (apr_file_open(&client->cache_fp, client->cache_path_tmp,
                      APR_FOPEN_CREATE | APR_FOPEN_WRITE | APR_FOPEN_TRUNCATE | APR_FOPEN_BUFFERED,
                      APR_OS_DEFAULT, client->pool) == APR_SUCCESS) {
      /* Reserve space for WAV header if we will wrap PCM into WAV */
      if (client->cache_path_final && strstr(client->cache_path_final, ".wav")) {
        /* We'll write header at finalize; for streaming write data immediately after header position */
        apr_off_t pos = 44; /* standard PCM WAV header size */
        apr_file_seek(client->cache_fp, APR_SET, &pos);
      }
    } else {
      apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_WARNING, "Failed to open cache temp file: %s", client->cache_path_tmp);
    }
  }

  /* mark start for latency metrics */
  client->start_time = apr_time_now();
  client->first_chunk_logged = FALSE;

  /* Launch background thread to perform the request */
  apr_status_t rv = apr_thread_create(&client->thread, NULL, elevenlabs_http_thread, client, client->pool);
  apr_thread_mutex_unlock(client->mutex);

  if (rv != APR_SUCCESS) {
    apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_ERROR, "Failed to create HTTP thread");
    return FALSE;
  }
  return TRUE;
}

/* Compute a deterministic cache key (SHA1 hex) over inputs that affect audio */
apt_bool_t elevenlabs_cache_compute_key(apr_pool_t *pool,
                                        const char *voice_id,
                                        const char *model_id,
                                        const char *output_format,
                                        const char *text,
                                        char **out_key_hex)
{
  if (!pool || !voice_id || !model_id || !output_format || !text || !out_key_hex) return FALSE;
  apr_sha1_ctx_t ctx; apr_sha1_init(&ctx);
  apr_sha1_update(&ctx, voice_id, (unsigned int)strlen(voice_id));
  apr_sha1_update(&ctx, model_id, (unsigned int)strlen(model_id));
  apr_sha1_update(&ctx, output_format, (unsigned int)strlen(output_format));
  apr_sha1_update(&ctx, text, (unsigned int)strlen(text));
  unsigned char digest[APR_SHA1_DIGESTSIZE];
  apr_sha1_final(digest, &ctx);
  static const char *hex = "0123456789abcdef";
  char *hexstr = apr_palloc(pool, APR_SHA1_DIGESTSIZE*2 + 1);
  for (int i=0;i<APR_SHA1_DIGESTSIZE;i++){ hexstr[i*2]=hex[(digest[i]>>4)&0xF]; hexstr[i*2+1]=hex[digest[i]&0xF]; }
  hexstr[APR_SHA1_DIGESTSIZE*2] = '\0';
  *out_key_hex = hexstr;
  return TRUE;
}

apt_bool_t elevenlabs_cache_ensure_dir(apr_pool_t *pool, const char *dir)
{
  if (!dir) return FALSE;
  apr_status_t rv = apr_dir_make_recursive(dir, APR_FPROT_OS_DEFAULT, pool);
  return (rv == APR_SUCCESS || APR_STATUS_IS_EEXIST(rv)) ? TRUE : FALSE;
}

// apt_bool_t elevenlabs_http_client_start_synthesis(elevenlabs_http_client_t
// *client,
//                                                  const char *text,
//                                                  elevenlabs_synth_channel_t
//                                                  *channel)
// {
//     if (!client || !text || !config) {
//         return FALSE;
//     }

//     apr_thread_mutex_lock(client->mutex);

//     /* Reset stopped flag */
//     client->stopped = FALSE;

//     /* Store config reference */
//     client->config = &channel->elevenlabs_engine->config;

//     /* Build URL */
//     client->url = apr_psprintf(client->pool, "%s/%s?output_format=%s",
//         ELEVENLABS_API_BASE_URL,
//         client->config->voice_id,
//         client->config->output_format);

//     /* Build POST data */
//     client->post_data = apr_psprintf(client->pool,
//         "{\"text\":\"%s\",\"model_id\":\"%s\"}", text, config->model_id);

//     apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_INFO,
//            "Starting synthesis with URL: %s", client->url);
//     apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_DEBUG,
//            "POST data: %s", client->post_data);

//     /* Set curl options for this request */
//     curl_easy_setopt(client->curl, CURLOPT_URL, client->url);
//     curl_easy_setopt(client->curl, CURLOPT_POST, 1L);
//     curl_easy_setopt(client->curl, CURLOPT_POSTFIELDS, client->post_data);
//     curl_easy_setopt(client->curl, CURLOPT_POSTFIELDSIZE,
//     strlen(client->post_data));

//     /* Set headers */
//     struct curl_slist *headers = NULL;
//     headers = curl_slist_append(headers, "Content-Type: application/json");
//     headers = curl_slist_append(headers, apr_psprintf(client->pool, "%s: %s",
//                                                     ELEVENLABS_API_KEY_HEADER,
//                                                     config->api_key));
//     curl_easy_setopt(client->curl, CURLOPT_HTTPHEADER, headers);

//     /* Set timeouts */
//     curl_easy_setopt(client->curl, CURLOPT_CONNECTTIMEOUT_MS,
//     config->connect_timeout_ms); curl_easy_setopt(client->curl,
//     CURLOPT_TIMEOUT_MS, config->read_timeout_ms);

//     /* Set buffer size for better streaming performance */
//     curl_easy_setopt(client->curl, CURLOPT_BUFFERSIZE, 1024);

//     /* Perform the request */
//     CURLcode res = curl_easy_perform(client->curl);

//     /* Clean up headers */
//     curl_slist_free_all(headers);

//     apr_thread_mutex_unlock(client->mutex);

//     if (res != CURLE_OK) {
//         if (res == CURLE_OPERATION_TIMEDOUT) {
//             apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_ERROR,
//                    "ElevenLabs API request timed out: %s",
//                    curl_easy_strerror(res));
//         } else if (res == CURLE_ABORTED_BY_CALLBACK) {
//             apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_INFO,
//                    "ElevenLabs API request was stopped");
//         } else {
//             apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_ERROR,
//                    "ElevenLabs API request failed: %s",
//                    curl_easy_strerror(res));
//         }
//         return FALSE;
//     }

//     /* Check HTTP response code */
//     long http_code = 0;
//     curl_easy_getinfo(client->curl, CURLINFO_RESPONSE_CODE, &http_code);

//     if (http_code != 200) {
//         apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_ERROR,
//                "ElevenLabs API returned HTTP %ld", http_code);
//         return FALSE;
//     }

//     apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_INFO,
//            "ElevenLabs API synthesis completed successfully");

//     return TRUE;
// }

/**
 * Stop HTTP client and cancel ongoing requests
 */
apt_bool_t elevenlabs_http_client_stop(elevenlabs_http_client_t *client) {
  if (!client) {
    return FALSE;
  }

  apr_thread_mutex_lock(client->mutex);

  client->stopped = TRUE;

  if (client->curl) {
    /* Cancel the request by setting a very short timeout */
    curl_easy_setopt(client->curl, CURLOPT_TIMEOUT_MS, 1);

    /* Alternative: pause the connection */
    curl_easy_pause(client->curl, CURLPAUSE_ALL);
  }
  if (client->headers) {
    curl_slist_free_all(client->headers);
    client->headers = NULL;
  }

  if (client->thread) {
    apr_status_t rv = APR_SUCCESS;
    apr_thread_join(&rv, client->thread);
    client->thread = NULL;
  }

  apr_thread_mutex_unlock(client->mutex);

  apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_INFO,
          "ElevenLabs HTTP client stopped");

  return TRUE;
}
