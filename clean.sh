#!/bin/bash
# clean.sh - Dọn dẹp build artifacts
set -e
echo "=== Clean ==="
rm -f /tmp/gotiengviet-demo /tmp/ibus-setup-gotiengviet /tmp/ibus-engine-gotiengviet /tmp/ibus-engine-gotiengviet-go
rm -f /tmp/gotiengviet_*.deb
rm -rf /tmp/gotiengviet-deb
rm -f gotiengviet_*.deb
rm -f ibus/*.pyc 2>/dev/null || true
echo "  dọn /tmp/*gotiengviet*"
echo "=== Clean xong ==="
