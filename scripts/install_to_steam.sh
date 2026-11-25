#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DLL_PATH="${ROOT_DIR}/build-win64/GW2QuestHelper.dll"

if [[ ! -f "${DLL_PATH}" ]]; then
    echo "DLL not found at ${DLL_PATH}; building first..."
    "${ROOT_DIR}/scripts/build.sh"
fi

find_gw2_dir() {
    if [[ -n "${GW2_DIR:-}" && -d "${GW2_DIR}" ]]; then
        printf '%s\n' "${GW2_DIR}"
        return 0
    fi

    local candidates=(
        "${HOME}/.local/share/Steam/steamapps/common/Guild Wars 2"
        "${HOME}/.steam/steam/steamapps/common/Guild Wars 2"
        "${HOME}/.var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/common/Guild Wars 2"
    )

    # Also inspect Steam libraryfolders.vdf for secondary drives/mounts
    local vdf_files=(
        "${HOME}/.local/share/Steam/steamapps/libraryfolders.vdf"
        "${HOME}/.steam/steam/steamapps/libraryfolders.vdf"
        "${HOME}/.var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/libraryfolders.vdf"
    )

    for vdf in "${vdf_files[@]}"; do
        if [[ -f "${vdf}" ]]; then
            while IFS= read -r libpath; do
                candidates+=("${libpath}/steamapps/common/Guild Wars 2")
            done < <(grep -oE '"path"[[:space:]]+"[^"]+"' "${vdf}" | sed -E 's/"path"[[:space:]]+"([^"]+)"/\1/')
        fi
    done

    for dir in "${candidates[@]}"; do
        if [[ -f "${dir}/Gw2-64.exe" || -d "${dir}/addons" ]]; then
            printf '%s\n' "${dir}"
            return 0
        fi
    done

    return 1
}

if ! TARGET_GW2_DIR="$(find_gw2_dir)"; then
    echo "Could not auto-detect your Steam 'Guild Wars 2' directory." >&2
    echo "Run again with: GW2_DIR=\"/path/to/Guild Wars 2\" $0" >&2
    exit 1
fi

ADDONS_DIR="${TARGET_GW2_DIR}/addons"
DATA_DIR="${ADDONS_DIR}/GW2QuestHelper"

mkdir -p "${ADDONS_DIR}" "${DATA_DIR}"
cp --remove-destination -f "${DLL_PATH}" "${ADDONS_DIR}/GW2QuestHelper.dll"

ENV_FILE="${DATA_DIR}/.env"
if [[ ! -f "${ENV_FILE}" ]]; then
    cat > "${ENV_FILE}" <<'EOF'
# Paste your Guild Wars 2 API key from https://account.arena.net/applications
# Required scopes: account, progression, unlocks, characters
GW2_API_KEY=
EOF
    chmod 600 "${ENV_FILE}"
    echo "==> Created API key config (chmod 600): ${ENV_FILE}"
else
    echo "==> Preserved existing API key config: ${ENV_FILE}"
fi

echo "==> Installed addon DLL to: ${ADDONS_DIR}/GW2QuestHelper.dll"
echo "==> In Guild Wars 2, open Raidcore Nexus (Ctrl+O) -> Addons -> Load 'GW2 Quest Helper' (or press ALT+SHIFT+Q)."
