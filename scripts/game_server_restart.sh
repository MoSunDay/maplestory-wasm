#!/bin/bash
set -euo pipefail

# Restart ms-game-server safely.
#
# The Java server uses MINA without SO_REUSEADDR: any socket still in
# TIME_WAIT on a game port when the new process binds makes that channel
# die with "Address already in use" (repeatedly hit on Reboot 7975).
# This script waits until every game port is fully released before
# starting the container again, then verifies all listeners came up.

CONTAINER=ms-game-server
GAME_PORTS='7575|7675|7775|7875|7975|8484'
ALL_PORTS='7575|7675|7775|7875|7975|8484'
RELEASE_TIMEOUT=180
READY_TIMEOUT=120

echo "---------------------------------------------------"
echo "Restarting MapleStory game server"
echo "---------------------------------------------------"

echo "[1/4] Stopping ${CONTAINER} (30s grace)..."
docker stop -t 30 "${CONTAINER}"

echo "[2/4] Waiting for game ports to be fully released..."
deadline=$((SECONDS + RELEASE_TIMEOUT))
while :; do
  lingering=$(ss -tan | awk '$1 != "LISTEN" {print $4}' \
    | grep -cE ":(${GAME_PORTS})$" || true)
  [ "${lingering}" -eq 0 ] && break
  if [ "${SECONDS}" -ge "${deadline}" ]; then
    echo "TIMEOUT: ${lingering} socket(s) still holding game ports after ${RELEASE_TIMEOUT}s" >&2
    exit 1
  fi
  echo "  ${lingering} lingering socket(s), retrying in 5s..."
  sleep 5
done
echo "  all game ports released."

echo "[3/4] Starting ${CONTAINER}..."
docker start "${CONTAINER}"

echo "[4/4] Waiting for all channel + login listeners..."
deadline=$((SECONDS + READY_TIMEOUT))
while :; do
  listening=$(ss -tln | awk 'NR > 1 {print $4}' \
    | grep -cE ":(${ALL_PORTS})$" || true)
  [ "${listening}" -eq 6 ] && break
  if [ "${SECONDS}" -ge "${deadline}" ]; then
    echo "TIMEOUT: only ${listening}/6 listeners up after ${READY_TIMEOUT}s:" >&2
    ss -tln | grep -E ":(${ALL_PORTS})" >&2 || true
    docker logs --tail 50 "${CONTAINER}" >&2 || true
    exit 1
  fi
  sleep 3
done

echo "---------------------------------------------------"
ss -tln | grep -E ":(${ALL_PORTS})"
echo "All services restarted successfully!"
echo "---------------------------------------------------"
