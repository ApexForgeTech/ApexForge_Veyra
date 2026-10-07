#!/usr/bin/env bash
# Fetches the official precompiled Tor binaries (the same tor-expert-bundle that
# Tor Browser ships) and vendors them so Veyra can bundle/launch its own Tor —
# desktop (vendor/routers/) and Android (jniLibs/). NOT compiled from source.
#
#   scripts/fetch-routers.sh           # desktop + android
#   scripts/fetch-routers.sh desktop   # just linux-x86_64
#   scripts/fetch-routers.sh android   # just the 4 android ABIs
#
# Re-run to update. Output dirs are git-ignored (binaries are large).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VER="${TOR_BUNDLE_VERSION:-15.0.15}"
BASE="https://archive.torproject.org/tor-package-archive/torbrowser/$VER"
DL="$ROOT/vendor/dl"; mkdir -p "$DL"
WHAT="${1:-all}"

fetch() { # url dest
  echo "↓ $(basename "$1")"
  curl -fsSL --retry 3 --max-time 300 -o "$2" "$1"
}

if [[ "$WHAT" == "all" || "$WHAT" == "desktop" ]]; then
  f="$DL/tor-expert-bundle-linux-x86_64-$VER.tar.gz"
  fetch "$BASE/tor-expert-bundle-linux-x86_64-$VER.tar.gz" "$f"
  dest="$ROOT/vendor/routers/linux-x86_64"; mkdir -p "$dest"
  tar xzf "$f" -C "$dest"
  echo "✓ desktop tor → $dest/tor/tor"
fi

if [[ "$WHAT" == "all" || "$WHAT" == "android" ]]; then
  declare -A MAP=( [aarch64]=arm64-v8a [armv7]=armeabi-v7a [x86]=x86 [x86_64]=x86_64 )
  for arch in aarch64 armv7 x86 x86_64; do
    abi="${MAP[$arch]}"
    f="$DL/tor-expert-bundle-android-$arch-$VER.tar.gz"
    fetch "$BASE/tor-expert-bundle-android-$arch-$VER.tar.gz" "$f"
    tmp="$(mktemp -d)"; tar xzf "$f" -C "$tmp"
    dest="$ROOT/android/app/src/main/jniLibs/$abi"; mkdir -p "$dest"
    cp "$tmp/tor/libTor.so" "$dest/libTor.so"
    [[ -f "$tmp/tor/pluggable_transports/lyrebird" ]] && \
      cp "$tmp/tor/pluggable_transports/lyrebird" "$dest/liblyrebird.so"
    rm -rf "$tmp"
    echo "✓ android $abi → $dest/libTor.so"
  done
fi

# ── I2P (i2pd) — bundled the same way, so I2P works with no system install ──
# Desktop: a copy of the i2pd executable + its reseed/family certificates.
# Android: the official per-ABI i2pd executables from PurpleI2P/i2pd-android,
# vendored as jniLibs/<abi>/libi2pd.so (exec'd from nativeLibraryDir, like Tor).
I2PD_VER="${I2PD_ANDROID_VERSION:-2.60.0}"
if [[ "$WHAT" == "all" || "$WHAT" == "android" ]]; then
  f="$DL/i2pd_${I2PD_VER}_android_binary.zip"
  fetch "https://github.com/PurpleI2P/i2pd-android/releases/download/$I2PD_VER/i2pd_${I2PD_VER}_android_binary.zip" "$f"
  tmp="$(mktemp -d)"; unzip -qo "$f" -d "$tmp"
  declare -A IMAP=( [aarch64]=arm64-v8a [armv7l]=armeabi-v7a [x86]=x86 [x86_64]=x86_64 )
  for arch in aarch64 armv7l x86 x86_64; do
    abi="${IMAP[$arch]}"
    dest="$ROOT/android/app/src/main/jniLibs/$abi"; mkdir -p "$dest"
    cp "$tmp/i2pd-$arch" "$dest/libi2pd.so"
    echo "✓ android $abi → $dest/libi2pd.so"
  done
  # Reseed/family certs ship in app assets; i2pd seeds them into its datadir.
  rm -rf "$ROOT/android/app/src/main/assets/i2pd/certificates"
  mkdir -p "$ROOT/android/app/src/main/assets/i2pd"
  cp -r "$tmp/certificates" "$ROOT/android/app/src/main/assets/i2pd/certificates"
  rm -rf "$tmp"
  echo "✓ android i2pd certificates → assets/i2pd/certificates"
fi
if [[ "$WHAT" == "all" || "$WHAT" == "desktop" ]]; then
  # Prefer a system i2pd if present; copy it (and the system certs) into the
  # bundle so it runs unconfined (Debian's apt i2pd is AppArmor-confined).
  if command -v i2pd >/dev/null 2>&1; then
    dest="$ROOT/vendor/routers/linux-x86_64/i2pd"; mkdir -p "$dest"
    cp "$(command -v i2pd)" "$dest/i2pd"; chmod +x "$dest/i2pd"
    [[ -d /usr/share/i2pd/certificates ]] && cp -r /usr/share/i2pd/certificates "$dest/certificates"
    echo "✓ desktop i2pd → $dest/i2pd"
  else
    echo "ℹ desktop i2pd: install with 'apt install i2pd' then re-run, or drop a"
    echo "  portable binary at vendor/routers/linux-x86_64/i2pd/i2pd"
  fi
fi

echo "Done."
