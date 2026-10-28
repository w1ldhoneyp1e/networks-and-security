## Архитектура

- `webserver/common/io.*` содержит надёжную запись в сокет и чтение HTTP-
  заголовков с ограничением их размера.
- `webserver/http/request.*` разбирает первую строку HTTP-запроса, а
  `webserver/http/response.*` формирует HTTP-ответ с заголовками.
- `webserver/server/static_files.*` безопасно сопоставляет URL с файлом в
  document root, читает его в бинарном режиме и определяет `Content-Type`.
- `webserver/server/web_server.*` создаёт TCP-сокет, выполняет `bind`, `listen`
  и `accept`, а затем обрабатывает одно подключение за раз.
- `webserver/client/main.cpp` проверяет аргументы и запускает сервер.
- В `webserver/tests/run_tests.py` находятся интеграционные проверки через
  loopback-соединение; внешняя сеть не используется.

Тесты вынесены в [TESTS.md](TESTS.md).

## Сборка

```bash
make -C webserver
```

## Запуск

```bash
./webserver/webserver [port] [document-root]
```

По умолчанию сервер слушает порт `8080` и отдаёт файлы из каталога `www`.
Пример запуска из корня репозитория:

```bash
./webserver/webserver 8080 ./webserver/www
```

В браузере откройте `http://127.0.0.1:8080/`. Сервер должен отдать
`index.html` с кодом `200`. Запрос несуществующего пути, например
`http://127.0.0.1:8080/missing.html`, вернёт `404`.
