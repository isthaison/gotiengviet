#!/bin/bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
# Only remove the default build tree; keep packaged releases and system files.
rm -rf -- build
