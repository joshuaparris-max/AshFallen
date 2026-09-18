#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARCHISO_PROFILE="${ARCHISO_PROFILE:-/usr/share/archiso/configs/releng}"
WORK_DIR="${WORK_DIR:-$ROOT/work}"
OUT_DIR="${OUT_DIR:-$ROOT/out}"
GENERATED_PROFILE="${GENERATED_PROFILE:-$WORK_DIR/josh-os-profile}"
PREPARE_ONLY=0

usage() {
  cat <<USAGE
Usage: $0 [--prepare-only]

Build a bootable Josh OS live ISO using Arch's current releng profile as the
bootable base, then layer the Josh OS shell and live-session configuration on
top.

Environment overrides:
  ARCHISO_PROFILE  Base profile (default: /usr/share/archiso/configs/releng)
  WORK_DIR         mkarchiso working directory (default: ./work)
  OUT_DIR          ISO output directory (default: ./out)
  GENERATED_PROFILE Generated profile path (default: ./work/josh-os-profile)
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --prepare-only) PREPARE_ONLY=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "build-iso: unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

if [[ ! -d "$ARCHISO_PROFILE" ]]; then
  echo "build-iso: ArchISO releng profile not found at $ARCHISO_PROFILE" >&2
  echo "Install archiso first (Arch Linux: pacman -S archiso), or set ARCHISO_PROFILE." >&2
  exit 1
fi

rm -rf "$GENERATED_PROFILE"
mkdir -p "$(dirname "$GENERATED_PROFILE")"
cp -a "$ARCHISO_PROFILE" "$GENERATED_PROFILE"

packages=(
  chromium
  lightdm
  lightdm-gtk-greeter
  mesa
  noto-fonts
  openbox
  ttf-dejavu
  xorg-server
  xorg-xrandr
  xorg-xset
  xorg-xsetroot
)

for package in "${packages[@]}"; do
  grep -qxF "$package" "$GENERATED_PROFILE/packages.x86_64" || echo "$package" >> "$GENERATED_PROFILE/packages.x86_64"
done

cp -a "$ROOT/iso/overlay/." "$GENERATED_PROFILE/airootfs/"

mkdir -p "$GENERATED_PROFILE/airootfs/opt/josh-os"
cp -a "$ROOT/shell" "$GENERATED_PROFILE/airootfs/opt/josh-os/"
cp -a "$ROOT/design" "$GENERATED_PROFILE/airootfs/opt/josh-os/"

version="dev"
if command -v git >/dev/null 2>&1 && git -C "$ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  version="$(git -C "$ROOT" rev-parse --short HEAD)"
fi
cat > "$GENERATED_PROFILE/airootfs/etc/josh-os-release" <<RELEASE
NAME="Josh OS"
STAGE="0-live"
BUILD="$version"
BASE="Arch Linux / archiso"
RELEASE

cat >> "$GENERATED_PROFILE/profiledef.sh" <<'PROFILE'

# Josh OS overrides. Keep these after the upstream releng profile definitions.
iso_name="josh-os"
iso_publisher="Josh OS <https://github.com/joshuaparris-max/AshFallen>"
iso_application="Josh OS Stage 0 Live"
file_permissions["/usr/local/bin/josh-os-session"]="0:0:0755"
file_permissions["/etc/sudoers.d/10-josh-os-live"]="0:0:0440"
PROFILE

mkdir -p "$GENERATED_PROFILE/airootfs/etc/systemd/system/multi-user.target.wants"
ln -sfn /usr/lib/systemd/system/graphical.target "$GENERATED_PROFILE/airootfs/etc/systemd/system/default.target"
ln -sfn /usr/lib/systemd/system/lightdm.service "$GENERATED_PROFILE/airootfs/etc/systemd/system/display-manager.service"
ln -sfn /usr/lib/systemd/system/vboxservice.service "$GENERATED_PROFILE/airootfs/etc/systemd/system/multi-user.target.wants/vboxservice.service"

while IFS= read -r -d '' cfg; do
  sed -i     -e 's/Arch Linux install medium/Josh OS live/g'     -e 's/Arch Linux/Josh OS/g'     "$cfg"
done < <(find "$GENERATED_PROFILE" -type f \( -name '*.cfg' -o -name '*.conf' \) -print0)

if [[ "$PREPARE_ONLY" -eq 1 ]]; then
  echo "Prepared ArchISO profile: $GENERATED_PROFILE"
  exit 0
fi

if ! command -v mkarchiso >/dev/null 2>&1; then
  echo "build-iso: mkarchiso is not installed (install the archiso package)." >&2
  exit 1
fi

mkdir -p "$OUT_DIR"
rm -rf "$WORK_DIR/mkarchiso"
mkarchiso -v -r -w "$WORK_DIR/mkarchiso" -o "$OUT_DIR" "$GENERATED_PROFILE"

echo
printf 'Josh OS ISO ready:\n'
find "$OUT_DIR" -maxdepth 1 -type f -name 'josh-os-*.iso' -print
