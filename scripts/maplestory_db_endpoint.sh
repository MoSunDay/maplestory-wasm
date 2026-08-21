#!/usr/bin/env bash
set -euo pipefail

SERVER_DIR="/data00/github/maplestory-wasm/link_repos/MapleStory-Server"

read_env_value() {
  local pid="$1"
  local key="$2"
  tr '\0' '\n' <"/proc/${pid}/environ" 2>/dev/null | sed -n "s/^${key}=//p" | head -n 1
}

find_maplestory_mysql_pid() {
  local env_file
  local pid
  local command
  for env_file in /proc/[0-9]*/environ; do
    pid="${env_file#/proc/}"
    pid="${pid%%/*}"

    command="$(cat "/proc/${pid}/comm" 2>/dev/null || true)"
    if [ "$command" != "mysqld" ] && [ "$command" != "mariadbd" ]; then
      continue
    fi

    if read_env_value "$pid" "MYSQL_DATABASE" | grep -qx "maplestory"; then
      echo "$pid"
      return 0
    fi
  done

  return 1
}

mysql_container_host() {
  local pid="$1"
  nsenter -t "$pid" -n ip -4 -o addr show eth0 2>/dev/null |
    sed -n 's/.* inet \([^/]*\)\/.*/\1/p' |
    head -n 1
}

require_database() {
  local pid
  local host

  pid="$(find_maplestory_mysql_pid)" || {
    echo "No running MySQL process advertises MYSQL_DATABASE=maplestory" >&2
    return 1
  }

  host="$(mysql_container_host "$pid")"
  if [ -z "$host" ]; then
    echo "Could not resolve IPv4 address for MySQL process ${pid}" >&2
    return 1
  fi

  printf '%s\n' "$pid:$host"
}

case "${1:-host}" in
  host)
    database="$(require_database)"
    printf '%s\n' "${database#*:}"
    ;;
  endpoint)
    database="$(require_database)"
    printf '%s:3306\n' "${database#*:}"
    ;;
  proxy)
    database="$(require_database)"
    host="${database#*:}"
    echo "Proxying MapleStory MySQL to ${host}:3306"
    exec /usr/lib/systemd/systemd-socket-proxyd "${host}:3306"
    ;;
  run-cosmic)
    database="$(require_database)"
    pid="${database%%:*}"
    host="${database#*:}"
    user="$(read_env_value "$pid" "MYSQL_USER")"
    pass="$(read_env_value "$pid" "MYSQL_PASSWORD")"

    if [ -z "$user" ]; then
      user="maplestory"
    fi
    if [ -z "$pass" ]; then
      echo "MySQL process ${pid} does not expose MYSQL_PASSWORD" >&2
      exit 1
    fi

    export MAPLE_DB_URL="jdbc:mysql://${host}:3306/maplestory?useUnicode=true&characterEncoding=UTF-8"
    export MAPLE_DB_USER="$user"
    export MAPLE_DB_PASS="$pass"

    cd "$SERVER_DIR"
    echo "Starting Cosmic with MapleStory database at ${host}:3306"
    exec ./run.sh
    ;;
  *)
    echo "Usage: $0 {host|endpoint|proxy|run-cosmic}" >&2
    exit 64
    ;;
esac
