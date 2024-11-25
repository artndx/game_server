docker build . -t http_server
docker run --name game_server -e POSTGRES_PASSWORD=postgres -d -p 8080:8080 http_server
docker exec -it game_server ./app/game_server -c ./app/data/config.json -w ./app/static