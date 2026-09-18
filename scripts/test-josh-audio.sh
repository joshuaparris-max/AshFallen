#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

cat >"$tmp/wpctl" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' "$*" >>"$JOSH_AUDIO_TEST_LOG"
case "${1:-}" in
  get-volume) echo "Volume: 0.50" ;;
  status) echo "Audio devices available" ;;
esac
EOF
chmod +x "$tmp/wpctl"

export JOSH_WPCTL="$tmp/wpctl"
export JOSH_AUDIO_TEST_LOG="$tmp/calls"
audio="$ROOT/iso/overlay/usr/local/bin/josh-audio"

"$audio" status >/dev/null
"$audio" volume 42
"$audio" up
"$audio" down 7
"$audio" toggle
"$audio" mute
"$audio" unmute
"$audio" mic-toggle
"$audio" devices >/dev/null
"$audio" default 71

grep -Fxq 'get-volume @DEFAULT_AUDIO_SINK@' "$JOSH_AUDIO_TEST_LOG"
grep -Fxq 'get-volume @DEFAULT_AUDIO_SOURCE@' "$JOSH_AUDIO_TEST_LOG"
grep -Fxq 'set-volume --limit 1.0 @DEFAULT_AUDIO_SINK@ 42%' "$JOSH_AUDIO_TEST_LOG"
grep -Fxq 'set-volume --limit 1.0 @DEFAULT_AUDIO_SINK@ 5%+' "$JOSH_AUDIO_TEST_LOG"
grep -Fxq 'set-volume --limit 1.0 @DEFAULT_AUDIO_SINK@ 7%-' "$JOSH_AUDIO_TEST_LOG"
grep -Fxq 'set-mute @DEFAULT_AUDIO_SINK@ toggle' "$JOSH_AUDIO_TEST_LOG"
grep -Fxq 'set-mute @DEFAULT_AUDIO_SOURCE@ toggle' "$JOSH_AUDIO_TEST_LOG"
grep -Fxq 'status --name' "$JOSH_AUDIO_TEST_LOG"
grep -Fxq 'set-default 71' "$JOSH_AUDIO_TEST_LOG"

if "$audio" volume 101 >/dev/null 2>&1; then
  echo "josh-audio accepted invalid volume" >&2
  exit 1
fi
if "$audio" down nope >/dev/null 2>&1; then
  echo "josh-audio accepted invalid step" >&2
  exit 1
fi
if "$audio" default abc >/dev/null 2>&1; then
  echo "josh-audio accepted invalid node ID" >&2
  exit 1
fi

echo "Josh audio control tests passed."
