#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JOSH_DEV="$ROOT/iso/overlay/usr/local/bin/josh-dev"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

export HOME="$TMP/home"
export JOSH_DEV_CONFIG_DIR="$TMP/config"
export JOSH_DEV_STATE_DIR="$TMP/state"
export JOSH_DEV_REPOS_FILE="$TMP/repos.conf"
export JOSH_DEV_RULES_FILE="$ROOT/iso/overlay/etc/josh-os/developer-agent-rules.md"
mkdir -p "$HOME" "$TMP/bin"
export PATH="$TMP/bin:$PATH"
export JOSH_DEV_TEST_CALL="$TMP/toolchain-call"
cat > "$TMP/bin/pacman" <<'SH'
#!/usr/bin/env bash
exit 0
SH
cat > "$TMP/bin/sudo" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
printf '%s\n' "$*" > "$JOSH_DEV_TEST_CALL"
SH
chmod +x "$TMP/bin/pacman" "$TMP/bin/sudo"

bash -n "$JOSH_DEV"
bash "$JOSH_DEV" --help >/dev/null
bash "$JOSH_DEV" enable --ephemeral --workspace "$TMP/workspace" >/dev/null
status_output="$(bash "$JOSH_DEV" status)"
grep -q 'Developer Mode: enabled' <<< "$status_output"
bash "$JOSH_DEV" install-toolchain >/dev/null
grep -q 'pacman -Syu --needed' "$JOSH_DEV_TEST_CALL"
grep -q 'archiso' "$JOSH_DEV_TEST_CALL"
grep -q 'qemu-system-x86' "$JOSH_DEV_TEST_CALL"

# Create a tiny local Stage0-shaped remote so sync/improve can be tested without
# network access and without trusting documentation as proof of behaviour.
mkdir -p "$TMP/source/scripts"
git -C "$TMP/source" init -q -b main
git -C "$TMP/source" config user.name 'Josh OS Test'
git -C "$TMP/source" config user.email 'test@joshos.invalid'
printf 'original\n' > "$TMP/source/value.txt"
cat > "$TMP/source/scripts/build-tokens.mjs" <<'NODE'
console.log('tokens fixture ok');
NODE
cat > "$TMP/source/scripts/build-shell.mjs" <<'NODE'
console.log('shell fixture ok');
NODE
cat > "$TMP/source/scripts/build-iso.sh" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
exit 0
SH
chmod +x "$TMP/source/scripts/build-iso.sh"
git -C "$TMP/source" add .
git -C "$TMP/source" commit -qm 'initial'
git clone -q --bare "$TMP/source" "$TMP/origin.git"
printf 'stage0|linux-product-prototype|file://%s\n' "$TMP/origin.git" > "$JOSH_DEV_REPOS_FILE"

bash "$JOSH_DEV" sync stage0 >/dev/null
[[ -f "$TMP/workspace/repos/stage0/value.txt" ]]

if bash "$JOSH_DEV" sync does-not-exist >/dev/null 2>&1; then
  echo 'sync should reject an unknown repository key' >&2
  exit 1
fi

# Fake Codex only for orchestration testing: the real ISO installs Codex from
# OpenAI's official installer at user request.
cat > "$TMP/bin/codex" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
if [[ "${1:-}" == "--version" ]]; then
  echo 'codex fixture'
  exit 0
fi
printf 'changed by fixture agent\n' >> value.txt
SH
chmod +x "$TMP/bin/codex"
improve_output="$(bash "$JOSH_DEV" improve stage0 --agent codex -- 'make a tested change')"
grep -q 'VERIFIED TASK:' <<< "$improve_output"
grep -q 'changed by fixture agent' "$TMP/workspace"/worktrees/stage0/*/value.txt
list_output="$(bash "$JOSH_DEV" list)"
grep -q 'verified' <<< "$list_output"

bash "$JOSH_DEV" disable >/dev/null
if bash "$JOSH_DEV" sync stage0 >/dev/null 2>&1; then
  echo 'sync should fail when Developer Mode is disabled' >&2
  exit 1
fi

echo 'developer-mode tests: PASS'
