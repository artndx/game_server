# docker build . -t http_server
# docker run --name game_server -e POSTGRES_PASSWORD=postgres -d -p 8080:8080 http_server
# docker exec -it game_server ./app/game_server -c ./app/data/config.json -w ./app/static
# 1. Образ для сборки сервера
FROM gcc:11.3 as build

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.*

# Запуск conan как раньше
COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. -s compiler.libcxx=libstdc++11 --build=missing -s build_type=Release

# Папка data больше не нужна
COPY ./src /app/src
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .
# 2. Образ для запуска сервера и СУБД с базой данных для сервера
FROM postgres:15 as ps

# Скопируем приложение со сборочного контейнера в директорию /app.
# Не забываем также папку data, она пригодится.
COPY --from=build /app/build/game_server /app/
COPY ./data /app/data
COPY ./static /app/static
# ENTRYPOINT ["/app/game_server", "-c", "/app/data/config.json","-w", "/app/static"]