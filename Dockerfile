FROM alpine:latest

RUN apk add --no-cache gcc make musl-dev

WORKDIR /app
COPY . .

RUN make

CMD ["./.build/wosh"]
