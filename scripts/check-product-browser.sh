#!/usr/bin/env bash
set -euo pipefail

profile="${1:?usage: check-product-browser.sh <generated-archiso-profile>}"
root="$profile/airootfs"
packages="$profile/packages.x86_64"

required_packages=(
  ca-certificates
  ca-certificates-mozilla
  chromium
  gnome-keyring
  sof-firmware
  mesa-utils
  ffmpeg
  intel-media-driver
  libsecret
  libva-intel-driver
  libva-utils
  mesa
  networkmanager
  pipewire
  pipewire-alsa
  pipewire-audio
  pipewire-pulse
  vulkan-intel
  vulkan-radeon
  wireplumber
)

for package in "${required_packages[@]}"; do
  grep -qxF "$package" "$packages" || {
    echo "browser check: missing package $package" >&2
    exit 1
  }
done

launcher="$root/usr/local/bin/josh-os-browser"
session="$root/usr/local/bin/josh-os-session"
persist="$root/usr/local/lib/josh-os/prepare-persistent-home"
media_probe="$root/usr/local/lib/josh-os/browser-media-probe"
media_page="$root/usr/local/share/josh-os/browser-media-probe.html"
policy="$root/etc/chromium/policies/managed/josh-os.json"

for path in "$launcher" "$session" "$persist" "$media_probe" "$media_page" "$policy"; do
  [[ -f "$path" ]] || { echo "browser check: missing $path" >&2; exit 1; }
done

grep -q -- '--password-store=gnome-libsecret' "$launcher"
grep -q -- '--restore-last-session' "$launcher"
if grep -Eq -- '^[[:space:]]*--no-sandbox([[:space:]]|$)' "$launcher"; then
  echo "browser check: launcher must not disable Chromium sandbox" >&2
  exit 1
fi
if grep -Eq -- '^[[:space:]]*--disable-gpu([[:space:]]|$)' "$launcher"; then
  echo "browser check: launcher must not disable GPU acceleration" >&2
  exit 1
fi

grep -q 'JOSH-DATA' "$persist"
grep -q 'ext4' "$persist"
grep -q '"DownloadDirectory": "/home/josh/Downloads"' "$policy"
grep -q '"RestoreOnStartup": 1' "$policy"
grep -q '"PasswordManagerEnabled": false' "$policy"
grep -q 'pipewire-pulse.service' "$session"
grep -q 'gnome-keyring-daemon' "$session"
grep -q 'josh-os-browser chrome://newtab' "$session"

[[ -L "$root/etc/systemd/system/multi-user.target.wants/josh-os-persistence.service" ]]
[[ -L "$root/etc/systemd/system/multi-user.target.wants/NetworkManager.service" ]]

echo "Josh OS product browser profile checks passed."
