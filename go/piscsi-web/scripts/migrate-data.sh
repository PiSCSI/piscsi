#!/usr/bin/env bash
# Migrate data from the legacy Python web interface to the Go web interface.

set -euo pipefail

LEGACY_HOME="/home/pi"
DATA_DIR="/var/lib/piscsi"
WEB_CONFIG_DIR="/etc/piscsi-web"
SERVICE_USER="piscsi-web"
SERVICE_GROUP="piscsi"
DRY_RUN=false

usage() {
    cat <<'EOF'
Usage: migrate-data.sh [options]

Copy data from the legacy Python web interface into the Go web interface's
data directories. Existing destination files are never overwritten, and the
legacy files are not removed.

Options:
  --source-home PATH     Legacy user's home directory (default: /home/pi)
  --data-dir PATH        New PiSCSI data directory (default: /var/lib/piscsi)
  --web-config-dir PATH  Protected web config directory (default: /etc/piscsi-web)
  --dry-run              Show what would be migrated without changing anything
  -h, --help             Show this help

Run the real migration after installing the Go web interface and while the
piscsi-web service is stopped:

  sudo systemctl stop piscsi-web
  sudo ./scripts/migrate-data.sh
  sudo systemctl start piscsi-web
EOF
}

die() {
    echo "Error: $*" >&2
    exit 1
}

warn() {
    echo "Warning: $*" >&2
}

require_absolute_safe_path() {
    local name=$1
    local value=$2

    [[ "$value" = /* ]] || die "$name must be an absolute path: $value"
    [[ "$value" != "/" ]] || die "$name must not be the filesystem root"
    [[ "$value" != *$'\n'* ]] || die "$name must not contain a newline"
}

paths_overlap() {
    local first=${1%/}
    local second=${2%/}

    [[ "$first" = "$second" || "$first" = "$second"/* || "$second" = "$first"/* ]]
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --source-home)
            [[ $# -ge 2 ]] || die "--source-home requires a path"
            LEGACY_HOME=$2
            shift 2
            ;;
        --data-dir)
            [[ $# -ge 2 ]] || die "--data-dir requires a path"
            DATA_DIR=$2
            shift 2
            ;;
        --web-config-dir)
            [[ $# -ge 2 ]] || die "--web-config-dir requires a path"
            WEB_CONFIG_DIR=$2
            shift 2
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        *)
            die "unknown option: $1"
            ;;
    esac
done

require_absolute_safe_path "--source-home" "$LEGACY_HOME"
require_absolute_safe_path "--data-dir" "$DATA_DIR"
require_absolute_safe_path "--web-config-dir" "$WEB_CONFIG_DIR"

LEGACY_HOME=${LEGACY_HOME%/}
DATA_DIR=${DATA_DIR%/}
WEB_CONFIG_DIR=${WEB_CONFIG_DIR%/}

paths_overlap "$LEGACY_HOME" "$DATA_DIR" &&
    die "source home and destination data directory must not overlap"
paths_overlap "$LEGACY_HOME" "$WEB_CONFIG_DIR" &&
    die "source home and protected web config directory must not overlap"

LEGACY_IMAGES="$LEGACY_HOME/images"
LEGACY_SHARED="$LEGACY_HOME/shared_files"
LEGACY_CONFIG="$LEGACY_HOME/.config/piscsi"
LEGACY_TOKEN="$LEGACY_CONFIG/secret"
LEGACY_DRIVERS="$LEGACY_HOME/mac-hard-disk-drivers"

NEW_IMAGES="$DATA_DIR/images"
NEW_SHARED="$DATA_DIR/shared"
NEW_CONFIG="$DATA_DIR/config"
NEW_DATA="$DATA_DIR/data"
NEW_DRIVERS="$NEW_DATA/mac-hard-disk-drivers"
WEB_ENV_FILE="$WEB_CONFIG_DIR/piscsi-web.env"

if [[ "$DRY_RUN" = false ]]; then
    [[ "$EUID" -eq 0 ]] || die "this script must be run as root (use sudo)"
    getent group "$SERVICE_GROUP" >/dev/null ||
        die "group '$SERVICE_GROUP' does not exist; install the Go web interface first"
    getent passwd "$SERVICE_USER" >/dev/null ||
        die "user '$SERVICE_USER' does not exist; install the Go web interface first"

    if command -v systemctl >/dev/null &&
        systemctl is-active --quiet piscsi-web 2>/dev/null; then
        die "piscsi-web is running; stop it before migrating to avoid inconsistent data"
    fi
fi

for path in "$DATA_DIR" "$NEW_IMAGES" "$NEW_SHARED" "$NEW_CONFIG" "$NEW_DATA" "$NEW_DRIVERS" "$WEB_CONFIG_DIR"; do
    if [[ -L "$path" ]]; then
        die "refusing symbolic-link destination: $path"
    fi
    if [[ -d "$path" && -n "$(find "$path" -type l -print -quit)" ]]; then
        die "refusing destination tree containing symbolic links: $path"
    fi
done

for source in "$LEGACY_IMAGES" "$LEGACY_SHARED" "$LEGACY_CONFIG" "$LEGACY_DRIVERS"; do
    if [[ -e "$source" && (! -d "$source" || -L "$source") ]]; then
        die "legacy data source is not a regular directory: $source"
    fi
done
if [[ "$DRY_RUN" = false && -d "$LEGACY_CONFIG" ]] &&
    find "$LEGACY_CONFIG" -type f -name '*.json' -print -quit | grep -q .; then
    command -v python3 >/dev/null ||
        die "python3 is required to migrate image paths in saved configurations"
fi
if [[ -e "$LEGACY_TOKEN" ]]; then
    [[ -f "$LEGACY_TOKEN" && ! -L "$LEGACY_TOKEN" ]] ||
        die "refusing non-regular legacy token file: $LEGACY_TOKEN"
    awk 'NR > 1 { exit 1 }' "$LEGACY_TOKEN" ||
        die "legacy token contains multiple lines and cannot be migrated safely"
fi
if [[ -e "$WEB_ENV_FILE" && (! -f "$WEB_ENV_FILE" || -L "$WEB_ENV_FILE") ]]; then
    die "protected environment file is not a regular file: $WEB_ENV_FILE"
fi

copied=0
skipped=0
missing=0
copied_config_files=()
migration_temporary_file=""

cleanup() {
    if [[ -n "$migration_temporary_file" ]]; then
        rm -f -- "$migration_temporary_file"
    fi
}
trap cleanup EXIT

copy_tree() {
    local label=$1
    local source=$2
    local destination=$3
    local excluded_name=${4:-}
    local record_configs=${5:-false}
    local item
    local relative
    local target

    if [[ ! -e "$source" ]]; then
        echo "Not found, skipping $label: $source"
        missing=$((missing + 1))
        return
    fi
    [[ -d "$source" && ! -L "$source" ]] ||
        die "$label source is not a regular directory: $source"

    echo "Migrating $label: $source -> $destination"
    if [[ "$DRY_RUN" = false ]]; then
        mkdir -p -- "$destination"
    fi

    while IFS= read -r -d '' item; do
        relative=${item#"$source"/}
        if [[ -n "$excluded_name" && "$relative" = "$excluded_name" ]]; then
            echo "  protected file handled separately: $relative"
            continue
        fi
        target="$destination/$relative"

        if [[ -L "$item" ]]; then
            warn "symbolic link not migrated: $item"
            skipped=$((skipped + 1))
            continue
        fi
        if [[ -d "$item" && ! -L "$item" ]]; then
            if [[ -e "$target" && ! -d "$target" ]]; then
                warn "destination blocks directory, skipping: $target"
                skipped=$((skipped + 1))
            elif [[ "$DRY_RUN" = true ]]; then
                [[ -d "$target" ]] || echo "  create directory: $target"
            else
                mkdir -p -- "$target"
            fi
            continue
        fi

        if [[ ! -f "$item" ]]; then
            warn "unsupported file type, skipping: $item"
            skipped=$((skipped + 1))
            continue
        fi
        if [[ -e "$target" || -L "$target" ]]; then
            echo "  already exists, preserving destination: $target"
            skipped=$((skipped + 1))
            continue
        fi

        if [[ "$DRY_RUN" = true ]]; then
            echo "  copy: $relative"
        else
            mkdir -p -- "$(dirname "$target")"
            migration_temporary_file=$(
                mktemp "$(dirname "$target")/.piscsi-migrate.XXXXXX"
            )
            cp -a -- "$item" "$migration_temporary_file"
            if ln -- "$migration_temporary_file" "$target" 2>/dev/null; then
                rm -f -- "$migration_temporary_file"
            else
                echo "  destination appeared while copying, preserving it: $target"
                rm -f -- "$migration_temporary_file"
                migration_temporary_file=""
                skipped=$((skipped + 1))
                continue
            fi
            migration_temporary_file=""
        fi
        if [[ "$record_configs" = true && "$target" = *.json ]]; then
            copied_config_files+=("$target")
        fi
        copied=$((copied + 1))
    done < <(find "$source" -mindepth 1 -print0)
}

warn_about_legacy_configs() {
    local file
    local first_character

    [[ -d "$LEGACY_CONFIG" ]] || return 0
    while IFS= read -r -d '' file; do
        first_character=$(
            awk '{
                sub(/^[[:space:]]*/, "")
                if (length > 0) {
                    print substr($0, 1, 1)
                    exit
                }
            }' "$file"
        )
        if [[ "$first_character" = "[" ]]; then
            warn "$file uses the obsolete top-level-list format; load and re-save it with the Python web client before switching"
        fi
    done < <(find "$LEGACY_CONFIG" -type f -name '*.json' -print0)
}

rewrite_config_image_paths() {
    local file

    [[ ${#copied_config_files[@]} -gt 0 ]] || return 0
    if [[ "$DRY_RUN" = true ]]; then
        echo "Would convert absolute legacy image paths to relative paths in newly copied configurations"
        return
    fi
    for file in "${copied_config_files[@]}"; do
        python3 - "$file" "$LEGACY_IMAGES" <<'PY'
import json
import os
import pathlib
import sys
import tempfile

filename = pathlib.Path(sys.argv[1])
legacy_images = pathlib.Path(sys.argv[2])

try:
    with filename.open(encoding="utf-8") as stream:
        config = json.load(stream)
except (OSError, UnicodeError, json.JSONDecodeError) as error:
    print(f"Warning: cannot migrate image paths in {filename}: {error}", file=sys.stderr)
    raise SystemExit(0)

if not isinstance(config, dict):
    raise SystemExit(0)

changed = False
for device in config.get("devices", []):
    if not isinstance(device, dict):
        continue
    image = device.get("image")
    if not isinstance(image, str) or not os.path.isabs(image):
        continue
    try:
        relative = pathlib.Path(image).relative_to(legacy_images)
    except ValueError:
        continue
    device["image"] = str(relative)
    changed = True

if changed:
    descriptor, temporary_name = tempfile.mkstemp(
        dir=filename.parent, prefix=f".{filename.name}.", text=True
    )
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            json.dump(config, stream, indent=4)
            stream.write("\n")
        os.replace(temporary_name, filename)
    except BaseException:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise
    print(f"Converted legacy image paths in {filename}")
PY
    done
}

migrate_token() {
    local token
    local escaped

    [[ -e "$LEGACY_TOKEN" ]] || return 0
    [[ -f "$LEGACY_TOKEN" && ! -L "$LEGACY_TOKEN" ]] ||
        die "refusing non-regular legacy token file: $LEGACY_TOKEN"
    if ! awk 'NR > 1 { exit 1 }' "$LEGACY_TOKEN"; then
        die "legacy token contains multiple lines and cannot be migrated safely"
    fi

    token=$(sed -n '1p' "$LEGACY_TOKEN")
    [[ -n "$token" ]] || {
        warn "legacy token is empty; not adding PISCSI_TOKEN"
        return
    }

    if [[ -f "$WEB_ENV_FILE" ]] &&
        grep -Eq '^[[:space:]]*PISCSI_TOKEN[[:space:]]*=' "$WEB_ENV_FILE"; then
        echo "Protected environment already defines PISCSI_TOKEN; preserving it"
        return
    fi

    if [[ "$DRY_RUN" = true ]]; then
        echo "Would import the legacy backend token into $WEB_ENV_FILE"
        return
    fi

    [[ ! -L "$WEB_ENV_FILE" ]] ||
        die "refusing symbolic-link environment file: $WEB_ENV_FILE"
    [[ ! -e "$WEB_ENV_FILE" || -f "$WEB_ENV_FILE" ]] ||
        die "environment file is not a regular file: $WEB_ENV_FILE"

    escaped=${token//\\/\\\\}
    escaped=${escaped//\"/\\\"}
    escaped=${escaped//\$/\\$}
    escaped=${escaped//\`/\\\`}

    migration_temporary_file=$(mktemp "$WEB_CONFIG_DIR/.piscsi-web.env.XXXXXX")
    if [[ -f "$WEB_ENV_FILE" ]]; then
        cp -- "$WEB_ENV_FILE" "$migration_temporary_file"
    fi
    printf '\nPISCSI_TOKEN="%s"\n' "$escaped" >>"$migration_temporary_file"
    chown root:"$SERVICE_GROUP" "$migration_temporary_file"
    chmod 0640 "$migration_temporary_file"
    mv -f -- "$migration_temporary_file" "$WEB_ENV_FILE"
    migration_temporary_file=""
    echo "Imported the legacy backend token into $WEB_ENV_FILE"
}

echo "PiSCSI Python-to-Go data migration"
[[ "$DRY_RUN" = true ]] && echo "Dry run: no files or permissions will be changed"
echo

warn_about_legacy_configs
copy_tree "disk images" "$LEGACY_IMAGES" "$NEW_IMAGES"
copy_tree "shared files" "$LEGACY_SHARED" "$NEW_SHARED"
copy_tree "saved configurations and image properties" \
    "$LEGACY_CONFIG" "$NEW_CONFIG" "secret" true
copy_tree "Macintosh hard disk drivers" "$LEGACY_DRIVERS" "$NEW_DRIVERS"
rewrite_config_image_paths

if [[ "$DRY_RUN" = false ]]; then
    install -d -o root -g "$SERVICE_GROUP" -m 2770 \
        "$DATA_DIR" "$NEW_IMAGES" "$NEW_SHARED"
    install -d -o "$SERVICE_USER" -g "$SERVICE_GROUP" -m 2770 "$NEW_CONFIG"
    install -d -o root -g root -m 0755 "$NEW_DATA" "$NEW_DRIVERS"
    install -d -o root -g "$SERVICE_GROUP" -m 0750 "$WEB_CONFIG_DIR"

    chown -R root:"$SERVICE_GROUP" "$NEW_IMAGES" "$NEW_SHARED"
    chown -R "$SERVICE_USER:$SERVICE_GROUP" "$NEW_CONFIG"
    find "$NEW_IMAGES" "$NEW_SHARED" "$NEW_CONFIG" -type d -exec chmod 2770 {} +
    find "$NEW_IMAGES" "$NEW_SHARED" "$NEW_CONFIG" -type f \
        -exec chmod u+rw,g+rw,o-rwx {} +
    chown -R root:root "$NEW_DRIVERS"
    chmod -R go-w "$NEW_DRIVERS"
fi

migrate_token

echo
echo "Migration complete: $copied file(s) copied, $skipped item(s) skipped."
if [[ "$missing" -gt 0 ]]; then
    echo "$missing legacy data directory/directories were not present."
fi
echo "The legacy data under $LEGACY_HOME was left unchanged."
if [[ "$DRY_RUN" = false ]]; then
    echo "Start the Go web interface with: systemctl start piscsi-web"
fi
