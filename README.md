# Лабораторная работа 5: Веб-интерфейс для мониторинга температурных данных

Система для сбора данных через последовательный порт с HTTP API и интерактивным веб-интерфейсом, автоматической агрегацией данных и двумя вариантами хранения.

## Сборка и запуск

```bash
# Сборка проекта
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .

# Запуск основной службы с конфигурацией
./main -c config.json

# Запуск симулятора данных (пример для Linux)
./tools/simulator -p /dev/ttyUSB0 -b 115200
```

## Основные возможности
- Чтение данных с COM-порта (Windows/Linux/macOS)
- 2 режима хранения:
  - Файловое хранилище в CSV формате
  - PostgreSQL с автоматической чисткой 
- Веб-интерфейс с графиками:
  - Сырые данные за 24 часа
  - Часовые средние за 30 дней
  - Дневные средние за 1 год 
- REST API для интеграции
- Шаблонизация HTML через Jinja2
- Автоматическая перезагрузка статических ресурсов

## Пример конфигурации для PostgreSQL (config.json)
```json
{
    "serial": {
        "serial_port": "/dev/ttyUSB0",
        "baud_rate": 115200
    },
    "storage": {
        "db_client": {
            "host_address": "127.0.0.1",
            "port": 5432,
            "db_name": "temperature",
            "user_name": "monitoring",
            "password_env": "DB_PASS"
        }
    },
    "mesure_delay": 150,
    "assets_path": "./assets"
}
```

## Пример конфигурации для файлового хранилища (file_config.json)
```json
{
    "serial": {
        "serial_port": "COM3",
        "baud_rate": 9600
    },
    "storage": {
        "file_system": {
            "temperature": "data/current.log",
            "hourly": "data/hourly_avg.log",
            "daily": "data/daily_avg.log"
        }
    },
    "mesure_delay": 200
}
```

## Проверка работы
1. Запустите сервис и симулятор
2. Откройте веб-интерфейс: `http://localhost:8080/assets/index.html`
3. Проверьте API через curl:
```bash
curl http://localhost:8080/list/raw
curl http://localhost:8080/list/hour
curl http://localhost:8080/list/day
```

4. Убедитесь в создании:
- CSV файлов в указанной директории (для файлового хранилища)
- Таблиц в базе данных (для PostgreSQL)

## Особенности файлового хранилища
- Сырые данные: `current.log`
- Часовые средние: `hourly_avg.log`
- Дневные средние: `daily_avg.log`
- Формат записей: `YYYY-MM-DDTHH:MM:SSZ <значение>`
- Автоматическая ротация данных
