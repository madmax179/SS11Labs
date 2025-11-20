/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file elevenlabs_synth_channel.c
 * @brief MRCP engine channel for the ElevenLabs UniMRCP TTS plugin.
 * @author Alexey Izosimov
 * @contact izosimov72@gmail.com | linkedin.com/in/izosimov72 | github.com/madmax179
 * @date 2025
 * @license Apache-2.0 — Copyright (c) 2025 Alexey Izosimov.
 */ 

#include "elevenlabs_synth.h"
#include "ulaw_decode.h"
#include <string.h>
#include <apr_thread_proc.h>

/* Forward declarations for static functions */
static apt_bool_t elevenlabs_channel_speak(mrcp_engine_channel_t *channel, 
                                          mrcp_message_t *request, 
                                          mrcp_message_t *response);
static apt_bool_t elevenlabs_channel_stop(mrcp_engine_channel_t *channel, 
                                         mrcp_message_t *request, 
                                         mrcp_message_t *response);
static apt_bool_t elevenlabs_channel_request_dispatch(mrcp_engine_channel_t *channel, 
                                                     mrcp_message_t *request);
static char* elevenlabs_extract_text_from_ssml(const char *ssml, apr_pool_t *pool);
static void elevenlabs_send_speak_complete(mrcp_engine_channel_t *channel, 
                                          mrcp_message_t *request, 
                                          mrcp_synth_completion_cause_e cause);

/* Audio buffer functions */
audio_buffer_t* audio_buffer_create(apr_pool_t *pool, apr_size_t initial_capacity)
{
    /* ElevenLabs API может вернуть большой объем данных, увеличим начальный размер буфера */
    initial_capacity = initial_capacity > (1024 * 1024) ? initial_capacity : (1024 * 1024);
    
    audio_buffer_t *buffer = apr_palloc(pool, sizeof(audio_buffer_t));
    if (!buffer) {
        return NULL;
    }
    
    buffer->buffer = apr_palloc(pool, initial_capacity);
    if (!buffer->buffer) {
        return NULL;
    }
    
    buffer->size = 0;
    buffer->capacity = initial_capacity;
    buffer->pool = pool;  /* Store pool for future allocations */
    
    apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_INFO, 
           "Created audio buffer with initial capacity: %zu bytes", initial_capacity);
    
    apr_thread_mutex_create(&buffer->mutex, APR_THREAD_MUTEX_DEFAULT, pool);
    
    return buffer;
}

void audio_buffer_destroy(audio_buffer_t *buffer)
{
    if (buffer && buffer->mutex) {
        apr_thread_mutex_destroy(buffer->mutex);
    }
}

apt_bool_t audio_buffer_write(audio_buffer_t *buffer, const uint8_t *data, apr_size_t size)
{
    if (!buffer || !data || size == 0) {
        return FALSE;
    }
    
    apr_thread_mutex_lock(buffer->mutex);
    
    /* Check if we need to expand buffer */
    if (buffer->size + size > buffer->capacity) {
        /* Calculate new capacity (double current or what we need, whichever is larger) */
        apr_size_t min_needed = buffer->size + size;
        apr_size_t new_capacity = buffer->capacity * 2;
        if (new_capacity < min_needed) {
            new_capacity = min_needed * 2;
        }

        /* Allocate new buffer */
        uint8_t *new_buffer = apr_palloc(buffer->pool, new_capacity);
        if (!new_buffer) {
            apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_ERROR, 
                   "Failed to allocate memory for expanded audio buffer");
            apr_thread_mutex_unlock(buffer->mutex);
            return FALSE;
        }

        /* Copy existing data */
        if (buffer->size > 0) {
            memcpy(new_buffer, buffer->buffer, buffer->size);
        }
        
        /* Update buffer */
        buffer->buffer = new_buffer;
        buffer->capacity = new_capacity;
        
        apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_DEBUG, 
               "Audio buffer expanded from %zu to %zu bytes", 
               buffer->capacity/2, new_capacity);
    }
    
    /* Copy new data */
    memcpy(buffer->buffer + buffer->size, data, size);
    buffer->size += size;
    
    apr_thread_mutex_unlock(buffer->mutex);
    return TRUE;
}

static apr_size_t audio_buffer_read_frame(audio_buffer_t *buffer, uint8_t *frame, apr_size_t frame_size)
{
    if (!buffer || !frame || frame_size == 0) {
        return 0;
    }
    
    apr_thread_mutex_lock(buffer->mutex);
    
    apr_size_t bytes_to_read = (buffer->size >= frame_size) ? frame_size : buffer->size;
    
    if (bytes_to_read > 0) {
        memcpy(frame, buffer->buffer, bytes_to_read);
        
        /* Move remaining data to beginning of buffer */
        if (bytes_to_read < buffer->size) {
            memmove(buffer->buffer, buffer->buffer + bytes_to_read, buffer->size - bytes_to_read);
        }
        buffer->size -= bytes_to_read;
    }
    
    apr_thread_mutex_unlock(buffer->mutex);
    return bytes_to_read;
}

static void audio_buffer_clear(audio_buffer_t *buffer)
{
    if (!buffer) {
        return;
    }
    
    apr_thread_mutex_lock(buffer->mutex);
    buffer->size = 0;
    apr_thread_mutex_unlock(buffer->mutex);
}

/* Message processing functions */
static apt_bool_t elevenlabs_synth_msg_signal(elevenlabs_synth_msg_type_e type, 
                                             mrcp_engine_channel_t *channel, 
                                             mrcp_message_t *request)
{
    apt_bool_t status = FALSE;
    elevenlabs_synth_channel_t *elevenlabs_channel = channel->method_obj;
    elevenlabs_synth_engine_t *elevenlabs_engine = elevenlabs_channel->elevenlabs_engine;
    apt_task_t *task = apt_consumer_task_base_get(elevenlabs_engine->task);
    apt_task_msg_t *msg = apt_task_msg_get(task);
    
    if (msg) {
        elevenlabs_synth_msg_t *elevenlabs_msg;
        msg->type = TASK_MSG_USER;
        elevenlabs_msg = (elevenlabs_synth_msg_t*) msg->data;
        
        elevenlabs_msg->type = type;
        elevenlabs_msg->channel = channel;
        elevenlabs_msg->request = request;
        status = apt_task_msg_signal(task, msg);
    }
    
    return status;
}

apt_bool_t elevenlabs_synth_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
    elevenlabs_synth_msg_t *elevenlabs_msg = (elevenlabs_synth_msg_t*)msg->data;
    
    switch (elevenlabs_msg->type) {
        case ELEVENLABS_SYNTH_MSG_OPEN_CHANNEL:
            /* Open channel and send async response */
            mrcp_engine_channel_open_respond(elevenlabs_msg->channel, TRUE);
            break;
            
        case ELEVENLABS_SYNTH_MSG_CLOSE_CHANNEL:
            /* Close channel and send async response */
            mrcp_engine_channel_close_respond(elevenlabs_msg->channel);
            break;
            
        case ELEVENLABS_SYNTH_MSG_REQUEST_PROCESS:
            elevenlabs_channel_request_dispatch(elevenlabs_msg->channel, elevenlabs_msg->request);
            break;
            
        default:
            break;
    }
    
    return TRUE;
}

/* Channel method implementations */
apt_bool_t elevenlabs_synth_channel_destroy(mrcp_engine_channel_t *channel)
{
    elevenlabs_synth_channel_t *synth_channel = channel->method_obj;
    
    apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_DEBUG,
           "Destroying synth channel [%p]", (void*)synth_channel);
    
    if (synth_channel) {
        if (synth_channel->http_client) {
            elevenlabs_http_client_stop(synth_channel->http_client);
            /* CRITICAL: Destroy HTTP client to cleanup background thread */
            elevenlabs_http_client_destroy(synth_channel->http_client);
            synth_channel->http_client = NULL;
        }
        
        if (synth_channel->audio_buffer) {
            audio_buffer_clear(synth_channel->audio_buffer);
            audio_buffer_destroy(synth_channel->audio_buffer);
            synth_channel->audio_buffer = NULL;
        }
        
        if (synth_channel->mutex) {
            apr_thread_mutex_destroy(synth_channel->mutex);
            synth_channel->mutex = NULL;
        }
    }
    
    return TRUE;
}

apt_bool_t elevenlabs_channel_open(mrcp_engine_channel_t *channel)
{
    return elevenlabs_synth_msg_signal(ELEVENLABS_SYNTH_MSG_OPEN_CHANNEL, channel, NULL);
}

apt_bool_t elevenlabs_channel_close(mrcp_engine_channel_t *channel)
{
    elevenlabs_synth_channel_t *synth_channel = channel->method_obj;
    
    /* Stop any ongoing synthesis */
    if (synth_channel->synthesizing && synth_channel->http_client) {
        elevenlabs_http_client_stop(synth_channel->http_client);
        synth_channel->synthesizing = FALSE;
    }
    
    return elevenlabs_synth_msg_signal(ELEVENLABS_SYNTH_MSG_CLOSE_CHANNEL, channel, NULL);
}

apt_bool_t elevenlabs_channel_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
    return elevenlabs_synth_msg_signal(ELEVENLABS_SYNTH_MSG_REQUEST_PROCESS, channel, request);
}

/* Request processing implementations */
static apt_bool_t elevenlabs_channel_speak(mrcp_engine_channel_t *channel, 
                                          mrcp_message_t *request, 
                                          mrcp_message_t *response)
{
    elevenlabs_synth_channel_t *synth_channel = channel->method_obj;
    elevenlabs_config_t *config = &synth_channel->elevenlabs_engine->config;
    
    /* Get Voice-Name from request if available */
    char *voice_id = NULL;
    mrcp_synth_header_t *synth_header = mrcp_resource_header_get(request);
    if (synth_header && mrcp_resource_header_property_check(request, SYNTHESIZER_HEADER_VOICE_NAME) == TRUE) {
        voice_id = apr_pstrdup(request->pool, synth_header->voice_param.name.buf);
        apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_DEBUG, 
               "Using voice_id from request: %s", voice_id);
    }
    
    /* If no voice_id in request, use default from config */
    if (!voice_id) {
        voice_id = config->voice_id;
        apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_DEBUG, 
               "Using default voice_id from config: %s", voice_id);
    }
    
    /* Set voice_id in HTTP client for this request */
    if (synth_channel->http_client) {
        synth_channel->http_client->request_voice_id = voice_id;
    }
    
    /* Extract text from request body */
    char *text = NULL;
    if (mrcp_generic_header_property_check(request, GENERIC_HEADER_CONTENT_LENGTH) == TRUE) {
        mrcp_generic_header_t *generic_header = mrcp_generic_header_get(request);
        if (generic_header && generic_header->content_type.buf) {
            /* Check if it's SSML or plain text */
            if (strstr(generic_header->content_type.buf, "application/ssml+xml")) {
                text = elevenlabs_extract_text_from_ssml(request->body.buf, request->pool);
            } else {
                text = apr_pstrdup(request->pool, request->body.buf);
            }
        }
    }
    
    if (!text || strlen(text) == 0) {
        apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_ERROR, 
               "No text content found in SPEAK request");
        response->start_line.status_code = MRCP_STATUS_CODE_METHOD_FAILED;
        return FALSE;
    }
    
    apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_INFO, 
           "Processing SPEAK request [channel=%p, http_client=%p] with text: %s",
           (void*)synth_channel, (void*)synth_channel->http_client, text);
    
    /* Clear audio buffer and reset state */
    audio_buffer_clear(synth_channel->audio_buffer);
    synth_channel->speak_request = request;
    synth_channel->stop_response = NULL;
	synth_channel->progress_counter = 0;

    /* Start synthesis via HTTP client (async thread) */
    if (synth_channel->http_client) {
        apt_bool_t success = elevenlabs_http_client_start_synthesis(
            synth_channel->http_client, text, synth_channel);

        if (!success) {
            apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_ERROR,
                   "Failed to start synthesis");
            response->start_line.status_code = MRCP_STATUS_CODE_METHOD_FAILED;
            mrcp_engine_channel_message_send(channel, response);
            return TRUE;
        }

        /* Set synthesizing flag */
        synth_channel->synthesizing = TRUE;
    }

    /* Send IN-PROGRESS immediately */
    response->start_line.request_state = MRCP_REQUEST_STATE_INPROGRESS;
    mrcp_engine_channel_message_send(channel, response);
    return TRUE;
}

static apt_bool_t elevenlabs_channel_stop(mrcp_engine_channel_t *channel, 
                                         mrcp_message_t *request, 
                                         mrcp_message_t *response)
{
    elevenlabs_synth_channel_t *synth_channel = channel->method_obj;
    
    apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_INFO, 
           "Processing STOP request [channel=%p, http_client=%p]",
           (void*)synth_channel, (void*)synth_channel->http_client);
    
    /* Clear audio buffer immediately */
    if (synth_channel->audio_buffer) {
        audio_buffer_clear(synth_channel->audio_buffer);
    }
    
    /* Stop ongoing synthesis */
    if (synth_channel->synthesizing && synth_channel->http_client) {
        elevenlabs_http_client_stop(synth_channel->http_client);
        synth_channel->synthesizing = FALSE;
        
        apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_INFO, 
               "Synthesis stopped, HTTP client terminated");
    }
    
    /* Send STOP response immediately, don't wait for stream_read */
    response->start_line.request_state = MRCP_REQUEST_STATE_COMPLETE;
    mrcp_engine_channel_message_send(channel, response);
    
    /* If there was an active SPEAK request, send SPEAK-COMPLETE with error cause */
    if (synth_channel->speak_request) {
        elevenlabs_send_speak_complete(channel, synth_channel->speak_request,
                                      SYNTHESIZER_COMPLETION_CAUSE_ERROR);
        synth_channel->speak_request = NULL;
    }
    
    /* Don't store stop_response - we already sent it */
    synth_channel->stop_response = NULL;
    
    return TRUE;
}

static apt_bool_t elevenlabs_channel_request_dispatch(mrcp_engine_channel_t *channel, 
                                                     mrcp_message_t *request)
{
    apt_bool_t processed = FALSE;
    mrcp_message_t *response = mrcp_response_create(request, request->pool);
    
    switch (request->start_line.method_id) {
        case SYNTHESIZER_SPEAK:
            processed = elevenlabs_channel_speak(channel, request, response);
            break;
            
        case SYNTHESIZER_STOP:
            processed = elevenlabs_channel_stop(channel, request, response);
            break;
            
        case SYNTHESIZER_SET_PARAMS:
        case SYNTHESIZER_GET_PARAMS:
        case SYNTHESIZER_PAUSE:
        case SYNTHESIZER_RESUME:
        case SYNTHESIZER_BARGE_IN_OCCURRED:
        case SYNTHESIZER_CONTROL:
        case SYNTHESIZER_DEFINE_LEXICON:
            /* Send async response for unhandled requests */
            mrcp_engine_channel_message_send(channel, response);
            processed = TRUE;
            break;
            
        default:
            break;
    }
    
    if (!processed) {
        /* Send async response for not handled request */
        mrcp_engine_channel_message_send(channel, response);
    }
    
    return TRUE;
}

/* Audio stream method implementations */
apt_bool_t elevenlabs_synth_stream_destroy(mpf_audio_stream_t *stream)
{
    return TRUE;
}

apt_bool_t elevenlabs_synth_stream_open(mpf_audio_stream_t *stream, mpf_codec_t *codec)
{
     /* Nothing extra for now; MPF allocates frame buffer based on negotiated codec.
         We will provide LPCM/8000 frames; server encodes to PCMA/8000 as per SDP. */
     return TRUE;
}

apt_bool_t elevenlabs_synth_stream_close(mpf_audio_stream_t *stream)
{
    return TRUE;
}

apt_bool_t elevenlabs_synth_stream_read(mpf_audio_stream_t *stream, mpf_frame_t *frame)
{
    elevenlabs_synth_channel_t *synth_channel = stream->obj;
    
    /* Check if there is active SPEAK request and synthesis is in progress */
    if (synth_channel->speak_request && synth_channel->synthesizing) {
        apr_size_t bytes_read = audio_buffer_read_frame(
            synth_channel->audio_buffer, 
            frame->codec_frame.buffer, 
            frame->codec_frame.size);
        
        if (bytes_read > 0) {
            frame->type |= MEDIA_FRAME_TYPE_AUDIO;
            apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_DEBUG, 
                   "Sent audio frame: %zu bytes", bytes_read);
			synth_channel->progress_counter = 0; /* Reset counter after sending data */
        } else {
            /* No audio data available, check if synthesis is still in progress */
            if (!synth_channel->http_client->stopped) {
                /* Still synthesizing, return silence and send progress updates */
                memset(frame->codec_frame.buffer, 0, frame->codec_frame.size);
                frame->type |= MEDIA_FRAME_TYPE_AUDIO;
                
                /* Send IN-PROGRESS every ~500ms to keep the session alive */
                synth_channel->progress_counter++;
                if (synth_channel->progress_counter >= 25) { /* 25 frames * 20ms = 500ms */
                    mrcp_message_t *in_progress = mrcp_response_create(synth_channel->speak_request, synth_channel->speak_request->pool);
                    in_progress->start_line.request_state = MRCP_REQUEST_STATE_INPROGRESS;
                    mrcp_engine_channel_message_send(synth_channel->channel, in_progress);
                    apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_DEBUG, 
                               "Sent IN-PROGRESS while waiting for audio data");
                    synth_channel->progress_counter = 0;
                }
            } else {
                /* Synthesis complete (http client stopped and buffer is empty) */
                apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_INFO,
                       "Synthesis complete.");
                
                elevenlabs_send_speak_complete(synth_channel->channel, 
                                             synth_channel->speak_request, 
                                             SYNTHESIZER_COMPLETION_CAUSE_NORMAL);
                synth_channel->speak_request = NULL;
                synth_channel->synthesizing = FALSE;
            }
        }
    }
    
    return TRUE;
}

/* Utility functions */
static char* elevenlabs_extract_text_from_ssml(const char *ssml, apr_pool_t *pool)
{
    if (!ssml || !pool) {
        return NULL;
    }
    
    /* Simple SSML tag removal - in production, use proper XML parser */
    char *text = apr_palloc(pool, strlen(ssml) + 1);
    char *dst = text;
    const char *src = ssml;
    int in_tag = 0;
    
    while (*src) {
        if (*src == '<') {
            in_tag = 1;
        } else if (*src == '>') {
            in_tag = 0;
        } else if (!in_tag) {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
    
    return text;
}

static void elevenlabs_send_speak_complete(mrcp_engine_channel_t *channel, 
                                          mrcp_message_t *request, 
                                          mrcp_synth_completion_cause_e cause)
{
    mrcp_message_t *message = mrcp_event_create(request, SYNTHESIZER_SPEAK_COMPLETE, request->pool);
    if (message) {
        /* Get/allocate synthesizer header */
        mrcp_synth_header_t *synth_header = mrcp_resource_header_prepare(message);
        if (synth_header) {
            /* Set completion cause */
            synth_header->completion_cause = cause;
            mrcp_resource_header_property_add(message, SYNTHESIZER_HEADER_COMPLETION_CAUSE);
        }
        
        /* Set request state */
        message->start_line.request_state = MRCP_REQUEST_STATE_COMPLETE;
        
        /* Send async event */
        mrcp_engine_channel_message_send(channel, message);
        
        apt_log(ELEVENLABS_SYNTH_LOG_MARK, APT_PRIO_INFO, 
               "Sent SPEAK-COMPLETE event with cause %d", cause);
    }
}
