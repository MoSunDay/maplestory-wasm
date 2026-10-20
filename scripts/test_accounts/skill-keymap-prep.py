#!/usr/bin/env python3
"""Bind per-class attack skills to keyboard letters for client-side testing.

Companion of create_accounts.py: moves the 12 ready-to-test characters to the
snail hunting ground (map 40001) and rewrites each character's keymap rows so
its attack skills sit on distinct letter keys. The keymap `key` column stores
the index into the client Keytable (src/client/IO/Keyboard.cpp), so a row
(characterid, key=30, type=1, action=1221009) makes pressing A fire the skill
(type 1 == KeyType::SKILL, action == skill id).

Skill rosters come from --json (attack_skills.json, schema
{"112": {"suffix": "Hero", "kind": "melee",
         "skills": [{"id": 1221009, "name": "...", "serverBuff": false,
                     "weapon": 0}]}}).
Entries with serverBuff:true are skipped (SkillFactory.isBuff buffs that
play no attack animation); special attacks (Meso Explosion, Snipe) stay
bound because their cast animation is key-triggerable.

Cross-character weapon dedup: shared-line skill ids (1st-job imgs such
as Slash Blast or Double Stab) appear in several characters' rosters,
but the client gate in Skill::can_use drops the attack whenever the wz
skill-scope "weapon" requirement (Weapon::by_value(100 + weapon)) does
not match the equipped weapon type (Weapon.h codes: 133 dagger, 137
wand, 138 staff, 145 bow, 146 crossbow, 147 claw, 148 knuckle, 149 gun,
130-132/140-144 warrior arms). Each skill id is therefore claimed by the
first roster character (JOBS order) whose weapon passes the requirement
(weapon 0 = no requirement); other characters do not rebind it and are
summarized in the output's coveredElsewhere key for report transparency.

The browser-side key roster needed to press the keys is written to --out
(default /tmp/skill_keymap.json) as
{"chars": [{"char", "keys": [...]}], "coveredElsewhere":
[{"skill", "name", "on"}]} and is consumed by
tools/verify/maplestory-webui/skill-anim-flow.mjs.

Account grouping mirrors create_accounts.py (4 jobs per account in JOBS
order): test1 = Hero/Pala/DK/FPA, test2 = ILA/Bish/BM/MM,
test3 = NL/Shad/Bucc/CS; character names are <account><suffix>.

Usage:
  python3 skill-keymap-prep.py [--json PATH] [--world 4]
  python3 skill-keymap-prep.py --restore   # chars back to Free Market

DB credentials are read at runtime from the server configuration.ini, same
as create_accounts.py (never hardcoded).
"""
import argparse
import json
import os
import subprocess

from create_accounts import mysql_run, read_db_config
from plans import JOBS

SNAIL_MAP = 40001        # snail hunting ground used as the skill test range
FREE_MARKET_MAP = 910000000
KEY_TYPE_SKILL = 1       # KeyType::SKILL in the client KeyType.h
MAX_BOUND_KEYS = 26      # one letter per attack skill, capped at 26

# Maple key indexes into the client Keytable (Keyboard.cpp): home row first,
# then the top and bottom rows. Distinct keys keep one skill per letter.
LETTER_KEYS = (
    ("A", 30), ("S", 31), ("D", 32), ("F", 33), ("G", 34), ("H", 35),
    ("J", 36), ("K", 37), ("L", 38),
    ("Q", 16), ("W", 17), ("E", 18), ("R", 19), ("T", 20), ("Y", 21),
    ("U", 22), ("I", 23), ("O", 24), ("P", 25),
    ("Z", 44), ("X", 45), ("C", 46), ("V", 47), ("B", 48), ("N", 49),
    ("M", 50),
)

# Equipped weapon (inventoryitems.position = -11) of each test character,
# measured read-only in the server DB, mapped to the client Weapon::Type
# code the itemid prefix stands for (src/client/Character/Inventory/
# Weapon.h): 1402xxx -> 140 SWORD_2H, 1322xxx -> 132 MACE_1H (BW),
# 1432xxx -> 143 SPEAR, 1382xxx -> 138 STAFF, 1452xxx -> 145 BOW,
# 1462xxx -> 146 CROSSBOW, 1472xxx -> 147 CLAW, 1332xxx -> 133 DAGGER,
# 1482xxx -> 148 KNUCKLE, 1492xxx -> 149 GUN.
WEAPON_OF_SUFFIX = {
    "Hero": 140,
    "Pala": 132,
    "DK": 143,
    "FPA": 138,
    "ILA": 138,
    "Bish": 138,
    "BM": 145,
    "MM": 146,
    "NL": 147,
    "Shad": 133,
    "Bucc": 148,
    "CS": 149,
}


def character_roster(prefix, chars_per_account):
    """The 12 test characters, in create_accounts.py JOBS/suffix order."""
    for index, job in enumerate(JOBS):
        account = "%s%d" % (prefix, index // chars_per_account + 1)
        yield {"name": account + job[8], "job": job[1], "suffix": job[8]}


def mysql_rows(cfg, sql):
    """SELECT helper: the same mysql-CLI pipe as create_accounts.mysql_run,
    with -N -B so the caller gets bare tab-separated rows back."""
    env = dict(os.environ)
    env["MYSQL_PWD"] = cfg["password"]
    proc = subprocess.run(
        ["mysql", "-N", "-B", "-h", cfg["host"], "-u", cfg["user"], cfg["database"]],
        input=sql, text=True, capture_output=True, env=env,
    )
    if proc.returncode != 0:
        raise RuntimeError("mysql query failed: %s" % proc.stderr.strip()[:400])
    return [line.split("\t") for line in proc.stdout.splitlines() if line.strip()]


def load_attack_skills(path):
    """Flatten attack_skills.json into {jobid: [(skillid, name, special,
    summon, client_attack, weapon), ...]} - the manifest's per-skill
    special marker, summon flag, clientAttack classification and wz
    weapon requirement ride along so the browser roster can surface them
    and the cross-character dedup can apply the client weapon gate.

    Only skills the server classifies as buffs (serverBuff:true, parsed
    from SkillFactory's isBuff switch by attack_skills.py) are dropped:
    they play no attack animation. Special attacks such as Meso Explosion
    and Snipe stay bound - their cast animation is key-triggerable.
    """
    with open(path, "r", encoding="utf-8") as handle:
        raw = json.load(handle)
    jobs = {}
    for jobid, entry in raw.items():
        skills = []
        for skill in entry.get("skills", []):
            if isinstance(skill, dict):
                if bool(skill.get("serverBuff")):
                    continue  # server-side buff: no plain attack animation
                skills.append((
                    int(skill["id"]),
                    str(skill.get("name", "") or ""),
                    str(skill.get("special", "") or ""),
                    bool(skill.get("summon", False)),
                    bool(skill.get("clientAttack", False)),
                    int(skill.get("weapon", 0) or 0),
                ))
            else:
                skills.append((int(skill), "", "", False, False, 0))
        jobs[int(jobid)] = {"suffix": entry.get("suffix", ""), "skills": skills}
    return jobs


def weapon_compatible(weapon_code, suffix):
    """Client Skill::can_use weapon gate (Gameplay/Combat/Skill.cpp): the
    wz skill-scope weapon value maps to Weapon::by_value(100 + weapon);
    0 means no requirement, otherwise it must equal the character's
    equipped weapon type (WEAPON_OF_SUFFIX, DB-measured)."""
    return weapon_code == 0 or WEAPON_OF_SUFFIX.get(suffix) == 100 + weapon_code


def claim_rosters(roster, skills_by_job):
    """Cross-character weapon dedup over the 12 characters in roster order.

    A skill id is claimed by the first character whose weapon passes the
    manifest weapon requirement; later characters - and characters whose
    own weapon mismatches (Double Stab style cross-weapon duplicates) -
    do not rebind it. Returns ({character name -> kept skill tuples},
    coveredElsewhere rows naming the claiming suffix per dropped skill).
    """
    claimed = {}
    per_char = {}
    for character in roster:
        name, suffix = character["name"], character["suffix"]
        skills = skills_by_job.get(character["job"], {"skills": []})["skills"]
        kept = []
        for skill in skills:
            if skill[0] in claimed or not weapon_compatible(skill[5], suffix):
                continue
            claimed[skill[0]] = suffix
            kept.append(skill)
        per_char[name] = kept
    covered = []
    for character in roster:
        suffix = character["suffix"]
        skills = skills_by_job.get(character["job"], {"skills": []})["skills"]
        for skill in skills:
            owner = claimed.get(skill[0])
            if owner is not None and owner != suffix:
                covered.append({"skill": skill[0], "name": skill[1], "on": owner})
    return per_char, covered


def quoted(names):
    return ",".join("'%s'" % name for name in names)


def restore(cfg, roster):
    """Undo mode: characters back to the Free Market, keymap rows stay."""
    names = quoted(character["name"] for character in roster)
    rows = mysql_rows(
        cfg,
        "UPDATE characters SET map=%d WHERE name IN (%s); SELECT ROW_COUNT();"
        % (FREE_MARKET_MAP, names),
    )
    print("restore: %s character(s) back to map %d (Free Market);"
          " keymap rows left in place" % (rows[0][0] if rows else 0, FREE_MARKET_MAP))


def prepare(cfg, roster, skills_by_job, prefix, world):
    """Move the test characters to the snail range and bind their skills."""
    rows = mysql_rows(
        cfg,
        "UPDATE characters SET map=%d WHERE name LIKE '%s%%' AND world = %d;"
        " SELECT ROW_COUNT();" % (SNAIL_MAP, prefix, world),
    )
    print("moved %s character(s) named '%s%%' (world %d) to map %d"
          % (rows[0][0] if rows else 0, prefix, world, SNAIL_MAP))

    ids = {
        name: int(cid)
        for cid, name in mysql_rows(
            cfg,
            "SELECT id, name FROM characters WHERE name IN (%s);" % quoted(
                character["name"] for character in roster),
        )
    }

    per_char, covered = claim_rosters(roster, skills_by_job)
    for row in covered:
        print("dedup %-7d %-24s kept on %s (covered elsewhere)"
              % (row["skill"], row["name"], row["on"]))

    browser_roster = []
    for character in roster:
        name, job = character["name"], character["job"]
        if name not in ids:
            print("WARN  %s (job %d) not found in DB; skipped" % (name, job))
            continue
        entry = skills_by_job.get(job)
        if not entry or not entry["skills"]:
            print("WARN  %s (job %d) has no attack skills in the JSON; skipped"
                  % (name, job))
            continue
        if entry["suffix"] and entry["suffix"] != character["suffix"]:
            print("WARN  %s: JSON suffix %r != roster suffix %r"
                  % (name, entry["suffix"], character["suffix"]))

        kept = per_char.get(name, [])
        bindings = []
        for (letter, maple_index), skill in zip(LETTER_KEYS, kept):
            bindings.append((letter, maple_index, skill[0], skill[1], skill[2], skill[3], skill[4]))
        bindings = bindings[:MAX_BOUND_KEYS]

        if not bindings:
            print("WARN  %s: every attack skill is covered elsewhere;"
                  " keymap left untouched" % name)
            continue
        statements = ["DELETE FROM keymap WHERE characterid = %d;" % ids[name]]
        values = ",".join(
            "(%d, %d, %d, %d)" % (ids[name], maple_index, KEY_TYPE_SKILL, skill_id)
            for _, maple_index, skill_id, _, _, _, _ in bindings
        )
        statements.append(
            "INSERT INTO keymap (characterid, `key`, type, action) VALUES %s;" % values)
        mysql_run(cfg, "\n".join(statements))

        print("%s: bound %d skill(s) to keys %s..%s (maple idx %d..%d)"
              % (name, len(bindings), bindings[0][0], bindings[-1][0],
                 bindings[0][1], bindings[-1][1]))
        for letter, maple_index, skill_id, skill_name, _special, _summon, _client_attack in bindings:
            print(json.dumps({
                "char": name, "mapleindex": maple_index,
                "glfw": "GLFW_KEY_%s" % letter, "key": letter,
                "skill": skill_id, "name": skill_name,
            }, sort_keys=True))
        browser_roster.append({
            "char": name,
            "keys": [{"key": letter, "skill": skill_id, "name": skill_name,
                      "special": special, "summon": summon,
                      "clientAttack": client_attack}
                     for letter, _, skill_id, skill_name, special, summon,
                         client_attack
                     in bindings],
        })
    return browser_roster, covered


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(
        description="Prep keymap + spawn map for client-side skill testing.")
    parser.add_argument("--json", default=os.path.join(script_dir, "attack_skills.json"),
                        help="attack_skills.json path")
    parser.add_argument("--world", type=int,
                        default=int(os.environ.get("MS_TEST_WORLD", "4")),
                        help="world the test characters live in")
    parser.add_argument("--prefix", default="test")
    parser.add_argument("--chars-per-account", type=int,
                        default=int(os.environ.get("MS_TEST_CHARS_PER_ACCOUNT", "4")))
    parser.add_argument("--config",
                        default="/data/maplestory-wasm/link_repos/MapleStory-Server/configuration.ini")
    parser.add_argument("--out", default="/tmp/skill_keymap.json",
                        help="browser-side key roster written for skill-anim-flow.mjs")
    parser.add_argument("--restore", action="store_true",
                        help="undo: move characters back to the Free Market")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    roster = list(character_roster(args.prefix, args.chars_per_account))
    cfg = read_db_config(args.config)

    if args.restore:
        if not args.dry_run:
            restore(cfg, roster)
        else:
            print("dry-run restore: chars %s -> map %d"
                  % (",".join(c["name"] for c in roster), FREE_MARKET_MAP))
        return

    skills_by_job = load_attack_skills(args.json)
    missing = [c["name"] for c in roster if c["job"] not in skills_by_job]
    if missing:
        print("WARN  no JSON entry for jobs of: %s" % ",".join(missing))

    per_char, covered = claim_rosters(roster, skills_by_job)
    if args.dry_run:
        for character in roster:
            entry = skills_by_job.get(character["job"], {"skills": []})
            bound = min(len(per_char.get(character["name"], [])), MAX_BOUND_KEYS)
            dropped = len(entry.get("skills", [])) - len(per_char.get(character["name"], []))
            print("dry-run %s: would bind %d skill(s) to %s%s"
                  % (character["name"], bound,
                     ",".join(letter for letter, _ in LETTER_KEYS[:bound]) or "-",
                     " (%d covered elsewhere)" % dropped if dropped else ""))
        for row in covered:
            print("dry-run dedup: %d %s kept on %s"
                  % (row["skill"], row["name"], row["on"]))
        return

    browser_roster, covered = prepare(cfg, roster, skills_by_job, args.prefix,
                                      args.world)
    with open(args.out, "w", encoding="utf-8") as handle:
        json.dump({"chars": browser_roster, "coveredElsewhere": covered},
                  handle, ensure_ascii=False, indent=2)
        handle.write("\n")
    print("wrote %s (%d chars, %d coveredElsewhere row(s))"
          % (args.out, len(browser_roster), len(covered)))
    print("next: E2E_ACCOUNT=<account> E2E_CHAR_INDEX=<0-3>"
          " E2E_KEYMAP_JSON=%s node"
          " tools/verify/maplestory-webui/skill-anim-flow.mjs" % args.out)


if __name__ == "__main__":
    main()
