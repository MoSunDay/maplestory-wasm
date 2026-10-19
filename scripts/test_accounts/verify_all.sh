#!/bin/bash
# End-to-end regression: create accounts -> manifest -> keymap prep -> protocol
# skill verification -> (optional) client animation verification -> report.
# Usage: verify_all.sh {setup|proto|visual <suffix...>|report|restore|all} [--skip-visual]
#   ./verify_all.sh visual DK   # re-run a single character (no manual result copying)
set -u
cd "$(dirname "$0")"
ROOT=/data/maplestory-wasm
DB_PASS=$(grep -oP '(?<=^DB_PASS=).*' "$ROOT/link_repos/MapleStory-Server/configuration.ini")
WORLD=${MS_TEST_WORLD:-4}
SUFFIXES=(Hero Pala DK FPA ILA Bish BM MM NL Shad Bucc CS)
declare -A ACC=( [Hero]=test1 [Pala]=test1 [DK]=test1 [FPA]=test1 [ILA]=test2 [Bish]=test2 [BM]=test2 [MM]=test2 [NL]=test3 [Shad]=test3 [Bucc]=test3 [CS]=test3 )
declare -A IDX=( [Hero]=0 [Pala]=1 [DK]=2 [FPA]=3 [ILA]=0 [Bish]=1 [BM]=2 [MM]=3 [NL]=0 [Shad]=1 [Bucc]=2 [CS]=3 )

reset_login() { mysql -uroot -p"$DB_PASS" -e "UPDATE maplestory.accounts SET loggedin=0 WHERE name IN ('test1','test2','test3');" 2>/dev/null; }

phase_setup() {
  python3 create_accounts.py --count 3 --world "$WORLD" || return 1
  python3 attack_skills.py || return 1
  python3 skill-keymap-prep.py --json attack_skills.json --world "$WORLD" || return 1
  echo "SETUP OK"
}

phase_proto() {
  for sfx in "${SUFFIXES[@]}"; do
    acct=${ACC[$sfx]}
    reset_login
    WIRE_USER=$acct WIRE_WORLD=$WORLD CHAR_SUFFIX=$sfx \
      timeout 180 node verify_skills_proto.mjs > "/tmp/skill_proto_${acct}_${sfx}.log" 2>&1
    echo "proto ${acct}/${sfx} rc=$? : $(grep -oE '[0-9]+ fired, [0-9]+ skipped, [0-9]+ not settled' "/tmp/skill_proto_${acct}_${sfx}.log" | tail -1)"
  done
  reset_login
  echo "PROTO DONE"
}

phase_visual() {
  local sfx acct idx
  for sfx in "$@"; do
    acct=${ACC[$sfx]}; idx=${IDX[$sfx]}
    reset_login
    sleep 10   # let the channel reap the previous char's session (killed browser)
    rm -rf "/tmp/skill-anim/${sfx}"
    E2E_ACCOUNT=$acct E2E_PASSWORD=$acct E2E_CHAR_INDEX=$idx \
      E2E_KEYMAP_JSON=/tmp/skill_keymap.json E2E_ARTIFACT_DIR="/tmp/skill-anim/${sfx}" \
      E2E_DEBUG_PORT=9320 CHROME_BIN=/usr/bin/chromium-browser \
      timeout 400 node "$ROOT/tools/verify/maplestory-webui/skill-anim-flow.mjs" 2>&1 \
      | grep -E "SUMMARY|FAIL|ERROR" | head -3
    reset_login
  done
  echo "VISUAL CHUNK DONE ($*)"
}

phase_report() {
  python3 - "$ROOT" <<'PY'
import glob, json, os, sys, time
proto_ok = proto_bad = vis_ok = vis_bad = vis_skip = 0
rows = []
for suffix in "Hero Pala DK FPA ILA Bish BM MM NL Shad Bucc CS".split():
    acc = {"Hero":"test1","Pala":"test1","DK":"test1","FPA":"test1","ILA":"test2","Bish":"test2","BM":"test2","MM":"test2","NL":"test3","Shad":"test3","Bucc":"test3","CS":"test3"}[suffix]
    p = f"/tmp/skill_proto_{acc}_{suffix}.json"
    v = f"/tmp/skill-anim/{suffix}/results-{acc}{suffix}.json"
    ps = vs = "-"; pl = vl = 0
    if os.path.exists(p):
        r = json.load(open(p)); settled = skipped = bad = 0
        for x in r["results"]:
            if x["ok"] == "settled" or x["ok"] == "accepted-layout": settled += 1
            elif str(x["ok"]).startswith("skip"): skipped += 1
            else:
                bad += 1; print("PROTO-FAIL", acc+suffix, x["id"], x["name"], x["ok"], file=sys.stderr)
        pl = len(r["results"]); ps = f"{settled}/{pl-skipped}"; proto_ok += settled; proto_bad += bad
    if os.path.exists(v):
        r = json.load(open(v)); ok = vskip = 0
        for x in r["results"]:
            if x.get("animation") == "skip": vskip += 1
            elif x.get("animation") == "ok" and x.get("damageVisual") == "ok": ok += 1
            else: print("VISUAL-FAIL", acc+suffix, x.get("skill"), x.get("name"), x.get("animation"), x.get("damageVisual"), file=sys.stderr)
        vl = len(r["results"]) - vskip; vs = f"{ok}/{vl}"; vis_ok += ok; vis_bad += vl - ok; vis_skip += vskip
    rows.append((acc+suffix, ps, vs))
lines = [f"{'char':12} {'proto settled/fired':22} visual ok/total"]
for n, a, b in rows: lines.append(f"{n:12} {a:22} {b}")
verdict = "PASS" if proto_bad == 0 and vis_bad == 0 else "FAIL"
lines.append(f"TOTAL proto {proto_ok} settled / {proto_bad} failed ; visual {vis_ok} ok / {vis_bad} failed{' ; ' + str(vis_skip) + ' skipped' if vis_skip else ''} -> {verdict}")
lines.insert(0, "# generated %s" % time.strftime("%Y-%m-%d %H:%M:%S"))
report = "\n".join(lines) + "\n"
sys.stdout.write(report)
open("/tmp/verify_all_report.txt", "w").write(report)
os.makedirs("/tmp/verify_all_reports", exist_ok=True)
open("/tmp/verify_all_reports/verify_all_report_%s.txt" % time.strftime("%Y%m%d-%H%M%S"), "w").write(report)
PY
}

phase_restore() {
  python3 skill-keymap-prep.py --restore || return 1
  reset_login
  mysql -uroot -p"$DB_PASS" -N -e "SELECT CONCAT('restore: ', COUNT(*), ' character(s) at map ', map) FROM maplestory.characters WHERE accountid IN (SELECT id FROM maplestory.accounts WHERE name IN ('test1','test2','test3')) GROUP BY map;" 2>/dev/null
  echo "RESTORE OK"
}

case "${1:-all}" in
  setup)  shift; phase_setup ;;
  proto)  shift; phase_proto ;;
  visual) shift; phase_visual "$@" ;;
  report) shift; phase_report ;;
  all)
    shift
    phase_setup && phase_proto
    if ! printf '%s\n' "$@" | grep -q -- --skip-visual; then
      for i in 0 3 6 9; do phase_visual "${SUFFIXES[@]:$i:3}"; done
    fi
    phase_report ;;
  restore) shift; phase_restore ;;
  *) echo "usage: $0 {setup|proto|visual <suffix...>|report|restore|all} [--skip-visual]"; exit 64 ;;
esac
