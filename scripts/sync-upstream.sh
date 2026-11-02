#!/usr/bin/env bash
# satorino: sync upstream (leafyzito/leafyrino) into the satorino branch.
#
# Strategy (approved 2026-11-01): MERGE, never rebase.
#  - merging keeps upstream commit SHAs identical to leafyrino's, so
#    `git log leafyrino/leafyrino..satorino` shows exactly our delta and
#    format-patch series stay stable across syncs;
#  - rerere is enabled so conflict resolutions are recorded once and
#    replayed automatically on every later sync.
#
# Usage:            scripts/sync-upstream.sh
# Override remote:  SATORINO_UPSTREAM_REMOTE / SATORINO_UPSTREAM_URL
# Override branch:  SATORINO_UPSTREAM_BRANCH (default: leafyrino)
#                   SATORINO_BRANCH          (default: satorino)

set -euo pipefail

REMOTE_NAME="${SATORINO_UPSTREAM_REMOTE:-leafyrino}"
REMOTE_URL="${SATORINO_UPSTREAM_URL:-https://github.com/leafyzito/leafyrino.git}"
REMOTE_BRANCH="${SATORINO_UPSTREAM_BRANCH:-leafyrino}"
WORK_BRANCH="${SATORINO_BRANCH:-satorino}"

cd "$(git rev-parse --show-toplevel)"

# 1. rerere: remember + replay merge conflict resolutions
git config rerere.enabled true

# 2. make sure the upstream remote exists
if ! git remote get-url "$REMOTE_NAME" >/dev/null 2>&1; then
    echo ">> adding remote '$REMOTE_NAME' -> $REMOTE_URL"
    git remote add "$REMOTE_NAME" "$REMOTE_URL"
fi

# 3. never sync on a dirty tree
if [ -n "$(git status --porcelain --untracked-files=no)" ]; then
    echo "!! working tree is not clean - commit or stash first" >&2
    exit 1
fi

# 4. fetch + merge (not rebase)
git fetch "$REMOTE_NAME" "$REMOTE_BRANCH"
git checkout "$WORK_BRANCH"

if ! git merge --no-edit "$REMOTE_NAME/$REMOTE_BRANCH"; then
    cat <<'EOF'
>> merge CONFLICTED.
   - resolve the conflicts, `git add <files>`, then `git commit`
   - rerere has already recorded your resolutions - identical hunks
     resolve themselves on the next sync
   - finish with: git submodule update --init --recursive
   - then re-run this script; it should end with "Already up to date."
EOF
    exit 1
fi

# 5. submodule pointers move with the merge
git submodule update --init --recursive

# 6. post-sync checks (details in docs/SATORINO.md)
cat <<'EOF'
>> sync done. Post-sync checklist (docs/SATORINO.md):
   1. git grep -n satorino     - every `satorino:` hook still present?
   2. git grep -in leafyrino   - only the allowed leftovers?
   3. re-check the hot files list by hand
   4. rebuild + smoke test BEFORE pushing:
        cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
              -DCHATTERINO_NO_AVIF_PLUGIN=On -DCHATTERINO_UPDATER=Off
        cmake --build build -j"$(nproc)"
EOF
