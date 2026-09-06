# AirSiren for ESP32-C6

Індикатор повітряної тривоги для Дніпра на Waveshare ESP32-C6-LCD-1.47. Пристрій опитує публічний API Tryvoha.online через HTTPS і показує безпечний стан на LCD та вбудованому RGB LED.

> Це допоміжний інформаційний пристрій. Він не замінює офіційні сирени, застосунок «Повітряна тривога», повідомлення ДСНС або вказівки органів влади.

## Стани

| Стан | Екран | RGB LED |
| --- | --- | --- |
| Тривога області або Дніпровського району | червоний, `ALERT` | червоний |
| Свіжа підтверджена відсутність тривоги | зелений, `ALL CLEAR` | зелений |
| Запуск, немає мережі, помилка API | бурштиновий, `STARTING` або `NO DATA` | вимкнений |
| Дані старші за 90 секунд | бурштиновий, `STALE` | вимкнений |

Зелений ніколи не вмикається за відсутності валідної свіжої відповіді. API опитується раз на 30 секунд; помилки використовують обмежений backoff 5–60 секунд.

Під час офіційної тривоги екран також показує `THREAT: MISSILE`,
`KAB`, `DRONE`, `RECON DRONE`, `WARNING`, `MULTIPLE` або `UNKNOWN`.

## Обладнання

- ESP32-C6FH8, перевірені 8 МБ flash
- ST7789, 172 × 320: MOSI GPIO6, SCLK GPIO7, CS GPIO14, DC GPIO15, RESET GPIO21
- LCD backlight: GPIO22, 40% яскравості
- WS2812-сумісний RGB LED: GPIO8, порядок RGB
- Wi-Fi: лише мережа 2.4 GHz

Проєкт використовує окремий 4 МБ factory application-розділ без OTA. Це відповідає перевіреному 8 МБ чіпу; не прошивайте цей образ у 4 МБ варіант плати.

## Налаштування Wi-Fi

Скопіюйте `main/secrets.example.h` у `main/secrets.h` і замініть значення:

```cpp
#define AIRSIREN_WIFI_SSID "назва-мережі-2.4GHz"
#define AIRSIREN_WIFI_PASSWORD "пароль"
```

`secrets.h` ігнорується Git. Не додавайте справжні паролі до tracked-файлів.

## Збірка, прошивка й монітор

Відкрийте каталог `ESP32/AirSiren` у VS Code з PlatformIO або виконайте:

```sh
/Users/sergejnomerovskij/.platformio/penv/bin/pio run
/Users/sergejnomerovskij/.platformio/penv/bin/pio run --target upload
/Users/sergejnomerovskij/.platformio/penv/bin/pio device monitor
```

Прошивка не входить до автоматичної перевірки: перед нею переконайтеся, що підключена саме ESP32-C6FH8 з 8 МБ flash і вибрано правильний USB-порт.

## Тести

Host-тести не звертаються до мережі та перевіряють парсинг, fail-closed поведінку, свіжість даних і планувальник:

```sh
sh test/run_host_tests.sh
```

## Джерело даних

- Endpoint: `https://tryvoha.online/api/v1/alerts/dnipropetrovska`
- Додатковий live-feed загроз: `https://tryvoha.online/api/events`
- Документація: <https://tryvoha.online/api>
- Дані на екрані атрибутовано як `DATA: TRYVOHA.ONLINE`.

Tryvoha.online є незалежним неофіційним агрегатором без гарантій доступності. Provider-specific код ізольований, щоб пізніше додати NEPTUN або офіційний API без зміни логіки LED та екрана.

Типи загроз є другорядною інформацією, автоматично сформованою сервісом з
відкритих повідомлень. Live-endpoint публічний, але не документований, а події
не є радарними даними. Якщо цей feed недоступний або застарів, офіційний
червоний статус зберігається, а екран показує `THREAT: UNKNOWN`.
