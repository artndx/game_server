# Игровой сервер

&emsp; Веб-сервер, представляющий собой игру, в которой нужно собирать потерянные предметы на карте при помощи поисковой собаки.

Реализован с использованием:
* Boost Asio
* Boost Beast
* Boost JSON
* Boost Log
* Boost Signals2
* Catch2
* LIBPQXX

---

## Установка и запуск

&emsp; Сервер можно собрать и запустить в докер контейнере, образ которого собирается из докер-файла. В директории проекта применить следующие команды:
```
docker build . -t http_server
docker run --name game_server -e POSTGRES_PASSWORD=postgres -d -p 8080:8080 http_server
docker exec -it game_server ./app/game_server -c ./app/data/config.json -w ./app/static
```

---

## Запросы

&emsp; Точка входа :
* ```http:/127.0.0.1:8080/```  - главная страница
* ```/api/v1/maps``` - вывести список всех доступных карт
* ```/api/v1/maps/{map_id}``` - вывести описание карты по её ```map_id```
* ```/api/v1/game/join``` - запрос на присоединение в сессию с указанием карты
```
POST http://127.0.0.1:8080/api/v1/game/join HTTP/1.1
Content-Type: application/json
{"userName": "User1", "mapId": "map1"}
```
- В результате будет выдан HTTP-ответ с генерированным авторизационным токеном, который требуется для дальнейших запросов от игрока:
```
HTTP/1.1 200 OK
Content-Type: application/json
Cache-Control: no-cache
Content-Length: 61

{
  "authToken": "{token}",
  "playerId": 0
}
```
- ```/api/v1/game/players``` - вывести список игроков, находящихся в одной сессии
```
GET http://127.0.0.1:8080/api/v1/game/players HTTP/1.1
Authorization: Bearer {token}
```
- ```http:/127.0.0.1:8080/api/v1/game/state``` - вывести информации о состоянии сессии (позиции игроков, скорость, собранные предметы, информацию о несобранных предметах, их позиции на карте)

* ```/api/v1/game/player/action``` - применить действие к управляемой игроком собаке .
- Можно задать 4 направления:
- ```{"move": "U"}``` - движение вверх
- ```{"move": "D"}``` - движение вниз
- ```{"move": "L"}``` - движение влево
- ```{"move": "R"}``` - движение вправо
- ```{"move": ""}``` - остановиться
```
POST http://127.0.0.1:8080/api/v1/game/player/action HTTP/1.1
Content-Type: application/json
Authorization: Bearer {token}

{"move": "R"}
```
- (Доступно только при отсутствии аргумента --tick-period {milliseconds}) 
- ```./app/game_server -c ./app/data/config.json -w ./app/static --tick-period {milliseconds}``` - включает автоматический ход игровых часов с периодом {milliseconds}
* ```/api/v1/game/tick``` - увеличивает время на заданную величину
```
POST http://127.0.0.1:8080/api/v1/game/tick HTTP/1.1
Content-Type: application/json

{"timeDelta": 1000}
```
---

 
