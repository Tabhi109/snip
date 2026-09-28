#!/usr/bin/env bash
set -euo pipefail

REPO="Tabhi109/snip"
INSTALL_DIR="${HOME}/.local/bin"

# Auto-detect OS
OS="$(uname -s | tr '[:upper:]' '[:lower:]')"
case "${OS}" in
  darwin) OS="macos" ;;
  linux)  OS="linux" ;;
  *)
    echo "Error: Unsupported OS '${OS}'" >&2
    exit 1
    ;;
esac

# Auto-detect Architecture
ARCH="$(uname -m)"
case "${ARCH}" in
  x86_64|amd64)  ARCH="x86_64" ;;
  arm64|aarch64) ARCH="arm64" ;;
  *)
    echo "Error: Unsupported architecture '${ARCH}'" >&2
    exit 1
    ;;
esac

TARGET="${OS}-${ARCH}"

# Fetch latest release tag dynamically if not specified
if [ -z "${VERSION:-}" ]; then
  VERSION=$(curl -fsSL "https://api.github.com/repos/${REPO}/releases/latest" | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/')
fi

if [ -z "${VERSION}" ]; then
  echo "Error: Failed to determine latest release tag from GitHub." >&2
  exit 1
fi

URL="https://github.com/${REPO}/releases/download/${VERSION}/snip-${TARGET}.tar.gz"

echo "==> Downloading snip ${VERSION} for ${TARGET}..."
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"' EXIT

HTTP_CODE=$(curl -sSL -w "%{http_code}" -o "${TMP_DIR}/snip.tar.gz" "${URL}")

if [ "${HTTP_CODE}" != "200" ]; then
  echo "Error: Release asset not found (HTTP ${HTTP_CODE}) at: ${URL}" >&2
  exit 1
fi

tar -xzf "${TMP_DIR}/snip.tar.gz" -C "${TMP_DIR}"

mkdir -p "${INSTALL_DIR}"
mv "${TMP_DIR}/snip" "${INSTALL_DIR}/snip"
chmod +x "${INSTALL_DIR}/snip"

echo "==> Successfully installed snip into ${INSTALL_DIR}/snip"