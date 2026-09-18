#!/usr/bin/env bash
set -euo pipefail

profile="${1:?usage: check-network-platform.sh <generated-archiso-profile>}"
root="$profile/airootfs"
packages="$profile/packages.x86_64"

required_packages=(
  ca-certificates
  ca-certificates-mozilla
  curl
  iproute2
  iw
  linux-firmware
  networkmanager
  openssl
  python
  wireless-regdb
  wpa_supplicant
)

for package in "${required_packages[@]}"; do
  grep -qxF "$package" "$packages" || {
    echo "network check: missing package $package" >&2
    exit 1
  }
done

required_files=(
  "$root/etc/NetworkManager/conf.d/10-josh-os.conf"
  "$root/etc/systemd/resolved.conf.d/10-josh-os.conf"
  "$root/usr/local/bin/josh-network-check"
  "$root/usr/local/bin/josh-wifi"
  "$root/usr/local/lib/josh-os/network-control.py"
  "$root/usr/lib/systemd/system/josh-network-control.service"
  "$root/usr/local/lib/josh-os/network-ready-probe"
  "$root/usr/lib/systemd/system/josh-network-ready.service"
  "$root/etc/NetworkManager/conf.d/20-josh-resilience.conf"
)

for path in "${required_files[@]}"; do
  [[ -f "$path" ]] || {
    echo "network check: missing $path" >&2
    exit 1
  }
done

grep -q '^dns=systemd-resolved$' "$root/etc/NetworkManager/conf.d/10-josh-os.conf"
grep -q 'https://www.youtube.com/' "$root/usr/local/bin/josh-network-check"
grep -q 'nmcli --ask device wifi connect' "$root/usr/local/bin/josh-wifi"
grep -q 'autoconnect-retries-default=0' "$root/etc/NetworkManager/conf.d/20-josh-resilience.conf"
grep -q '/api/network/connect' "$root/usr/local/lib/josh-os/network-control.py"

declare -A required_links=(
  ["$root/etc/systemd/system/multi-user.target.wants/NetworkManager.service"]="/usr/lib/systemd/system/NetworkManager.service"
  ["$root/etc/systemd/system/network-online.target.wants/NetworkManager-wait-online.service"]="/usr/lib/systemd/system/NetworkManager-wait-online.service"
  ["$root/etc/systemd/system/multi-user.target.wants/josh-network-control.service"]="/usr/lib/systemd/system/josh-network-control.service"
  ["$root/etc/systemd/system/multi-user.target.wants/josh-network-ready.service"]="/usr/lib/systemd/system/josh-network-ready.service"
  ["$root/etc/systemd/system/multi-user.target.wants/systemd-resolved.service"]="/usr/lib/systemd/system/systemd-resolved.service"
  ["$root/etc/systemd/system/multi-user.target.wants/systemd-timesyncd.service"]="/usr/lib/systemd/system/systemd-timesyncd.service"
  ["$root/etc/resolv.conf"]="/run/systemd/resolve/stub-resolv.conf"
)

for path in "${!required_links[@]}"; do
  [[ -L "$path" ]] || {
    echo "network check: expected symlink $path" >&2
    exit 1
  }
  target="$(readlink "$path")"
  [[ "$target" == "${required_links[$path]}" ]] || {
    echo "network check: $path points to $target, expected ${required_links[$path]}" >&2
    exit 1
  }
done

for path in   "$root/etc/systemd/system/multi-user.target.wants/systemd-networkd.service"   "$root/etc/systemd/system/network-online.target.wants/systemd-networkd-wait-online.service"   "$root/etc/systemd/system/multi-user.target.wants/iwd.service"; do
  if [[ -e "$path" || -L "$path" ]]; then
    echo "network check: competing network service still enabled: $path" >&2
    exit 1
  fi
done

echo "Josh OS Stage 0 network platform profile checks passed."
