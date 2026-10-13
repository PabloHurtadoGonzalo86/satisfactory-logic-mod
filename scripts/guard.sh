#!/usr/bin/env bash
# Run a heavy command only when the (shared) host is calm: memory pressure and load must be low.
# Usage: scripts/guard.sh <command> [args...]   (waits up to ~20 min, then refuses with exit 75)
for _ in $(seq 1 120); do
  mem=$(awk '/^some/ { split($2, a, "="); print a[2] }' /proc/pressure/memory)
  load=$(cut -d' ' -f1 /proc/loadavg)
  if awk -v m="${mem:-100}" -v l="${load:-100}" 'BEGIN { exit !(m < 10 && l < 12) }'; then
    exec "$@"
  fi
  sleep 10
done
echo "guard: host busy (memory pressure ${mem}%, load ${load}); refusing to run: $*" >&2
exit 75
