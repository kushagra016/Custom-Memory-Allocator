FROM gcc:latest
WORKDIR /app
COPY . .
RUN g++ main.cpp allocator.cpp utils.cpp -o memory_server -std=c++17 -pthread
CMD ["./memory_server"]