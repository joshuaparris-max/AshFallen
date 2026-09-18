#!/usr/bin/env bash
set -euo pipefail

iso="${1:?usage: smoke-product-browser.sh <josh-os.iso> [data-image]}"
data_image="${2:-browser-persistence-test.img}"
qemu="${QEMU:-qemu-system-x86_64}"
timeout_seconds="${BROWSER_SMOKE_TIMEOUT:-210}"

command -v "$qemu" >/dev/null
command -v mkfs.ext4 >/dev/null

rm -f "$data_image"
truncate -s 512M "$data_image"
mkfs.ext4 -F -L JOSH-DATA "$data_image" >/dev/null

run_boot() {
  local pass="$1"
  local persistence_marker="$2"
  local secret_marker="$3"
  local log="browser-boot-${pass}.log"
  rm -f "$log"

  "$qemu"     -machine q35     -m 2048     -boot order=d     -cdrom "$iso"     -drive file="$data_image",format=raw,if=virtio     -nic user,model=e1000     -audiodev none,id=josh-audio     -device intel-hda     -device hda-duplex,audiodev=josh-audio     -display none     -serial "file:$log"     -monitor none     -no-reboot     >/dev/null 2>&1 &
  local pid=$!

  cleanup() {
    if kill -0 "$pid" >/dev/null 2>&1; then
      kill "$pid" >/dev/null 2>&1 || true
      wait "$pid" 2>/dev/null || true
    fi
  }
  trap cleanup RETURN

  for _ in $(seq 1 "$timeout_seconds"); do
    if grep -q 'JOSHOS_BROWSER_READY' "$log" 2>/dev/null &&
       grep -q 'JOSHOS_BROWSER_PROFILE_READY' "$log" 2>/dev/null &&
       grep -q 'JOSHOS_BROWSER_MEDIA_OK' "$log" 2>/dev/null &&
       grep -q 'JOSHOS_NETWORK_READY' "$log" 2>/dev/null &&
       grep -q "$secret_marker" "$log" 2>/dev/null &&
       grep -q "$persistence_marker" "$log" 2>/dev/null; then
      cleanup
      trap - RETURN
      echo "Product browser + internet boot pass $pass succeeded ($persistence_marker)."
      return 0
    fi
    if ! kill -0 "$pid" >/dev/null 2>&1; then
      echo "Product browser boot pass $pass exited before readiness." >&2
      cat "$log" >&2 || true
      trap - RETURN
      return 1
    fi
    sleep 1
  done

  echo "Product browser boot pass $pass timed out." >&2
  cat "$log" >&2 || true
  return 1
}

run_boot 1 JOSHOS_BROWSER_PERSISTENCE_PRIMED JOSHOS_SECRET_STORE_PRIMED
run_boot 2 JOSHOS_BROWSER_PERSISTENCE_OK JOSHOS_SECRET_STORE_OK

echo "Josh OS product browser + internet two-boot persistence smoke test passed."
