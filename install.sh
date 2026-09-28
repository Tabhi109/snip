#!/usr/bin/env bash
set -e

REPO="Tabhi109/snip"
VERSION="v0.2.0"

OS="$(uname -s | tr '[:upper:]' '[:lower:]')"
ARCH="$(uname -m)"

case "$ARCH" in
  x86_64) ARCH="x86_64" ;;
  arm64|aarch64) ARCH="arm64" ;;
  *) echo "Unsupported architecture: $ARCH"; exit 1 ;;
esac

case "$OS" in
  darwin) PLATFORM="macos" ;;
  linux) PLATFORM="linux" ;;
  *) echo "Unsupported operating system: $OS"; exit 1 ;;
esac

ASSET="snip-${PLATFORM}-${ARCH}.tar.gz"
URL="https://github.com/${REPO}/releases/download/${VERSION}/${ASSET}"
INSTALL_DIR="${HOME}/.local/bin"

echo "==> Downloading snip ${VERSION} for ${PLATFORM}-${ARCH}..."
mkdir -p "${INSTALL_DIR}"
curl -sSL "${URL}" | tar -xz -C "${INSTALL_DIR}"

chmod +x "${INSTALL_DIR}/snip"

echo "==> Successfully installed snip into ${INSTALL_DIR}/snip"

# Check PATH
if [[ ":$PATH:" != *":${INSTALL_DIR}:"* ]]; then
  echo ""
  echo "Add snip to your PATH by adding this to your ~/.zshrc or ~/.bashrc:"
  echo "  export PATH=\"\$HOME/.local/bin:\$PATH\""
fi

echo ""
echo "Verify installation:"
echo "  snip gain"
echo "  snip init --claude"