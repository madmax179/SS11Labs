<div align="center">

# ElevenLabs TTS for UniMRCP 1.8 (Ubuntu 20.04)

High‑quality streaming speech synthesis via the ElevenLabs API with low latency, on‑disk cache, and flexible configuration.

</div>

## 📌 Features
- Streaming synthesis (HTTP/2) with fast TTFB (Time To First Byte) and IN-PROGRESS keep-alive
- On-disk cache: repeated requests (voice+model+format+text) — instant, no API call
- Formats: `pcm_8000`, `ulaw_8000`, `alaw_8000`, `mp3_*` (+ auto WAV wrapper for G.711/PCM in cache)
- No-transcoding setup: end-to-end L16/8000 from ElevenLabs to RTP (see “No transcoding”)
- Switch voices on the fly via MRCP `Voice-Name` header
- Fallback μ-law/A-law → PCM (optional) to unify RTP stream
- TTFB, progress, and cache logging; minimal locking

![Plugin's HL diagram](./binary/plugin.drawio2.svg)

## � Binaries

| Version | File |
|--------|------|
| 1.0 | [elevenlabs-synth.so](https://github.com/madmax179/SS11Labs/raw/refs/heads/main/binary/elevenlabs-synth.so) |

## ✅ Requirements

| Component | Version / Example |
|-----------|-------------------|
| OS | Ubuntu 20.04 (focal) |
| UniMRCP | 1.8.0 (dev packages) |
| APR / APR-Util | Provided with UniMRCP packages |
| libcurl | System (OpenSSL) |
| Compiler | gcc (Ubuntu 9/10) |

## 🧱 Install UniMRCP 1.8 via APT (recommended)

Official guide: https://unimrcp.org/manuals/html/DebInstallationManual.html

1) Add credentials for `unimrcp.org` (registration required):
```bash
sudo tee /etc/apt/auth.conf.d/unimrcp.conf >/dev/null <<'EOF'
machine unimrcp.org
 login     <username>
 password  <password>
EOF
```

2) Add repository and key:
```bash
sudo mkdir -p /usr/share/keyrings/
wget -qO - https://unimrcp.org/keys/unimrcp-gpg-key-2025-08-20.gpg | sudo tee /usr/share/keyrings/unimrcp-archive-keyring.gpg >/dev/null
sudo chmod 644 /usr/share/keyrings/unimrcp-archive-keyring.gpg
sudo tee /etc/apt/sources.list.d/unimrcp.list >/dev/null <<'EOF'
deb [arch=amd64 signed-by=/usr/share/keyrings/unimrcp-archive-keyring.gpg] https://unimrcp.org/repo/apt/ focal main unimrcp-1.8
EOF
sudo apt update
```

3) Verify packages are available:
```bash
apt-cache policy unimrcp-client
apt-cache policy unimrcp-server
```

4) Install UniMRCP (into `/opt/unimrcp`):
```bash
sudo apt-get install -y \
   unimrcp-client=1:1.8.0-focal \
   unimrcp-client-dev=1:1.8.0-focal \
   unimrcp-server=1:1.8.0-focal \
   unimrcp-server-dev=1:1.8.0-focal \
   unimrcp-demo-plugins=1:1.8.0-focal
```

## � Plugin installation

### Option A. Prebuilt .so
```bash
sudo install -m 0755 elevenlabs-synth.so /opt/unimrcp/plugin/
```

### Option B. Build from source (standalone/Makefile)
```bash
sudo apt-get install -y build-essential pkg-config cmake autoconf automake libtool \
   libcurl4-openssl-dev libapr1-dev libaprutil1-dev

git clone --single-branch -b main https://github.com/madmax179/SS11Labs.git
cd SS11Labs/standalone
make UNIMRCP_DIR=/opt/unimrcp -j"$(nproc)"
sudo make UNIMRCP_DIR=/opt/unimrcp install
```

Check dependencies (ldd):
```bash
ldd /opt/unimrcp/plugin/elevenlabs-synth.so
```
<details>
<summary>Full sample ldd output (expand)</summary>

```text
linux-vdso.so.1 (0x00007fff9f99f000)
libapr-1.so.0 => /opt/unimrcp/plugin/../lib/libapr-1.so.0 (0x00007ff8db8da000)
libaprutil-1.so.0 => /opt/unimrcp/plugin/../lib/libaprutil-1.so.0 (0x00007ff8db8b1000)
libcurl.so.4 => /lib/x86_64-linux-gnu/libcurl.so.4 (0x00007ff8db816000)
libunimrcpserver.so.0 => /opt/unimrcp/plugin/../lib/libunimrcpserver.so.0 (0x00007ff8db7b5000)
libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007ff8db5c3000)
libuuid.so.1 => /lib/x86_64-linux-gnu/libuuid.so.1 (0x00007ff8db5ba000)
libpthread.so.0 => /lib/x86_64-linux-gnu/libpthread.so.0 (0x00007ff8db595000)
libdl.so.2 => /lib/x86_64-linux-gnu/libdl.so.2 (0x00007ff8db58f000)
libexpat.so.1 => /lib/x86_64-linux-gnu/libexpat.so.1 (0x00007ff8db561000)
libcrypt.so.1 => /lib/x86_64-linux-gnu/libcrypt.so.1 (0x00007ff8db526000)
libnghttp2.so.14 => /lib/x86_64-linux-gnu/libnghttp2.so.14 (0x00007ff8db4fc000)
libidn2.so.0 => /lib/x86_64-linux-gnu/libidn2.so.0 (0x00007ff8db4db000)
librtmp.so.1 => /lib/x86_64-linux-gnu/librtmp.so.1 (0x00007ff8db4b9000)
libssh.so.4 => /lib/x86_64-linux-gnu/libssh.so.4 (0x00007ff8db44a000)
libpsl.so.5 => /lib/x86_64-linux-gnu/libpsl.so.5 (0x00007ff8db437000)
libssl.so.1.1 => /lib/x86_64-linux-gnu/libssl.so.1.1 (0x00007ff8db3a4000)
libcrypto.so.1.1 => /lib/x86_64-linux-gnu/libcrypto.so.1.1 (0x00007ff8db0cd000)
libgssapi_krb5.so.2 => /lib/x86_64-linux-gnu/libgssapi_krb5.so.2 (0x00007ff8db080000)
libldap_r-2.4.so.2 => /lib/x86_64-linux-gnu/libldap_r-2.4.so.2 (0x00007ff8db028000)
liblber-2.4.so.2 => /lib/x86_64-linux-gnu/liblber-2.4.so.2 (0x00007ff8db017000)
libbrotlidec.so.1 => /lib/x86_64-linux-gnu/libbrotlidec.so.1 (0x00007ff8db009000)
libz.so.1 => /lib/x86_64-linux-gnu/libz.so.1 (0x00007ff8dafed000)
libsofia-sip-ua.so.0 => /opt/unimrcp/lib/libsofia-sip-ua.so.0 (0x00007ff8dae68000)
libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007ff8dad19000)
/lib64/ld-linux-x86-64.so.2 (0x00007ff8db91c000)
libunistring.so.2 => /lib/x86_64-linux-gnu/libunistring.so.2 (0x00007ff8dab95000)
libgnutls.so.30 => /lib/x86_64-linux-gnu/libgnutls.so.30 (0x00007ff8da9bf000)
libhogweed.so.5 => /lib/x86_64-linux-gnu/libhogweed.so.5 (0x00007ff8da988000)
libnettle.so.7 => /lib/x86_64-linux-gnu/libnettle.so.7 (0x00007ff8da94e000)
libgmp.so.10 => /lib/x86_64-linux-gnu/libgmp.so.10 (0x00007ff8da8ca000)
libkrb5.so.3 => /lib/x86_64-linux-gnu/libkrb5.so.3 (0x00007ff8da7eb000)
libk5crypto.so.3 => /lib/x86_64-linux-gnu/libk5crypto.so.3 (0x00007ff8da7ba000)
libcom_err.so.2 => /lib/x86_64-linux-gnu/libcom_err.so.2 (0x00007ff8da7b3000)
libkrb5support.so.0 => /lib/x86_64-linux-gnu/libkrb5support.so.0 (0x00007ff8da7a4000)
libresolv.so.2 => /lib/x86_64-linux-gnu/libresolv.so.2 (0x00007ff8da788000)
libsasl2.so.2 => /lib/x86_64-linux-gnu/libsasl2.so.2 (0x00007ff8da76b000)
libgssapi.so.3 => /lib/x86_64-linux-gnu/libgssapi.so.3 (0x00007ff8da724000)
libbrotlicommon.so.1 => /lib/x86_64-linux-gnu/libbrotlicommon.so.1 (0x00007ff8da701000)
libp11-kit.so.0 => /lib/x86_64-linux-gnu/libp11-kit.so.0 (0x00007ff8da5cb000)
libtasn1.so.6 => /lib/x86_64-linux-gnu/libtasn1.so.6 (0x00007ff8da5b5000)
libkeyutils.so.1 => /lib/x86_64-linux-gnu/libkeyutils.so.1 (0x00007ff8da5ae000)
libheimntlm.so.0 => /lib/x86_64-linux-gnu/libheimntlm.so.0 (0x00007ff8da5a0000)
libkrb5.so.26 => /lib/x86_64-linux-gnu/libkrb5.so.26 (0x00007ff8da50d000)
libasn1.so.8 => /lib/x86_64-linux-gnu/libasn1.so.8 (0x00007ff8da467000)
libhcrypto.so.4 => /lib/x86_64-linux-gnu/libhcrypto.so.4 (0x00007ff8da42f000)
libroken.so.18 => /lib/x86_64-linux-gnu/libroken.so.18 (0x00007ff8da416000)
libffi.so.7 => /lib/x86_64-linux-gnu/libffi.so.7 (0x00007ff8da40a000)
libwind.so.0 => /lib/x86_64-linux-gnu/libwind.so.0 (0x00007ff8da3de000)
libheimbase.so.1 => /lib/x86_64-linux-gnu/libheimbase.so.1 (0x00007ff8da3cc000)
libhx509.so.5 => /lib/x86_64-linux-gnu/libhx509.so.5 (0x00007ff8da37e000)
libsqlite3.so.0 => /lib/x86_64-linux-gnu/libsqlite3.so.0 (0x00007ff8da255000)
```

</details>

## ⚙️ Configuration

### 1) Plugin (`/opt/unimrcp/conf/mrcpengine.xml`)
```xml
<plugin id="elevenlabs-synth" name="elevenlabs-synth" enable="true">
   <param name="api_key" value="YOUR_XI_API_KEY"/>
   <param name="voice_id" value="NFG5..."/>
   <param name="model_id" value="eleven_multilingual_v2"/>
   <param name="output_format" value="pcm_8000"/>
   <param name="fallback_ulaw_to_pcm" value="true"/>
   <param name="cache_enabled" value="true"/>
   <param name="cache_dir" value="./data/11labs"/>
   <param name="optimize_streaming_latency" value="0"/>
   <param name="chunk_ms" value="20"/>
   <param name="connect_timeout_ms" value="5000"/>
   <param name="read_timeout_ms" value="15000"/>
</plugin>
```

Important: MRCP SPEAK header `Voice-Name` overrides `voice_id`.

Parameters and values:

| Parameter | Description | Possible values | Default | Req. |
|-----------|-------------|------------------|---------|------|
| api_key | ElevenLabs API Key | string | — | Yes |
| voice_id | Default voice | string (voice UUID) | — | Yes |
| model_id | TTS model | string (e.g., eleven_multilingual_v2) | eleven_multilingual_v2 | No |
| base_url | API base URL | https URL | https://api.elevenlabs.io/v1/text-to-speech | No |
| output_format | Audio format | pcm_8000, ulaw_8000, alaw_8000, mp3_* | ulaw_8000 | No |
| chunk_ms | Frame size, ms | 10..60 (typically 20) | 20 | No |
| optimize_streaming_latency | Lower latency mode | 0..4 | 0 | No |
| connect_timeout_ms | Connect timeout | 1000..30000 | 5000 | No |
| read_timeout_ms | Read timeout | 5000..120000 | 15000 | No |
| fallback_ulaw_to_pcm | Decode G.711 to PCM | true/false | true | No |
| cache_enabled | Enable cache | true/false | false | No |
| cache_dir | Cache directory | path (relative/absolute) | ./data/11labs | No |

### 2) unimrcp.service (working directory is required)

Important: add `WorkingDirectory=/opt/unimrcp` — this is critical for resolving relative config paths and proper library loading via rpath.

Unit file: `/usr/lib/systemd/system/unimrcp.service`

```ini
[Unit]
Description=UniMRCP Server

[Service]
WorkingDirectory=/opt/unimrcp
ExecStart=/opt/unimrcp/bin/unimrcpserver -r /opt/unimrcp -o 2 -w
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

### 3) UniMRCP server (`/opt/unimrcp/conf/unimrcpserver.xml`)

Wire the plugin into the factory and profile:
```xml
<!-- Factory of plugins (MRCP engines) -->
<plugin-factory>
   <engine id="Demo-Synth-1" name="demosynth" enable="false"/>
   <engine id="Demo-Recog-1" name="demorecog" enable="false"/>
   <engine id="Recorder-1" name="mrcprecorder" enable="false"/>
   <engine id="elevenlabs-synth" name="elevenlabs-synth" enable="true"/>
</plugin-factory>

<!-- MRCPv2 default profile -->
<mrcpv2-profile id="uni2">
   <sip-uas>SIP-Agent-1</sip-uas>
   <mrcpv2-uas>MRCPv2-Agent-1</mrcpv2-uas>
   <media-engine>Media-Engine-1</media-engine>
   <rtp-factory>RTP-Factory-1</rtp-factory>
   <rtp-settings>RTP-Settings-1</rtp-settings>

   <resource-engine-map>
      <resource id="speechsynth" engine="elevenlabs-synth"/>
   </resource-engine-map>
</mrcpv2-profile>
```

Default SIP agent:
```xml
<sip-uas id="SIP-Agent-1" type="SofiaSIP">
   <sip-port>8060</sip-port>
   <sip-transport>udp,tcp</sip-transport>
   <ua-name>UniMRCP SofiaSIP</ua-name>
   <sdp-origin>UniMRCPServer</sdp-origin>
</sip-uas>
```

Explicitly add the following lines (these are what you add):

- Plugin engine:
```xml
<engine id="elevenlabs-synth" name="elevenlabs-synth" enable="true"/>
```

- Bind resource to the plugin:
```xml
<resource-engine-map>
  <resource id="speechsynth" engine="elevenlabs-synth"/>
</resource-engine-map>
```

### 4) No transcoding (end-to-end PCM L16/8000)

We recommend client-driven codec order:

- On the client (example FreeSWITCH MRCP) put `L16/96/8000` first:
```xml
<profile name="11labs" version="2">
  <param name="speechsynth" value="elevenlabs-synth"/>
  <param name="codecs" value="L16/96/8000 PCMA PCMU"/>
</profile>
```

- On the server keep client preference:
```xml
<rtp-settings id="RTP-Settings-1">
  <codecs own-preference="false">L16/96/8000 PCMU PCMA telephone-event/101/8000 G722</codecs>
  <ptime>20</ptime>
</rtp-settings>
```

Result: offer/answer negotiates `L16/8000`, and RTP runs without transcoding.

## � Restart and logs
```bash
sudo systemctl daemon-reload
sudo systemctl restart unimrcp
tail -f -n 100 /opt/unimrcp/log/unimrcpserver_current.log
```

## �💾 Cache: how it works and how to manage

### Enable and path
- Enable cache:
   ```xml
   <param name="cache_enabled" value="true"/>
   <param name="cache_dir" value="./data/11labs"/>
   ```
- `cache_dir` may be relative. It resolves from the process working directory. Therefore in the systemd unit set:
   ```ini
   [Service]
   WorkingDirectory=/opt/unimrcp
   ```
- You can use an absolute path (e.g., `/opt/unimrcp/data/11labs`) so the cache does not depend on `WorkingDirectory`.

### Key and files
- Key: `SHA1(voice_id + model_id + output_format + text)` — deterministic naming for repeatability.
- Artifacts:
   - `pcm_*` → `<key>.wav` (PCM16 S16LE; WAV header appended after download completes)
   - `ulaw_*`/`alaw_*` → `<key>.wav` (G.711 or PCM if `fallback_ulaw_to_pcm=true`)
   - `mp3_*` → `<key>.mp3` (raw mp3)
- Atomicity: write to `<key>.*.part` then `rename()` to the final name. On failure `.part` is removed.

### Processing flow (simplified)
1) SPEAK → build key → check for the artifact.
2) Cache hit → read from disk (for WAV, skip header if needed in MPF) → fill buffer → RTP.
3) Cache miss → background HTTP stream from ElevenLabs → write to buffer and `.part` → finalize/patch WAV header (PCM/G.711) → atomic `rename` → RTP.

### Cache management
- Check cache size:
   ```bash
   du -sh /opt/unimrcp/data/11labs
   ```
- List recent files:
   ```bash
   ls -lt /opt/unimrcp/data/11labs | head -20
   ```
- Remove a single entry by key (example):
   ```bash
   rm -f /opt/unimrcp/data/11labs/0123abcd*.{wav,mp3,part}
   ```
- Wipe cache (careful!):
   ```bash
   rm -f /opt/unimrcp/data/11labs/*
   ```
- Delete older than N days (example 7 days):
   ```bash
   find /opt/unimrcp/data/11labs -type f -mtime +7 -delete
   ```

### Tips
- For end-to-end RTP without transcoding use `output_format=pcm_8000` and prefer L16/8000 in codec lists (see “No transcoding”).
- There is no TTL/LRU yet — plan periodic cleanup if disk space is limited.
- Normalize text (whitespace/case) to improve cache hit ratio.

## � Troubleshooting (short)
| Symptom | Cause | Resolution |
|---------|-------|------------|
| Could not open config file ... | Missing WorkingDirectory | Add WorkingDirectory=/opt/unimrcp |
| HTTP 401 | Key/access | Verify `api_key`, `voice_id` |
| No Cache hit | Text/format differ | Enable `cache_enabled`, normalize text |

## 🧯 Troubleshooting (mini)

Real log signatures in `/opt/unimrcp/log/unimrcpserver_current.log`:
```text
[elevenlabs] Configuration loaded (voice_id=NFG5..., format=pcm_8000)
[elevenlabs] Cache directory ready: /opt/unimrcp/data/11labs
[elevenlabs] Starting synthesis with URL: https://api.elevenlabs.io/v1/text-to-speech/...
[elevenlabs] TTFB: 143 ms (first audio chunk)
[elevenlabs] Cache hit: /opt/unimrcp/data/11labs/7f9c...c1.wav
[elevenlabs] Cached audio saved: /opt/unimrcp/data/11labs/7f9c...c1.wav (8000Hz)
[elevenlabs] Discarded partial cache: /opt/unimrcp/data/11labs/7f9c...c1.wav.part
[elevenlabs] Synthesis complete.
```

Quick checks:
- Config not found: ensure systemd working directory (`WorkingDirectory=/opt/unimrcp`).
- 401/403: verify `api_key` and access to the selected `voice_id`.
- Long TTFB: increase `optimize_streaming_latency` (1–3) and check network.
- No `Cache hit`: enable `cache_enabled`, ensure `voice_id/model_id/output_format/text` are identical.

Note: TTFB (Time To First Byte) is the time to the first byte/first audio chunk received from ElevenLabs after the request.

## � Backlog
- LRU/TTL cache policy
- More formats/G.711 passthrough
- Better logging and metrics
- Improved SSML parsing

## 📄 License
Apache-2.0 (see `LICENSE`).

---
If you find this plugin useful — a ⭐ for the repository is appreciated.
