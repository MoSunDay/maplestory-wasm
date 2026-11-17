#!/bin/bash
set -euo pipefail

# E2E 测试账号角色槽位夹具（幂等）。
#
# 背景：建角 E2E 用的账号需要容纳"标定阵容 4 个角色 + 新建 1 个"共 5 槽，
# 而 accounts.characterslots 默认 3——槽位不足时服务端 CharacterFactory
# .createNewCharacter 返回 -3，CreateCharHandler 对 -1/-3 曾静默无应答导致
# 客户端建角界面永久挂起（2026-11-16 起 -1/-3 已补失败应答）。本脚本把
# 槽位夹具需求固化，避免环境重建后依赖手工 SQL。
#
# 用法：
#   scripts/game_server_test_account_slots.sh --account test1hold            # 默认 --slots 15
#   scripts/game_server_test_account_slots.sh --account test1hold --slots 15
#
# 行为：UPDATE accounts SET characterslots=N，幂等可重复执行；输出 "slots: 旧 -> 新"。
# 账号不存在时报错退出码 1。
#
# 数据库口令在运行时从 link_repos/MapleStory-Server/configuration.ini 的 DB_PASS=
# 行读取，脚本内不包含任何硬编码口令（与 game_server_cleanup_test_character.sh
# 同源约定）。

ACCOUNT_NAME=""
SLOTS=15

usage() {
  cat <<'USAGE'
用法: scripts/game_server_test_account_slots.sh --account 账号名 [--slots N]

参数:
  --account   账号名（必填）
  --slots     目标槽位数（默认 15）
  -h|--help   显示本帮助

输出: slots: 旧 -> 新
USAGE
}

while [ $# -gt 0 ]; do
  case "$1" in
    --account)
      [ $# -ge 2 ] || { echo "错误: --account 缺少参数" >&2; usage >&2; exit 2; }
      ACCOUNT_NAME="$2"
      shift 2
      ;;
    --slots)
      [ $# -ge 2 ] || { echo "错误: --slots 缺少参数" >&2; usage >&2; exit 2; }
      SLOTS="$2"
      case "${SLOTS}" in
        ''|*[!0-9]*) echo "错误: --slots 需为正整数" >&2; exit 2 ;;
      esac
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

if [ -z "${ACCOUNT_NAME}" ]; then
  echo "错误: --account 为必填参数" >&2
  usage >&2
  exit 2
fi

if [ "${SLOTS}" -lt 1 ] || [ "${SLOTS}" -gt 127 ]; then
  echo "错误: --slots 取值范围 1-127（tinyint）" >&2
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

OLD_SLOTS="$(msql -e "SELECT characterslots FROM accounts WHERE name = '${ACCOUNT_SQL}';")"
if [ -z "${OLD_SLOTS}" ]; then
  echo "错误: 账号 '${ACCOUNT_NAME}' 不存在" >&2
  exit 1
fi

msql -e "UPDATE accounts SET characterslots = ${SLOTS} WHERE name = '${ACCOUNT_SQL}';"
NEW_SLOTS="$(msql -e "SELECT characterslots FROM accounts WHERE name = '${ACCOUNT_SQL}';")"

echo "slots: ${OLD_SLOTS} -> ${NEW_SLOTS}"
