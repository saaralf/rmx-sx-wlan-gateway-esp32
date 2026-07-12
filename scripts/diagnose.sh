#!/usr/bin/env bash
# diagnose.sh — collect a diagnostic bundle for the gateway.
# Dumps service, AP, serial, and recent logs into a timestamped directory.
set -euo pipefail

SERVICE_NAME="rmx-sx-gateway"
OUT="/tmp/rmx-sx-diag-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUT"

echo "Writing diagnostics to $OUT"

{
  echo "=== systemctl status ==="
  systemctl status "$SERVICE_NAME" --no-pager -l 2>&1 | head -30
  echo "=== port 8080 ==="
  ss -ltnp 2>/dev/null | grep ':8080' || echo "not listening"
  echo "=== hostapd ==="
  systemctl status hostapd --no-pager -l 2>&1 | head -10
  echo "=== dnsmasq ==="
  systemctl status dnsmasq --no-pager -l 2>&1 | head -10
} > "$OUT/service.txt" 2>&1

{
  ls -l /dev/serial/by-id/ 2>/dev/null || echo "no by-id"
  echo "--- lsusb ---"
  lsusb 2>/dev/null || echo "no lsusb"
} > "$OUT/interfaces.txt" 2>&1

journalctl -u "$SERVICE_NAME" -n 200 --no-pager > "$OUT/journal.log" 2>&1 || \
  cp /var/log/rmx-sx-gateway/rmx-sx-gateway.log "$OUT/gateway.log" 2>/dev/null || true

echo "=== Diagnostics written to $OUT ==="
ls -l "$OUT"
echo "Share this directory when reporting issues."
