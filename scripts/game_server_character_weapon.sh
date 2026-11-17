#!/bin/bash
set -euo pipefail

# 查询指定账号下指定角色当前装备的武器 itemid（建角 E2E 断言探针）。
#
# 用途：建角 E2E 断言新职业起始武器——Aran=1442079 长枪 / Pirate=1482000 指虎
# （Pirate 备用 1492000 短枪在背包、不装备）。查询 inventoryitems 中
# inventorytype=-1（装备栏）且 position=-11（武器位）的行。
#
# 用法：
#   scripts/game_server_character_weapon.sh --account 账号名 --name 角色名
#
# 输出：仅一行裸数字（itemid）。无记录时输出 0；账号或角色不存在时同样输出 0
# 且退出码 0——本脚本是断言探针，缺行不是错误路径。
#
# 数据库口令在运行时从 link_repos/MapleStory-Server/configuration.ini 的 DB_PASS=
# 行读取，脚本内不包含任何硬编码口令（与 game_server_cleanup_test_character.sh
# 同源约定）。mysql 用法与既有运维命令一致（-uroot -p"$DB_PASS"）。

ACCOUNT_NAME=""
CHARACTER_NAME=""

usage() {
  cat <<'USAGE'
用法: scripts/game_server_character_weapon.sh --account 账号名 --name 角色名

参数:
  --account   账号名（必填）
  --name      角色名（必填）
  -h|--help   显示本帮助

输出: 当前装备武器 itemid（裸数字，无记录输出 0）
USAGE
}

while [ $# -gt 0 ]; do
  case "$1" in
    --account)
      [ $# -ge 2 ] || { echo "错误: --account 缺少参数" >&2; usage >&2; exit 2; }
      ACCOUNT_NAME="$2"
      shift 2
      ;;
    --name)
      [ $# -ge 2 ] || { echo "错误: --name 缺少参数" >&2; usage >&2; exit 2; }
      CHARACTER_NAME="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "错误: 未知参数 $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [ -z "${ACCOUNT_NAME}" ] || [ -z "${CHARACTER_NAME}" ]; then
  echo "错误: --account 与 --name 均为必填参数" >&2
  usage >&2
  exit 2
fi

# ---------------------------------------------------------------------------
# 读取服务端配置：DB 口令 / 用户 / 主机 / 库名（与 cleanup 脚本同源）
# ---------------------------------------------------------------------------
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SERVER_DIR="${SERVER_DIR:-${REPO_ROOT}/link_repos/MapleStory-Server}"
CONFIG_FILE="${SERVER_DIR}/configuration.ini"

if [ ! -f "${CONFIG_FILE}" ]; then
  echo "错误: 找不到服务端配置 ${CONFIG_FILE}" >&2
  exit 1
fi

read_ini_value() {
  sed -n "s/^$1=//p" "$CONFIG_FILE" | tr -d '\r' | head -n 1
}

DB_USER="$(read_ini_value DB_USER)"
DB_PASS="$(read_ini_value DB_PASS)"
DB_HOST="127.0.0.1"
DB_NAME="maplestory"

# 从 URL=jdbc:mysql://host:port/db?... 解析主机与库名（与 create_accounts.py 口径一致）
JDBC_URL="$(read_ini_value URL)"
if [ -n "${JDBC_URL}" ]; then
  rest="${JDBC_URL#*//}"
  host_part="${rest%%/*}"
  DB_HOST="${host_part%%:*}"
  tail_part="${rest#*/}"
  DB_NAME="${tail_part%%\?*}"
fi

if [ -z "${DB_USER}" ]; then
  DB_USER="root"
fi
if [ -z "${DB_PASS}" ]; then
  echo "错误: ${CONFIG_FILE} 中未找到 DB_PASS= 配置" >&2
  exit 1
fi

# mysql 包装：口令通过 -p"$DB_PASS" 传入（与既有运维命令一致），并过滤掉
# mysql 自带的 "Using a password on the command line" 告警，避免污染输出。
msql() {
  local errfile
  errfile="$(mktemp)"
  if ! mysql -h"${DB_HOST}" -u"${DB_USER}" -p"${DB_PASS}" \
      --default-character-set=utf8mb4 -N -B "${DB_NAME}" "$@" 2>"${errfile}"; then
    grep -v "Using a password" "${errfile}" >&2 || true
    rm -f "${errfile}"
    return 1
  fi
  grep -v "Using a password" "${errfile}" >&2 || true
  rm -f "${errfile}"
}

# SQL 字符串字面量转义（反斜杠 + 单引号翻倍），防止注入/语法错误
sql_escape() {
  local s="$1"
  s="${s//\\/\\\\}"
  s="${s//\'/\'\'}"
  printf '%s' "${s}"
}

ACCOUNT_SQL="$(sql_escape "${ACCOUNT_NAME}")"
CHARACTER_SQL="$(sql_escape "${CHARACTER_NAME}")"

# 装备栏（inventorytype=-1）武器位（position=-11）的 itemid；无行输出 0
WEAPON="$(msql -e "SELECT ii.itemid FROM inventoryitems ii
JOIN characters c ON ii.characterid = c.id
JOIN accounts a ON c.accountid = a.id
WHERE a.name = '${ACCOUNT_SQL}' AND c.name = '${CHARACTER_SQL}'
  AND ii.inventorytype = -1 AND ii.position = -11;" | head -n 1)"

if [ -z "${WEAPON}" ]; then
  echo 0
else
  echo "${WEAPON}"
fi
