#!/usr/bin/env bash
# Generate Go protobuf bindings from piscsi_interface.proto

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
WEB_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="$(cd "${WEB_ROOT}/../.." && pwd)"
PROTO_FILE="${REPO_ROOT}/proto/piscsi_interface.proto"

echo "🔧 Generating Go protobuf bindings..."

# Generate Go bindings from the repository's canonical schema. The package
# mapping avoids adding Go-specific metadata to a schema shared with C++ and Python.
cd "${WEB_ROOT}"
protoc \
  --proto_path="${REPO_ROOT}" \
  --go_out=. \
  --go_opt=paths=source_relative \
  --go_opt=Mproto/piscsi_interface.proto=github.com/piscsi/piscsi-web/proto \
  "${PROTO_FILE}"

echo "✅ Generated Go protobuf bindings in proto"
echo "   Output file: proto/piscsi_interface.pb.go"
