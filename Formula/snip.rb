class Snip < Formula
  desc "High-performance C++ token pruning engine and MCP server for AI coding agents"
  homepage "https://github.com/Tabhi109/snip"
  version "0.2.1"
  license "MIT"

  on_macos do
    if Hardware::CPU.arm?
      url "https://github.com/Tabhi109/snip/releases/download/v0.2.1/snip-macos-arm64.tar.gz"
      sha256 "dd118301320064e33d8f95fd946fe99dcc907fcec10e994c763cd296bfe3af40"
    else
      url "https://github.com/Tabhi109/snip/releases/download/v0.2.1/snip-macos-x86_64.tar.gz"
      sha256 "f25a14f471f4118f2e9d29ef583e89c3a4cbf0ccbf90f54b1124bf2a83b6ffed"
    end
  end

  on_linux do
    url "https://github.com/Tabhi109/snip/releases/download/v0.2.1/snip-linux-x86_64.tar.gz"
    sha256 "d41f0327f4d6bd0e56b1d9160da8b7f436b4db35012c3201773f26777b74d342"
  end

  def install
    bin.install "snip"
  end

  test do
    system "#{bin}/snip", "gain"
  end
end
