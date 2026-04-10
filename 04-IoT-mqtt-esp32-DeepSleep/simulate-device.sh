#!/bin/sh
# ─────────────────────────────────────────────────────────────────
# simulate-device.sh — Simulates an ESP32 deep-sleep device
# publishing sensor data on each wake cycle.
#
# Unlike the always-on examples, this script publishes one burst
# of telemetry per cycle (simulating a single wake), then sleeps.
#
# Usage:
#   ./simulate-device.sh                  # uses defaults (5 min interval)
#   ./simulate-device.sh AABBCCDDEEFF     # custom MAC address
#   ./simulate-device.sh AABBCCDDEEFF 30  # custom MAC + 30s interval
#
# Requirements: mosquitto_pub (install with brew install mosquitto
#               or apt install mosquitto-clients)
# ─────────────────────────────────────────────────────────────────

BROKER="control.aut.utcluj.ro"
PORT=11188
MAC="${1:-AABBCCDDEEFF}"
INTERVAL="${2:-300}"
MAC_FORMATTED="$(echo "$MAC" | sed 's/\(..\)/\1:/g; s/:$//')"
IP="192.168.1.100"
CHANNEL=6
BOOT=0

# Check that mosquitto_pub is available
if ! command -v mosquitto_pub > /dev/null 2>&1; then
    echo "Error: mosquitto_pub not found."
    echo "Install it with:"
    echo "  macOS:  brew install mosquitto"
    echo "  Linux:  sudo apt install mosquitto-clients"
    exit 1
fi

echo "Simulating ESP32 Deep Sleep device"
echo "  MAC:      $MAC_FORMATTED ($MAC)"
echo "  Broker:   $BROKER:$PORT"
echo "  Interval: ${INTERVAL}s"
echo "  Topics:   devices/$MAC/*"
echo ""
echo "Press Ctrl+C to stop"
echo "───────────────────────────────────"

while true; do
    BOOT=$((BOOT + 1))

    # Generate random sensor values
    TEMP=$(awk 'BEGIN { srand(); printf "%.1f", 25 + rand() * 15 }')
    RSSI=$(awk 'BEGIN { srand(); printf "%d", -30 - int(rand() * 50) }')
    HEAP=$(awk 'BEGIN { srand(); printf "%d", 250000 + int(rand() * 50000) }')

    # Publish all fields in one burst (simulating a single wake cycle)
    mosquitto_pub -h "$BROKER" -p "$PORT" -t "devices/$MAC/boot_count" -m "$BOOT"
    mosquitto_pub -h "$BROKER" -p "$PORT" -t "devices/$MAC/mac" -m "$MAC_FORMATTED"
    mosquitto_pub -h "$BROKER" -p "$PORT" -t "devices/$MAC/ip" -m "$IP"
    mosquitto_pub -h "$BROKER" -p "$PORT" -t "devices/$MAC/rssi" -m "$RSSI"
    mosquitto_pub -h "$BROKER" -p "$PORT" -t "devices/$MAC/heap" -m "$HEAP"
    mosquitto_pub -h "$BROKER" -p "$PORT" -t "devices/$MAC/wifi_channel" -m "$CHANNEL"
    mosquitto_pub -h "$BROKER" -p "$PORT" -t "devices/$MAC/ssid" -m "SimulatedAP"
    mosquitto_pub -h "$BROKER" -p "$PORT" -t "devices/$MAC/firmware_version" -m "1.0.0"
    mosquitto_pub -h "$BROKER" -p "$PORT" -t "devices/$MAC/sleep_interval" -m "$INTERVAL"
    mosquitto_pub -h "$BROKER" -p "$PORT" -t "devices/$MAC/temperature" -m "$TEMP"

    echo "[Boot #$BOOT] temp=${TEMP}C  rssi=${RSSI}dBm  heap=${HEAP}  interval=${INTERVAL}s"
    echo "  Sleeping for ${INTERVAL}s..."

    sleep "$INTERVAL"
done
