#!/bin/bash
set -euo pipefail

# 验收前测试角色清理脚本。
#
# 背景：tools/verify/maplestory-webui/run.mjs 默认用账号 test1 创建角色"测试一"，
# 历次运行残留同名角色会导致下次验收在角色命名一步因重名出现
# "accepted name never entered customization" 假失败。本脚本在验收前把该账号下
# 同名角色（可能多条）连同其子表数据一并清理，避免手工 SQL 操作。
#
# 各子表关联列均已逐表 SHOW COLUMNS 核实（见每段注释），已知坑：
#   - area_info / monsterbook / cooldowns / wishlists / playerdiseases 的列是 charid
#     而不是 characterid；
#   - mts_cart 的列是 cid；
#   - pets / petignores 没有角色列，须经 inventoryitems.petid 关联删除；
#   - inventoryequipment 经 inventoryitems.inventoryitemid 关联删除；
#   - playernpcs 表没有任何角色关联列（本库中该表为空，按角色清理无从下手），
#     跳过不处理；
#   - storages 表按 accountid+world 存储且无角色列（账号级仓库，含其他角色财产），
#     按角色清理无从下手，跳过不处理。
#
# 用法：
#   scripts/game_server_cleanup_test_character.sh                       # 默认 test1 / 测试一
#   scripts/game_server_cleanup_test_character.sh --account test1 --name 测试一
#   scripts/game_server_cleanup_test_character.sh --dry-run             # 只统计不删除
#
# 数据库口令在运行时从 link_repos/MapleStory-Server/configuration.ini 的 DB_PASS=
# 行读取，脚本内不包含任何硬编码口令。mysql 用法与既有运维命令一致（-uroot -p"$DB_PASS"）。

ACCOUNT_NAME="test1"
CHARACTER_NAME="测试一"
DRY_RUN=0

usage() {
  cat <<'USAGE'
用法: scripts/game_server_cleanup_test_character.sh [--account 账号名] [--name 角色名] [--dry-run]

参数:
  --account   账号名（默认 test1，即 run.mjs 的 E2E_ACCOUNT）
  --name      角色名（默认 测试一，即 run.mjs 的 E2E_CHARACTER）
  --dry-run   只统计将要删除的行数，不执行任何 DELETE
  -h|--help   显示本帮助
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
    --dry-run)
      DRY_RUN=1
      shift
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

# ---------------------------------------------------------------------------
# 读取服务端配置：DB 口令 / 用户 / 主机 / 库名
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

echo "---------------------------------------------------"
echo "MapleStory 测试角色清理"
echo "  账号: ${ACCOUNT_NAME}  角色名: ${CHARACTER_NAME}"
if [ "${DRY_RUN}" -eq 1 ]; then
  echo "  模式: dry-run（仅统计，不删除）"
else
  echo "  模式: 实删"
fi
echo "---------------------------------------------------"

# ---------------------------------------------------------------------------
# 定位账号与角色
# ---------------------------------------------------------------------------
ACCOUNT_ID="$(msql -e "SELECT id FROM accounts WHERE name = '$(sql_escape "${ACCOUNT_NAME}")';")"
if [ -z "${ACCOUNT_ID}" ]; then
  echo "错误: 账号 ${ACCOUNT_NAME} 不存在，无法定位角色" >&2
  exit 1
fi

# 同名角色可能有多条（历次验收残留），全部纳入清理
mapfile -t CHAR_IDS < <(msql -e \
  "SELECT id FROM characters WHERE name = '$(sql_escape "${CHARACTER_NAME}")' AND accountid = ${ACCOUNT_ID};")
if [ "${#CHAR_IDS[@]}" -eq 0 ]; then
  echo "账号 ${ACCOUNT_NAME}(id=${ACCOUNT_ID}) 下没有名为 ${CHARACTER_NAME} 的角色，无需清理。"
  exit 0
fi

IDS_LIST="$(IFS=,; echo "${CHAR_IDS[*]}")"
echo "目标角色 id: ${IDS_LIST}（共 ${#CHAR_IDS[@]} 个）"

# ---------------------------------------------------------------------------
# 子表清理定义
# 列名均已 SHOW COLUMNS 核实：
#   characterid: inventoryitems/skills/keymap/queststatus/questprogress/medalmaps/
#                savedlocations/buddies/trocklocations/eventstats/skillmacros/
#                famelog(两侧)/server_queue
#   charid:      wishlists/cooldowns/monsterbook/area_info/playerdiseases
#   cid:         mts_cart
# 直连列清理表（inventoryitems 放最后：pets/inventoryequipment 依赖其做子查询）
DIRECT_SPECS=(
  "skills|characterid"
  "keymap|characterid"
  "queststatus|characterid"
  "questprogress|characterid"
  "medalmaps|characterid"
  "savedlocations|characterid"
  "buddies|characterid"
  "buddies|buddyid"
  "trocklocations|characterid"
  "eventstats|characterid"
  "skillmacros|characterid"
  "famelog|characterid"
  "famelog|characterid_to"
  "server_queue|characterid"
  "wishlists|charid"
  "cooldowns|charid"
  "monsterbook|charid"
  "area_info|charid"
  "playerdiseases|charid"
  "mts_cart|cid"
  "inventoryitems|characterid"
)
# 经 inventoryitems 子查询清理的表（必须先于 inventoryitems 本身删除）
SUBQ_SPECS=(
  "inventoryequipment|inventoryitemid"
  "pets|petid"
  "petignores|petid"
)

total_rows=0

report_table() {
  local table="$1" column="$2" rows="$3" action="$4"
  printf '  %-20s %-14s %s %s 行\n' "${table}" "${column}" "${action}" "${rows}"
  total_rows=$((total_rows + rows))
}

# 直连列：WHERE col IN (角色 id 列表)
process_direct() {
  local spec table column rows
  for spec in "${DIRECT_SPECS[@]}"; do
    table="${spec%%|*}"
    column="${spec##*|}"
    if [ "${DRY_RUN}" -eq 1 ]; then
      rows="$(msql -e "SELECT COUNT(*) FROM \`${table}\` WHERE \`${column}\` IN (${IDS_LIST});")"
      report_table "${table}" "${column}" "${rows}" "将删除"
    else
      # DELETE 后立即用 ROW_COUNT() 取实际删除行数（同一会话内读取）
      rows="$(msql -e "DELETE FROM \`${table}\` WHERE \`${column}\` IN (${IDS_LIST}); SELECT ROW_COUNT();")"
      report_table "${table}" "${column}" "${rows}" "已删除"
    fi
  done
}

# 子查询列：col 经 inventoryitems 关联到角色
process_subquery() {
  local spec table column filter rows
  for spec in "${SUBQ_SPECS[@]}"; do
    table="${spec%%|*}"
    column="${spec##*|}"
    # pets/petignores 经 petid 关联，petid = -1 表示非宠物物品，需排除
    filter="IN (SELECT \`${column}\` FROM inventoryitems WHERE characterid IN (${IDS_LIST})"
    if [ "${column}" = "petid" ]; then
      filter="${filter} AND petid > -1"
    fi
    filter="${filter})"
    if [ "${DRY_RUN}" -eq 1 ]; then
      rows="$(msql -e "SELECT COUNT(*) FROM \`${table}\` WHERE \`${column}\` ${filter};")"
      report_table "${table}" "${column}" "${rows}" "将删除"
    else
      rows="$(msql -e "DELETE FROM \`${table}\` WHERE \`${column}\` ${filter}; SELECT ROW_COUNT();")"
      report_table "${table}" "${column}" "${rows}" "已删除"
    fi
  done
}

# 跳过表说明（无角色关联列，按角色清理无从下手）：
#   playernpcs  —— 列为 id/name/hair/... 无 characterid/charid（当前库为空表）
#   storages    —— 按 accountid+world 的账号级仓库，属账号财产而非单角色数据
echo "子表清理:"
process_subquery
process_direct

# 最后删除 characters 主表行
if [ "${DRY_RUN}" -eq 1 ]; then
  echo "characters            id             将删除 ${#CHAR_IDS[@]} 行"
else
  msql -e "DELETE FROM characters WHERE id IN (${IDS_LIST});" >/dev/null
  echo "characters            id             已删除 ${#CHAR_IDS[@]} 行"
fi
total_rows=$((total_rows + ${#CHAR_IDS[@]}))

echo "---------------------------------------------------"
if [ "${DRY_RUN}" -eq 1 ]; then
  echo "dry-run 完成：以上共 ${total_rows} 行待删除（本次未执行任何 DELETE）。"
else
  echo "清理完成：共删除 ${total_rows} 行（含 characters ${#CHAR_IDS[@]} 行）。"
fi
echo "---------------------------------------------------"
