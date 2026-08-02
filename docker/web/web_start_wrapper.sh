#!/usr/bin/env sh
set -eu

# The Docker environment is for local development. It uses a throwaway session
# key unless the caller provides SESSION_KEY or SESSION_KEY_FILE explicitly.
if [ -z "${SESSION_KEY_FILE+x}" ] && [ -z "${SESSION_KEY:-}" ]; then
    export SESSION_KEY="$(openssl rand -base64 48)"
fi

exec /usr/local/bin/piscsi-web "$@"
