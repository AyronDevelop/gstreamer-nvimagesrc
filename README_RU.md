# GStreamer NvImageSrc Plugin

Высокопроизводительный плагин GStreamer для захвата экрана с использованием NVIDIA Frame Buffer Capture (NvFBC) API и аппаратного кодирования H.264 через NVIDIA Video Encode API (NVENC).

## Возможности

- **Захват экрана с минимальной задержкой** используя NVIDIA NvFBC Direct Capture
- **Аппаратное кодирование H.264** с NVENC для максимальной производительности
- **Оптимизировано для потокового вещания в реальном времени** с минимальной нагрузкой на CPU
- **Интеграция с GStreamer** для простого создания пайплайнов
- **Многопоточная архитектура** для оптимальной производительности
- **Автоматическая обработка разрешения и форматов**

## Требования

### Аппаратное обеспечение
- Видеокарта NVIDIA с архитектурой Kepler или новее
- Драйвер GPU версии 470.x или новее
- Дисплей, подключенный к видеокарте NVIDIA

### Программное обеспечение
- Операционная система Linux
- Библиотеки разработки GStreamer 1.0+
- NVIDIA CUDA Toolkit
- Библиотеки разработки X11
- Библиотеки разработки OpenGL

### Библиотеки NVIDIA
- `libnvidia-fbc` - библиотека Frame Buffer Capture
- `libnvidia-encode` - библиотека Video Encode
- `libcuda` - библиотека CUDA runtime

## Инструкции по сборке

### Предварительные требования
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install build-essential pkg-config
sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
sudo apt install libx11-dev libgl1-mesa-dev
sudo apt install nvidia-cuda-toolkit

# Установить GStreamer в /opt/gstreamer (ожидается скриптом сборки)
# Или измените build.sh с путем к вашей установке GStreamer
```

### Компиляция
```bash
cd nvimage/
chmod +x build.sh
./build.sh
```

Это создаст файл плагина `libgstnvimagesrc.so`.

### Установка
```bash
# Скопировать плагин в директорию плагинов GStreamer
sudo cp libgstnvimagesrc.so /usr/lib/x86_64-linux-gnu/gstreamer-1.0/

# Или использовать переменную окружения GST_PLUGIN_PATH
export GST_PLUGIN_PATH=$PWD:$GST_PLUGIN_PATH
```

## Использование

### Базовый пайплайн
```bash
# Простой захват экрана в файл
gst-launch-1.0 nvimagesrc ! filesink location=screen.h264

# Захват экрана с настраиваемым битрейтом и частотой кадров
gst-launch-1.0 nvimagesrc bitrate=5000000 fps=60 ! filesink location=screen.h264

# Прямое вещание через UDP
gst-launch-1.0 nvimagesrc fps=30 bitrate=2000000 ! \
    udpsink host=192.168.1.100 port=5000
```

### Примеры продвинутых пайплайнов
```bash
# RTMP стриминг
gst-launch-1.0 nvimagesrc fps=60 bitrate=6000000 ! \
    h264parse ! flvmux ! \
    rtmpsink location=rtmp://live.twitch.tv/live/YOUR_STREAM_KEY

# Подготовка для WebRTC стриминга
gst-launch-1.0 nvimagesrc fps=30 bitrate=1500000 ! \
    h264parse ! rtph264pay ! \
    udpsink host=127.0.0.1 port=5004

# Множественные выходы
gst-launch-1.0 nvimagesrc fps=60 bitrate=8000000 ! tee name=t \
    t. ! queue ! filesink location=recording.h264 \
    t. ! queue ! udpsink host=192.168.1.100 port=5000
```

## Свойства плагина

| Свойство | Тип | По умолчанию | Описание |
|----------|-----|--------------|----------|
| `display-name` | string | NULL | Имя дисплея X11 (например, ":0") |
| `bitrate` | uint | 2000000 | Битрейт видео в битах в секунду |
| `fps` | double | 25.0 | Целевая частота кадров |
| `show-pointer` | boolean | TRUE | Включить курсор мыши в захват |

### Примеры свойств
```bash
# Захват высокого качества
gst-launch-1.0 nvimagesrc bitrate=10000000 fps=60 show-pointer=false ! \
    filesink location=high_quality.h264

# Стриминг с низкой задержкой
gst-launch-1.0 nvimagesrc bitrate=1000000 fps=30 ! \
    udpsink host=127.0.0.1 port=5000
```

## Оптимизация производительности

### Режим Direct Capture
Плагин автоматически включает NVIDIA Direct Capture когда это возможно для минимальной задержки:
- Требует полноэкранные приложения без перекрытий
- Курсор должен быть отключен (`show-pointer=false`)
- Push model включается автоматически
- Следите за сообщениями "Direct Capture ACTIVE" в логах

### Оптимальные настройки
```bash
# Захват игр/рабочего стола (высокое качество)
nvimagesrc fps=60 bitrate=8000000 show-pointer=false

# Удаленный рабочий стол (сбалансированно)
nvimagesrc fps=30 bitrate=2000000 show-pointer=true

# Стриминг (низкая задержка)
nvimagesrc fps=30 bitrate=1500000 show-pointer=false
```

## Устранение неполадок

### Частые проблемы

1. **Плагин не найден**
   ```bash
   # Проверить установку плагина
   gst-inspect-1.0 nvimagesrc
   
   # Если не найден, проверить GST_PLUGIN_PATH
   export GST_PLUGIN_PATH=/path/to/plugin:$GST_PLUGIN_PATH
   ```

2. **Отказано в доступе**
   ```bash
   # Пользователю нужен доступ к GPU и X дисплею
   sudo usermod -a -G video $USER
   # Требуется повторный вход в систему
   ```

3. **Черный экран при захвате**
   - Проверьте, подключен ли дисплей к видеокарте NVIDIA
   - Убедитесь, что X11 запущен на видеокарте NVIDIA
   - Проверьте настройки композитора

4. **Высокая нагрузка на CPU**
   - Убедитесь, что аппаратное кодирование работает
   - Проверьте, что Direct Capture активен
   - Уменьшите частоту кадров или битрейт

### Отладочная информация
```bash
# Включить отладочный вывод
GST_DEBUG=nvimagesrc:5 gst-launch-1.0 nvimagesrc ! fakesink

# Проверить статус Direct Capture
GST_DEBUG=nvimagesrc:4 gst-launch-1.0 nvimagesrc show-pointer=false ! fakesink
```

## Архитектура

### Компоненты
- **gstnvimagesrc.c**: Основная реализация плагина GStreamer
- **nvimageutil.c**: Основные утилиты захвата и кодирования
- **Интеграция NvFBC**: Захват экрана используя NVIDIA Frame Buffer Capture
- **Интеграция NVENC**: Аппаратное кодирование H.264
- **Многопоточность**: Отдельный рабочий поток для операций GPU

### Поток данных
1. NvFBC захватывает буфер кадра экрана в текстуру OpenGL
2. Текстура регистрируется в NVENC как входной ресурс
3. NVENC кодирует кадр в битовый поток H.264
4. Закодированные данные оборачиваются в буфер GStreamer
5. Буфер передается в нижестоящие элементы

## Лицензия

Эта библиотека является свободным программным обеспечением; вы можете распространять и/или модифицировать её в соответствии с условиями GNU Library General Public License, опубликованной Free Software Foundation; либо версии 2 лицензии, либо (по вашему выбору) любой более поздней версии.

## Участие в разработке

1. Сделайте форк репозитория
2. Создайте ветку для функции
3. Внесите изменения с английскими комментариями
4. Тщательно протестируйте
5. Отправьте pull request

## Авторы

- Оригинальная интеграция с GStreamer: Luca Ognibene
- Реализация захвата экрана: Zaheer Merali  
- Интеграция NVIDIA и оптимизации: Lukas Hejtmanek

## См. также

- [Документация GStreamer](https://gstreamer.freedesktop.org/documentation/)
- [NVIDIA Video Codec SDK](https://developer.nvidia.com/nvidia-video-codec-sdk)
- [NVIDIA Frame Buffer Capture](https://docs.nvidia.com/capture-sdk/)