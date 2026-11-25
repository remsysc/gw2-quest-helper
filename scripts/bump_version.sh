#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION_FILE="${ROOT_DIR}/VERSION"

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <patch|minor|major|X.Y.Z>" >&2
    exit 1
fi

CURRENT="$(tr -d '[:space:]' < "${VERSION_FILE}")"
IFS='.' read -r MAJ MIN PAT <<< "${CURRENT}"

case "$1" in
    patch)
        PAT=$((PAT + 1))
        NEXT="${MAJ}.${MIN}.${PAT}"
        ;;
    minor)
        MIN=$((MIN + 1))
        PAT=0
        NEXT="${MAJ}.${MIN}.${PAT}"
        ;;
    major)
        MAJ=$((MAJ + 1))
        MIN=0
        PAT=0
        NEXT="${MAJ}.${MIN}.${PAT}"
        ;;
    *)
        if [[ ! "$1" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
            echo "Invalid SemVer '$1' (expected X.Y.Z)" >&2
            exit 1
        fi
        NEXT="$1"
        ;;
esac

printf '%s\n' "${NEXT}" > "${VERSION_FILE}"
echo "==> Updated VERSION: ${CURRENT} -> ${NEXT}"
echo "==> Don't forget to add an entry under [v${NEXT}] in CHANGELOG.md before building!"
