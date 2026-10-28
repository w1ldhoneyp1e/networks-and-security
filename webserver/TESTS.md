# Тесты

## 1. Автоматическая проверка

```bash
make -C webserver test
```

Команда собирает сервер, запускает его с временным document root на
`127.0.0.1` и посылает HTTP-запросы через TCP. Внешняя сеть не требуется.

Автотесты проверяют:

- ответ `200 OK`, тело файла, `Content-Length` и `Content-Type`;
- ответ `404 Not Found` для отсутствующего файла;
- ответ `405 Method Not Allowed` для метода, отличного от `GET`;
- защиту от перехода за document root через `..` в URL.

## 2. Ручная проверка

Соберите и запустите сервер:

```bash
make -C webserver
./webserver/webserver 8080 ./webserver/www
```

В другом терминале выполните:

```bash
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8080/not-found.html
```

Первый запрос должен вернуть `HTTP/1.1 200 OK` и содержимое `index.html`,
второй — `HTTP/1.1 404 Not Found` и текст `File Not Found`. В консоли сервера
будут строки вида `GET / -> 200` и `GET /not-found.html -> 404`.
