.DEFAULT_GOAL := build

# Go parameters
GOCMD=go
GOBUILD=$(GOCMD) build
GOCLEAN=$(GOCMD) clean
GOTEST=$(GOCMD) test
GOMOD=$(GOCMD) mod
PROTOC_GEN_GO_VERSION=v1.36.11
GO_BIN=$(shell $(GOCMD) env GOBIN)
ifeq ($(GO_BIN),)
GO_BIN=$(shell $(GOCMD) env GOPATH)/bin
endif
PROTOC_GEN_GO=$(GO_BIN)/protoc-gen-go

# Binary names
BINARY_NAME=piscsi-web
BINARY_UNIX=$(BINARY_NAME)_unix
MOCK_BINARY=mock-piscsi
OLED_BINARY=piscsi-oled
CTRLBOARD_BINARY=piscsi-ctrlboard
OLED_INSTALL_DIR ?= /opt/piscsi/oled
SYSTEMD_UNIT_DIR ?= /etc/systemd/system
OLED_SERVICE=piscsi-oled.service

# Build flags
LDFLAGS=-ldflags "-s -w"
DEFAULT_BASE_DIR ?= /var/lib/piscsi/images
DEFAULT_SHARED_DIR ?= /var/lib/piscsi/shared
DEFAULT_CONFIG_DIR ?= /var/lib/piscsi/config
DEFAULT_DATA_DIR ?= /var/lib/piscsi/data
DEFAULT_SESSION_KEY_FILE ?= /etc/piscsi-web/session.key
CONFIG_LDFLAGS = -X github.com/piscsi/piscsi/go/piscsi-web/internal/config.defaultBaseDir=$(DEFAULT_BASE_DIR) \
	-X github.com/piscsi/piscsi/go/piscsi-web/internal/config.defaultSharedDir=$(DEFAULT_SHARED_DIR) \
	-X github.com/piscsi/piscsi/go/piscsi-web/internal/config.defaultConfigDir=$(DEFAULT_CONFIG_DIR) \
	-X github.com/piscsi/piscsi/go/piscsi-web/internal/config.defaultDataDir=$(DEFAULT_DATA_DIR) \
	-X github.com/piscsi/piscsi/go/piscsi-web/internal/config.defaultSessionKeyFile=$(DEFAULT_SESSION_KEY_FILE)
WEB_LDFLAGS = -ldflags "-s -w $(CONFIG_LDFLAGS)"
STATIC_WEB_LDFLAGS = -ldflags "-s -w $(CONFIG_LDFLAGS) -extldflags=-static"
# Gin's optional MessagePack codec is unused and expensive to compile on
# memory-constrained Raspberry Pis.
WEB_TAGS ?= nomsgpack
STATIC_WEB_TAGS ?= nomsgpack
WEB_TAG_FLAGS=-tags "$(WEB_TAGS)"
STATIC_WEB_TAG_FLAGS=-tags "$(STATIC_WEB_TAGS)"

all: test build

build: proto
	$(GOBUILD) $(WEB_TAG_FLAGS) $(WEB_LDFLAGS) -o $(BINARY_NAME) -v ./piscsi-web/cmd/piscsi-web

mock: proto
	@echo "Building mock PiSCSI daemon..."
	$(GOBUILD) $(LDFLAGS) -o $(MOCK_BINARY) -v ./piscsi-web/cmd/mock-piscsi
	@echo "✅ Mock daemon created: $(MOCK_BINARY)"
	@echo "Run with: ./$(MOCK_BINARY)"

build-linux-arm64: proto
	@echo "Building for ARM64 (Raspberry Pi 4/5)..."
	CGO_ENABLED=0 GOOS=linux GOARCH=arm64 \
	  $(GOBUILD) $(WEB_TAG_FLAGS) $(WEB_LDFLAGS) -o $(BINARY_NAME)-arm64 ./piscsi-web/cmd/piscsi-web
	@echo "✅ ARM64 binary created"
	@ls -lh $(BINARY_NAME)-arm64

build-linux-armv7: proto
	@echo "Building for ARMv7 (Raspberry Pi 2/3)..."
	CGO_ENABLED=0 GOOS=linux GOARCH=arm GOARM=7 \
	  $(GOBUILD) $(WEB_TAG_FLAGS) $(WEB_LDFLAGS) -o $(BINARY_NAME)-armv7 ./piscsi-web/cmd/piscsi-web
	@echo "✅ ARMv7 binary created"
	@ls -lh $(BINARY_NAME)-armv7

build-linux-arm64-static: proto
	@echo "Building static ARM64 binary (Raspberry Pi 4/5)..."
	CGO_ENABLED=0 GOOS=linux GOARCH=arm64 \
	  $(GOBUILD) $(STATIC_WEB_TAG_FLAGS) $(STATIC_WEB_LDFLAGS) -o $(BINARY_NAME)-arm64-static ./piscsi-web/cmd/piscsi-web
	@echo "✅ Static ARM64 binary created: $(BINARY_NAME)-arm64-static"
	@file $(BINARY_NAME)-arm64-static
	@ls -lh $(BINARY_NAME)-arm64-static

build-linux-armv7-static: proto
	@echo "Building static ARMv7 binary (Raspberry Pi 2/3)..."
	CGO_ENABLED=0 GOOS=linux GOARCH=arm GOARM=7 \
	  $(GOBUILD) $(STATIC_WEB_TAG_FLAGS) $(STATIC_WEB_LDFLAGS) -o $(BINARY_NAME)-armv7-static ./piscsi-web/cmd/piscsi-web

build-oled: proto
	$(GOBUILD) $(LDFLAGS) -o $(OLED_BINARY) -v ./cmd/piscsi-oled

build-ctrlboard: proto
	$(GOBUILD) $(LDFLAGS) -o $(CTRLBOARD_BINARY) -v ./cmd/piscsi-ctrlboard

build-oled-linux-arm64: proto
	CGO_ENABLED=0 GOOS=linux GOARCH=arm64 $(GOBUILD) $(LDFLAGS) -o $(OLED_BINARY)-arm64 ./cmd/piscsi-oled

build-oled-linux-armv7: proto
	CGO_ENABLED=0 GOOS=linux GOARCH=arm GOARM=7 $(GOBUILD) $(LDFLAGS) -o $(OLED_BINARY)-armv7 ./cmd/piscsi-oled
	@echo "✅ Static ARMv7 binary created: $(BINARY_NAME)-armv7-static"
	@file $(BINARY_NAME)-armv7-static
	@ls -lh $(BINARY_NAME)-armv7-static

build-ctrlboard-linux-arm64: proto
	CGO_ENABLED=0 GOOS=linux GOARCH=arm64 $(GOBUILD) $(LDFLAGS) -o $(CTRLBOARD_BINARY)-arm64 ./cmd/piscsi-ctrlboard

build-ctrlboard-linux-armv7: proto
	CGO_ENABLED=0 GOOS=linux GOARCH=arm GOARM=7 $(GOBUILD) $(LDFLAGS) -o $(CTRLBOARD_BINARY)-armv7 ./cmd/piscsi-ctrlboard

test: proto
	$(GOTEST) $(WEB_TAG_FLAGS) -v -coverprofile=coverage.txt -covermode=atomic ./...

test-race: proto
	$(GOTEST) $(WEB_TAG_FLAGS) -v -race -coverprofile=coverage.txt -covermode=atomic ./...

test-short: proto
	$(GOTEST) $(WEB_TAG_FLAGS) -v -short ./...

test-server: proto
	$(GOTEST) $(WEB_TAG_FLAGS) -v ./piscsi-web/internal/server

test-coverage: proto
	$(GOTEST) $(WEB_TAG_FLAGS) -v -coverprofile=coverage.txt -covermode=atomic ./...
	$(GOCMD) tool cover -html=coverage.txt -o coverage.html
	@echo "✅ Coverage report generated: coverage.html"

test-verbose: proto
	$(GOTEST) $(WEB_TAG_FLAGS) -v ./...

clean:
	$(GOCLEAN)
	rm -f $(BINARY_NAME)
	rm -f $(BINARY_NAME)-arm64 $(BINARY_NAME)-armv7
	rm -f $(BINARY_NAME)-arm64-static $(BINARY_NAME)-armv7-static
	rm -f $(MOCK_BINARY)
	rm -f $(OLED_BINARY) $(OLED_BINARY)-arm64 $(OLED_BINARY)-armv7
	rm -f $(CTRLBOARD_BINARY) $(CTRLBOARD_BINARY)-arm64 $(CTRLBOARD_BINARY)-armv7
	rm -f coverage.txt coverage.html

run: proto
	$(GOBUILD) $(WEB_TAG_FLAGS) $(WEB_LDFLAGS) -o $(BINARY_NAME) -v ./piscsi-web/cmd/piscsi-web
	./$(BINARY_NAME)

deps:
	$(GOMOD) download
	$(GOMOD) verify

tidy:
	$(GOMOD) tidy

proto: proto-tools
	PATH="$(GO_BIN):$$PATH" ./proto/generate.sh

proto-tools:
	@command -v protoc >/dev/null || { echo "protoc is required; install protobuf-compiler."; exit 1; }
	@test -x "$(PROTOC_GEN_GO)" || { echo "protoc-gen-go is required; install it with: go install google.golang.org/protobuf/cmd/protoc-gen-go@$(PROTOC_GEN_GO_VERSION)"; exit 1; }
	@actual="$$($(PROTOC_GEN_GO) --version)"; expected="protoc-gen-go $(PROTOC_GEN_GO_VERSION)"; test "$$actual" = "$$expected" || { echo "$$expected is required (found $$actual)."; exit 1; }

install:
	@echo "Installing PiSCSI Web Interface..."
	@./piscsi-web/scripts/install.sh

install-oled: build-oled
	@echo "Installing PiSCSI OLED monitor..."
	install -d "$(OLED_INSTALL_DIR)"
	install -m 0755 "$(OLED_BINARY)" "$(OLED_INSTALL_DIR)/$(OLED_BINARY)"
	install -m 0644 "$(OLED_SERVICE)" "$(SYSTEMD_UNIT_DIR)/$(OLED_SERVICE)"
	systemctl daemon-reload

migrate-data:
	@./piscsi-web/scripts/migrate-data.sh

uninstall:
	@echo "Uninstalling PiSCSI Web Interface..."
	@./piscsi-web/scripts/uninstall.sh

.PHONY: all build mock build-oled build-ctrlboard build-linux-arm64 build-linux-armv7 build-linux-arm64-static build-linux-armv7-static build-oled-linux-arm64 build-oled-linux-armv7 build-ctrlboard-linux-arm64 build-ctrlboard-linux-armv7 test test-race test-short test-server test-coverage test-verbose clean run deps tidy proto proto-tools install install-oled migrate-data uninstall
