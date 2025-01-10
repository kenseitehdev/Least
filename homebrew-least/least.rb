
class Least < Formula
  desc "A Lightweight less Alternative for Code Browsing"
  homepage "https://github.com/kenseitehdev/Least"
  url "https://github.com/kenseitehdev/Least/releases/download/0.2/least-0.2.tar.gz"
  sha256 "7bd0174d90a33de76cb4554506dc593a4a5844f3e6fec5b6cb881b158b22a66f"
  license "MIT"

  depends_on "ncurses"

  def install
    system "make"
    bin.install "least"
  end

  test do
    system "#{bin}/least", "--version"
  end
end
