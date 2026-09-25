class Snip < Formula
    desc "High-performance C++ token pruning engine and MCP server for AI coding agents"
    homepage "https://github.com/your-username/snip"
    version "0.2.0"
    license "Apache-2.0"
  
    on_macos do
      if Hardware::CPU.arm?
        url "https://github.com/your-username/snip/releases/download/v0.2.0/snip-macos-arm64.tar.gz"
        sha256 "REPLACE_WITH_SHA256_ARM64"
      else
        url "https://github.com/your-username/snip/releases/download/v0.2.0/snip-macos-x86_64.tar.gz"
        sha256 "REPLACE_WITH_SHA256_X86_64"
      end
    end
  
    on_linux do
      url "https://github.com/your-username/snip/releases/download/v0.2.0/snip-linux-x86_64.tar.gz"
      sha256 "REPLACE_WITH_SHA256_LINUX"
    end
  
    def install
      bin.install "snip"
    end
  
    test do
      system "#{bin}/snip", "gain"
    end
  end