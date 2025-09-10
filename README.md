<div align="center">

# ElevenLabs TTS Plugin for UniMRCP

Высококачественный  потоковый синтез речи через ElevenLabs API для UniMRCP Server 1.8 (Ubuntu 20.04). Поддерживает низкую задержку, кэширование аудио и гибкую конфигурацию.

</div>

## 📌 Основные возможности

- **Потоковый синтез (HTTP/2)** с быстрым TTFB и IN-PROGRESS keep-alive
- **On-disk кэширование**: повторные запросы (голос+модель+формат+текст) выполняются мгновенно без обращения к API
- **Гибкие форматы**: `pcm_8000`, `ulaw_8000`, `alaw_8000`, `mp3_*` (+ авто WAV-обёртка для G.711/PCM в кэше)
- **Переключение голосов на лету** через MRCP заголовок `Voice-Name`
- **Fallback μ-law/A-law → PCM** для унификации RTP потока (опционально)
- **Потокобезопасный буфер аудио**, фоновые HTTP потоки, минимальные блокировки
- **Логирование TTFB**, прогресса и кэша

## 🔧 Поддерживаемая среда

| Компонент | Версия / Пример |
|-----------|-----------------|
| ОС | Ubuntu 20.04 (focal) |
| UniMRCP | 1.8.0 (dev пакеты) |
| APR / APR-Util | Поставляются с UniMRCP пакетами |
| libcurl | Системная (OpenSSL) |
| Компилятор | gcc (Ubuntu 9/10) |

## 🧱 Установка UniMRCP (Ubuntu 20.04)

Установите сервер и dev-пакеты (важно для заголовков/линковки):
```bash
sudo apt-get update
sudo apt-get install -y \
   unimrcp-client=1:1.8.0-focal \
   unimrcp-client-dev=1:1.8.0-focal \
   unimrcp-server=1:1.8.0-focal \
   unimrcp-server-dev=1:1.8.0-focal
```
Путь установки (по пакетам) обычно: `/opt/unimrcp`.

## 🚀 Быстрый старт (если есть готовый .so)

1. Скопируйте файл `elevenlabs-synth.so` в каталог плагинов:
```bash
sudo install -m 0755 elevenlabs-synth.so /opt/unimrcp/plugin/
```
2. Добавьте блок плагина в `/opt/unimrcp/conf/mrcpengine.xml` (пример ниже).
3. Убедитесь, что в systemd юните добавлен рабочий каталог:
```ini
[Service]
WorkingDirectory=/opt/unimrcp
```
4. Перезапустите сервер:
```bash
sudo systemctl daemon-reload
sudo systemctl restart unimrcp
```
5. Отправьте MRCP SPEAK (через UMC / клиент) — проверьте логи на `Cache hit` при втором запросе.

## 🛠️ Способ 1. Локальная сборка (standalone Makefile)

```bash
sudo apt-get install -y build-essential pkg-config libcurl4-openssl-dev
cd standalone
make UNIMRCP_DIR=/opt/unimrcp -j"$(nproc)"
sudo make UNIMRCP_DIR=/opt/unimrcp install
```
Результат: `/opt/unimrcp/plugin/elevenlabs-synth.so`.

## 🛠️ Способ 2. Сборка (CMake) — опционально

Если будет добавлен полноценный `CMakeLists.txt` с флагом `ELEVENLABS_STANDALONE`:
```bash
mkdir build && cd build
cmake -DELEVENLABS_STANDALONE=ON -DUNIMRCP_DIR=/opt/unimrcp ..
make -j"$(nproc)"
sudo make install
```

## ⚙️ Конфигурация плагина (mrcpengine.xml)

Добавьте в секцию `<plugins>`:
```xml
<plugin id="elevenlabs-synth" name="elevenlabs-synth" enable="true">
   <param name="base_url" value="https://api.elevenlabs.io/v1/text-to-speech"/>
   <param name="api_key" value="YOUR_XI_API_KEY"/>
   <param name="voice_id" value="NFG5qt843uXKj4pFvR7C"/>
   <param name="model_id" value="eleven_multilingual_v2"/>
   <param name="output_format" value="pcm_8000"/>
   <param name="optimize_streaming_latency" value="0"/>
   <param name="chunk_ms" value="20"/>
   <param name="connect_timeout_ms" value="5000"/>
   <param name="read_timeout_ms" value="15000"/>
   <param name="fallback_ulaw_to_pcm" value="true"/>
   <param name="cache_enabled" value="true"/>
   <param name="cache_dir" value="./data/11labs"/>
</plugin>
```

| Параметр | Назначение | По умолчанию | Обяз. |
|----------|------------|--------------|-------|
| api_key | ElevenLabs API Key | — | Да |
| voice_id | Голос по умолчанию | — | Да |
| model_id | Модель TTS | eleven_multilingual_v2 | Нет |
| output_format | Формат аудио | ulaw_8000 | Нет |
| chunk_ms | Размер аудиокадра (мс) | 20 | Нет |
| optimize_streaming_latency | 0..4 (снижение задержки) | 0 | Нет |
| connect_timeout_ms | Таймаут TCP connect | 5000 | Нет |
| read_timeout_ms | Таймаут чтения | 15000 | Нет |
| fallback_ulaw_to_pcm | Декодировать G.711 в PCM | true | Нет |
| cache_enabled | Включить кэш | false | Нет |
| cache_dir | Каталог кэша | ./data/11labs | Нет |

`Voice-Name` в MRCP SPEAK переопределяет `voice_id`.

## 💾 Кэширование

Механизм: SHA1(voice_id + model_id + output_format + text) → имя файла. 

Расширения:
- `pcm_*` → `.wav` (S16LE, добавляется заголовок в конце записи)
- `ulaw_/alaw_` → `.wav` (если fallback=true, фактически PCM)
- `mp3*` → `.mp3`

Поведение:
- Cache hit: читается локальный файл → загружается в буфер → HTTP не вызывается
- Cache miss: поток ElevenLabs → запись `.part` → при успехе WAV header (если надо) → атомарное rename
- Ошибка: `.part` удаляется

Очистка: можно удалять любые файлы в `cache_dir` без остановки сервера.

## 🧵 Потоки и безопасность

- HTTP запрос выполняется в фоне (APR thread)
- Audio buffer защищён mutex; чтение фреймов в MPF потоке
- При cache hit mutex корректно освобождается (фикс включён)

## 🔍 Тестирование (UMC пример)
```bash
echo '<speak>What is your name</speak>' > speak.ssml
umc -r synthesizer -m speak -i speak.ssml -p uni3
```
Повторный вызов → ожидайте `Cache hit:` в логах.

## 🛡️ Systemd юнит (важно)
Добавьте WorkingDirectory, иначе относительные пути (`conf/`, `./data/11labs`) не найдутся:
```ini
[Service]
WorkingDirectory=/opt/unimrcp
ExecStart=/opt/unimrcp/bin/unimrcpserver -r /opt/unimrcp -o 2 -w
```

## 🧪 Диагностика
| Симптом | Причина | Решение |
|---------|---------|---------|
| Missing required parameter: api_key | Нет параметра в mrcpengine.xml | Добавьте param api_key |
| Could not open config file conf/mrcpengine.xml | Неверный cwd/systemd unit | Добавьте WorkingDirectory |
| HTTP 401 | Неверный ключ / нет доступа к голосу | Проверьте ключ, voice_id |
| Нет Cache hit | Выключен кэш или другой текст | Включите cache_enabled / сравните текст |
| Завис после второй сессии (устранено) | Deadlock при cache hit (mutex не освобождался) | Исправлено в текущем коде |

## 📐 Архитектурный поток
```
MRCP SPEAK -> Channel -> (cache? hit -> buffer) | (miss -> HTTP thread -> stream -> buffer)
                                                       buffer -> MPF frame read -> RTP -> клиент
```

## 🔧 Оптимизация размера
Собранный standalone плагин меньше, т.к. динамически линкуется с `unimrcpserver` и не тащит лишние объектные секции. Для ещё меньшего размера можно применить:
```
CFLAGS+=' -ffunction-sections -fdata-sections -fvisibility=hidden'
LDFLAGS+=' -Wl,--gc-sections'
strip --strip-unneeded elevenlabs-synth.so
```

## 📄 Лицензия
Apache-2.0 (см. `LICENSE`).

## 🤝 Контрибьюция / Roadmap
- LRU/TTL для кэша
- Полноценный SSML парсер
- Параллельная защита (single-flight) при одновременном cache miss
- Конфигурируемая политика логирования

---
Если этот плагин полезен — поставьте ⭐ на репозиторий.
