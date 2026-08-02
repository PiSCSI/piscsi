ARG GO_VERSION=1.25

FROM golang:${GO_VERSION}-bookworm

WORKDIR /src

RUN apt-get update \
    && apt-get install --yes --no-install-recommends protobuf-compiler \
    && rm -rf /var/lib/apt/lists/* \
    && go install google.golang.org/protobuf/cmd/protoc-gen-go@v1.36.11

COPY go go
COPY proto proto

WORKDIR /src/go
ENTRYPOINT ["make", "test"]
