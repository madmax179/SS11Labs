# ElevenLabs TTS Plugin for UniMRCP

Плагин синтезатора речи для UniMRCP сервера, использующий API ElevenLabs для высококачественного преобразования текста в речь.

## Возможности

- **Потоковый синтез**: Поддержка streaming HTTP для получения аудио в реальном времени
- **Множественные форматы**: Поддержка PCM 8000 Hz и μ-law 8000 Hz с автоматической конвертацией
- **Гибкая настройка**: Настраиваемые параметры через XML конфигурацию
- **MRCP совместимость**: Полная поддержка MRCP протокола (SPEAK, STOP, SPEAK-COMPLETE)
- **Потокобезопасность**: Многопоточная архитектура с защищенными буферами

## Требования

- UniMRCP установлен (headers/libs доступны), например в `/opt/unimrcp`
- libcurl 7.x
- APR 1.x и APR-Util 1.x
- GCC (Ubuntu 20.04) / Clang
- CMake ≥ 3.10 (для cmake-варианта) или GNU Make (для standalone Makefile)

## Установка зависимостей

### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install libcurl4-openssl-dev
```

### CentOS/RHEL
```bash
sudo yum install libcurl-devel
```

### macOS
```bash
brew install curl
```

## Вариант A: Standalone сборка (Ubuntu 20.04) — только плагин

Подразумевается, что UniMRCP уже установлен (например, в `/opt/unimrcp`).

1) Установите зависимости:
```bash
sudo apt-get update
sudo apt-get install -y build-essential pkg-config libcurl4-openssl-dev libapr1-dev libaprutil1-dev
```

2) Сборка и установка плагина:
```bash
cd plugins/elevenlabs-synth/standalone
make UNIMRCP_DIR=/opt/unimrcp -j$(nproc)
sudo make UNIMRCP_DIR=/opt/unimrcp install
```

Результат: `elevenlabs-synth.so` установлен в `/opt/unimrcp/plugin/`.

Примечания:
- Если UniMRCP установлен в другом префиксе, укажите `UNIMRCP_DIR=/ваш/путь`.
- Линковка производится против системных APR/APU/libcurl, символы UniMRCP будут разрешены сервером при загрузке.

## Вариант B: CMake standalone сборка

1) Установите зависимости (см. выше).

2) Сборка через CMake:
```bash
cd plugins/elevenlabs-synth
mkdir -p build && cd build
cmake -DELEVENLABS_STANDALONE=ON -DUNIMRCP_DIR=/opt/unimrcp ..
make -j$(nproc)
```

3) Установка:
```bash
sudo make install
```

Плагин установится в `/opt/unimrcp/plugin/` по умолчанию (можно переопределить `CMAKE_INSTALL_PREFIX`).

## Конфигурация

### 1. Добавление в mrcpengine.xml

Добавьте следующую секцию в файл `conf/mrcpengine.xml` в раздел `<plugins>`:

```xml
<plugin id="elevenlabs-synth" enable="true">
  <param name="api_key" value="YOUR_XI_API_KEY"/>
  <param name="voice_id" value="JBFqnCBsd6RMkjVDRZzb"/>
  <param name="model_id" value="eleven_multilingual_v2"/>
  <param name="output_format" value="pcm_8000"/>
  <param name="chunk_ms" value="20"/>
  <param name="connect_timeout_ms" value="5000"/>
  <param name="read_timeout_ms" value="15000"/>
  <param name="fallback_ulaw_to_pcm" value="true"/>
</plugin>
```

### 2. Параметры конфигурации

| Параметр | Описание | По умолчанию | Обязательный |
|----------|----------|---------------|--------------|
| `api_key` | API ключ ElevenLabs | - | Да |
| `voice_id` | ID голоса в ElevenLabs | - | Да |
| `model_id` | ID модели TTS | `eleven_multilingual_v2` | Нет |
| `output_format` | Формат аудио | `pcm_8000` | Нет |
| `chunk_ms` | Размер фрейма в миллисекундах | `20` | Нет |
| `connect_timeout_ms` | Таймаут подключения | `5000` | Нет |
| `read_timeout_ms` | Таймаут чтения | `15000` | Нет |
| `fallback_ulaw_to_pcm` | Конвертация μ-law в PCM | `true` | Нет |

### 3. Получение API ключа и Voice ID

1. Зарегистрируйтесь на [ElevenLabs](https://elevenlabs.io/)
2. Получите API ключ в разделе Profile Settings
3. Выберите голос в разделе Voice Library и скопируйте его ID

## Тестирование

### 1. Запуск UniMRCP сервера
```bash
cd installed
./unimrcpserver
```

### 2. Тестирование через UMC клиент

Создайте файл `test.txt` с текстом для синтеза:
```
Hello, this is a test of the ElevenLabs TTS plugin for UniMRCP.
```

Запустите UMC клиент:
```bash
./umc -r synthesizer -m speak -i test.txt
```

### 3. Ожидаемый вывод

В логах сервера должны появиться сообщения:
```
[elevenlabs-synth] Configuration loaded: voice_id=JBFqnCBsd6RMkjVDRZzb, model_id=eleven_multilingual_v2, output_format=pcm_8000, chunk_ms=20
[elevenlabs-synth] Processing SPEAK request with text: Hello, this is a test...
[elevenlabs-synth] Starting synthesis with URL: https://api.elevenlabs.io/v1/text-to-speech/JBFqnCBsd6RMkjVDRZzb?output_format=pcm_8000
[elevenlabs-synth] ElevenLabs API synthesis completed successfully
[elevenlabs-synth] Sent SPEAK-COMPLETE event with cause 0
```

## Архитектура

### Основные компоненты

1. **Engine** (`elevenlabs_synth_engine.c`)
   - Создание и управление движком
   - Парсинг конфигурации
   - Управление жизненным циклом

2. **Channel** (`elevenlabs_synth_channel.c`)
   - Обработка MRCP сообщений
   - Управление состоянием синтеза
   - Координация между HTTP клиентом и аудио потоком

3. **HTTP Client** (`elevenlabs_http.c`)
   - Взаимодействие с ElevenLabs API
   - Потоковая загрузка аудио
   - Обработка ошибок и таймаутов

4. **Audio Buffer** (встроен в channel)
   - Буферизация входящего аудио
   - Нарезка на фреймы заданного размера
   - Потокобезопасный доступ

5. **μ-law Decoder** (`ulaw_decode.c`)
   - Конвертация μ-law в 16-bit PCM
   - Табличная реализация для производительности

### Поток данных

```
MRCP SPEAK → Channel → HTTP Client → ElevenLabs API
                                    ↓
Audio Buffer ← HTTP Callback ← Streaming Response
    ↓
MPF Audio Stream → MRCP Client
```

## Устранение неполадок

### Ошибка "Failed to initialize libcurl"
- Убедитесь, что libcurl установлен
- Проверьте версию: `curl --version`

### Ошибка "Missing required parameter: api_key"
- Проверьте конфигурацию в `mrcpengine.xml`
- Убедитесь, что плагин включен (`enable="true"`)

### Ошибка "ElevenLabs API returned HTTP 401"
- Проверьте правильность API ключа
- Убедитесь, что у вас есть доступ к выбранному голосу

### Ошибка "ElevenLabs API request timed out"
- Увеличьте значения `connect_timeout_ms` и `read_timeout_ms`
- Проверьте сетевое подключение

### Проблемы с аудио
- Убедитесь, что `output_format` поддерживается
- Проверьте, что `chunk_ms` кратен 20 мс
- Убедитесь, что аудио кодек настроен на 8000 Hz

## Производительность

### Рекомендуемые настройки

- **chunk_ms**: 20-50 мс для низкой задержки
- **connect_timeout_ms**: 5000-10000 мс для стабильных сетей
- **read_timeout_ms**: 15000-30000 мс для длинных текстов

### Мониторинг

Плагин выводит подробные логи с префиксом `[elevenlabs-synth]`:
- `APT_PRIO_INFO`: Основные события
- `APT_PRIO_DEBUG`: Детальная отладочная информация
- `APT_PRIO_WARNING`: Предупреждения
- `APT_PRIO_ERROR`: Ошибки

## Лицензия

Apache License 2.0 - см. файл LICENSE в корне проекта.

## Поддержка
## Рекомендованная структура отдельного репозитория (только плагин)

```
elevenlabs-unimrcp-plugin/
├─ include/
│  ├─ elevenlabs_synth.h
│  ├─ elevenlabs_http.h
│  ├─ elevenlabs_utils.h
│  ├─ ulaw_decode.h
│  └─ elevenlabs_defs.h
├─ src/
│  ├─ elevenlabs_synth_engine.c
│  ├─ elevenlabs_synth_channel.c
│  ├─ elevenlabs_http.c
│  ├─ ulaw_decode.c
│  └─ elevenlabs_utils.c
├─ standalone/
│  └─ Makefile           # GNU Make сборка против установленного UniMRCP
├─ CMakeLists.txt        # CMake сборка (ELEVENLABS_STANDALONE=ON)
├─ README.md             # Эта инструкция
├─ LICENSE               # Apache-2.0
└─ doc.txt               # Краткий гайд по настройке/форматам/кэшу
```

Такой репозиторий можно публиковать на GitHub и собирать .so отдельно от исходников UniMRCP.

При возникновении проблем:
1. Проверьте логи UniMRCP сервера
2. Убедитесь в правильности конфигурации
3. Проверьте доступность ElevenLabs API
4. Создайте issue в репозитории проекта

## Дополнительные возможности

### Поддержка SSML
Плагин поддерживает базовую обработку SSML (удаление тегов). Для полной поддержки SSML рекомендуется использовать специализированные библиотеки.

### Кастомизация голосов
ElevenLabs позволяет создавать собственные голоса. Используйте их ID в параметре `voice_id`.

### Масштабирование
Плагин поддерживает множественные каналы и может быть использован в кластерной конфигурации UniMRCP.
