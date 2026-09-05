#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${project_root}"

if [[ ! -f build/prod/CMakeCache.txt ]]; then
    cmake --preset prod
fi

# An incremental build is effectively free when nothing changed, and prevents
# an existing production binary from silently falling behind the source tree.
cmake --build --preset prod

exec build/prod/offgrid-assistant "$@"
