#!/bin/bash
# Build and test the native C applications without starting a desktop session.
set -euo pipefail
cd -- "$(dirname -- "$0")"
make build test "$@"
