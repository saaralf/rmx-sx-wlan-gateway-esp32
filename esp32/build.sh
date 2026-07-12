#!/usr/bin/env bash
# ============================================================================
# build.sh — ESP32-Firmware bauen/flashen OHNE dass das WLAN-Passwort im Repo
# steht. Das Passwort wird aus ~/.config/wlanhandregler/secrets.env gelesen
# (eine einzige, lokal gespeicherte Quelle) und nur dem Compiler via
# -D WIFI_PASSWORD=... übergeben.
#
# Nutzung:
#   ./build.sh            # nur bauen
#   ./build.sh flash      # bauen + flashen (esptool --after no_reset)
#   ./build.sh monitor    # seriellen Monitor (Vorsicht: belegt /dev/ttyUSB0)
#
# Voraussetzung: secrets.env existiert mit WIFI_SSID + WIFI_PASSWORD.
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SECRETS="${WLANHANDREGLER_SECRETS:-$HOME/.config/wlanhandregler/secrets.env}"

if [[ ! -f "$SECRETS" ]]; then
  echo "FEHLER: $SECRETS nicht gefunden." >&2
  echo "Erstelle sie z.B. so:" >&2
  echo "  mkdir -p ~/.config/wlanhandregler" >&2
  echo "  printf 'WIFI_SSID=\"Modellbahn-Fahrregler\"\nWIFI_PASSWORD=\"DEIN_PASSWORT\"\n' > ~/.config/wlanhandregler/secrets.env" >&2
  echo "  chmod 600 ~/.config/wlanhandregler/secrets.env" >&2
  exit 1
fi

# secrets.env laden (nur WIFI_SSID / WIFI_PASSWORD)
set -a
# shellcheck disable=SC1090
source "$SECRETS"
set +a

if [[ -z "${WIFI_PASSWORD:-}" ]]; then
  echo "FEHLER: WIFI_PASSWORD in $SECRETS leer." >&2
  exit 1
fi

# build_flags um Passwort ergaenzen (PlattformIO erwartet -D NAME=\"wert\")
EXTRA_FLAGS=(-D "WIFI_PASSWORD=\\\"${WIFI_PASSWORD}\\\"")
if [[ -n "${WIFI_SSID:-}" ]]; then
  EXTRA_FLAGS+=(-D "WIFI_SSID=\\\"${WIFI_SSID}\\\"")
fi

export PATH="$HOME/.local/bin:$PATH"
cd "$SCRIPT_DIR"

TARGET=""
if [[ "${1:-}" == "flash" ]]; then
  TARGET="upload"
elif [[ "${1:-}" == "monitor" ]]; then
  TARGET="monitor"
fi

# Bauen
echo "==> pio run ${TARGET:+(target: $TARGET)}"
if [[ -n "$TARGET" ]]; then
  if [[ "$TARGET" == "upload" ]]; then
    # Flashen ueber esptool --after no_reset, damit der CYD nicht im
    # Download-Mode haengen bleibt (PITFALL 1). Danach USB-Power-Cycle.
    pio run -t upload --upload-port "${UPLOAD_PORT:-/dev/ttyUSB0}"
  else
    pio run -t "$TARGET"
  fi
else
  pio run
fi

echo "==> fertig."
