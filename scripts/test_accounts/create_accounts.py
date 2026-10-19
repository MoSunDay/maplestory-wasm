#!/usr/bin/env python3
"""Create ready-to-test MapleStory accounts (level 200, all jobs).

The 12 explorer 4th-job characters are spread across the accounts, at most
--chars-per-account per account (default 4):
  test1: Hero, Pala, DK, FPA
  test2: ILA, Bish, BM, MM
  test3: NL, Shad, Bucc, CS
Each character is level 200 with wz-computed optimal stats, every quest
completed, all job skills maxed, perfect-scrolled Timeless gear equipped
and starter consumables.

Usage:
  python3 create_accounts.py [--prefix test] [--count 3]
                             [--chars-per-account 4] [--dry-run] [--skip-existing]
                             [--config PATH]

DB credentials are read at runtime from the game server configuration.ini
(never hardcoded). The account password comes from MS_TEST_ACCOUNT_PASSWORD;
when unset, each account's password is its own account name. Characters per
account defaults to MS_TEST_CHARS_PER_ACCOUNT (4). Passwords are
stored as the sha512 hex form HeavenMS accepts.
"""
import argparse
import hashlib
import os
import subprocess
import time

from wzdata import (
    load_beginner_skills,
    load_consume_slot_max,
    load_job_line_skills,
    load_quest_ids,
)
from plans import JOBS, SLOT_POSITION, character_blueprint

STARTER_MESO = 1000000000
STARTER_FAME = 30000
CHARACTER_MAP = 910000000  # Free Market
INSERT_CHUNK = 500
# Space=NPCCHAT, Alt=JUMP, Ctrl=ATTACK; without rows the server KEYMAP packet
# blanks every slot and these actions stop working.
DEFAULT_KEYMAP = ((57, 5, 54), (56, 5, 53), (29, 5, 52))
EQUIP_BONUS_COLUMNS = (
    "str", "dex", "int", "luk", "hp", "mp", "watk", "matk",
    "wdef", "mdef", "acc", "avoid", "hands", "speed", "jump",
)
# Per-character tables wiped by cleanup_sql. Every entry is confirmed to
# exist in link_repos/MapleStory-Server/sql/db_database.sql AND to be keyed
# by a literal `characterid` column. Siblings keyed by `charid`
# (area_info, cooldowns, monsterbook, playerdiseases) or `cid`
# (family_character, fredstorage, mts_cart) are out of scope here.
# The set mirrors the server's own per-character delete cascade
# (MapleCharacter.deleteCharacterFromDB: medalmaps/questprogress/queststatus,
# famelog, keymap, savedlocations, trocklocations, skillmacros, skills,
# eventstats, server_queue) plus buddies, which is characterid-keyed too.
CHILD_TABLES = (
    "questprogress", "medalmaps", "queststatus",
    "skills", "skillmacros", "keymap",
    "famelog", "savedlocations", "trocklocations",
    "eventstats", "server_queue", "buddies",
)


def read_db_config(path):
    """Parse URL/DB_USER/DB_PASS from the server configuration.ini."""
    values = {}
    with open(path, "r", encoding="utf-8") as handle:
        for line in handle:
            if "=" in line:
                key, _, val = line.strip().partition("=")
                values.setdefault(key.strip(), val.strip())
    host, database = "127.0.0.1", "maplestory"
    url = values.get("URL", "")
    if url:
        rest = url.split("//", 1)[-1]
        host_part, _, tail = rest.partition("/")
        if ":" in host_part:
            host = host_part.split(":", 1)[0]
        database = tail.split("?", 1)[0] or database
    return {
        "host": host,
        "database": database,
        "user": values.get("DB_USER", "root"),
        "password": values.get("DB_PASS", ""),
    }


def mysql_run(cfg, sql, flags=()):
    env = dict(os.environ)
    env["MYSQL_PWD"] = cfg["password"]
    proc = subprocess.run(
        ["mysql", "-h", cfg["host"], "-u", cfg["user"], cfg["database"], *flags],
        input=sql, text=True, capture_output=True, env=env,
    )
    if proc.returncode != 0:
        raise RuntimeError("mysql failed: %s" % proc.stderr.strip()[:400])
    return proc.stdout


def existing_accounts(cfg, account_names):
    """Return the subset of account_names already present in accounts."""
    names = ",".join("'%s'" % n for n in account_names)
    out = mysql_run(cfg, "SELECT name FROM accounts WHERE name IN (%s);" % names,
                    flags=("-N",))
    return {row.strip() for row in out.splitlines() if row.strip()}


def account_slot(account_name):
    """Split an account name into (base, slot): 'test2' -> ('test', 2).

    The slot is the account's 1-based blueprint/character-name number, so
    skipping existing accounts never shifts names or job blocks.
    """
    base = account_name.rstrip("0123456789")
    return base, int(account_name[len(base):] or 1)


def account_password_hash(password):
    return hashlib.sha512(password.encode("utf-8")).hexdigest()


def cleanup_sql(account_names):
    """Build DELETE statements wiping the named accounts and every row tied
    to their characters.

    Character-keyed tables removed (one DELETE per CHILD_TABLES entry, all
    keyed by `characterid`): questprogress, medalmaps, queststatus, skills,
    skillmacros, keymap, famelog, savedlocations, trocklocations, eventstats,
    server_queue, buddies. inventoryequipment/inventoryitems are joined
    separately, then characters and the accounts themselves.
    """
    names = ",".join("'%s'" % n for n in account_names)
    stmts = [(
        "CREATE TEMPORARY TABLE _del_acc (id INT PRIMARY KEY);"
        "INSERT INTO _del_acc SELECT id FROM accounts WHERE name IN (%s);" % names
    ), (
        "CREATE TEMPORARY TABLE _del_chr (id INT PRIMARY KEY);"
        "INSERT INTO _del_chr SELECT id FROM characters WHERE accountid IN (SELECT id FROM _del_acc);"
    )]
    for table in CHILD_TABLES:
        stmts.append(
            "DELETE FROM %s WHERE characterid IN (SELECT id FROM _del_chr);" % table
        )
    stmts.append(
        "DELETE ie FROM inventoryequipment ie JOIN inventoryitems ii"
        " ON ie.inventoryitemid = ii.inventoryitemid"
        " WHERE ii.characterid IN (SELECT id FROM _del_chr);"
    )
    stmts.append("DELETE FROM inventoryitems WHERE characterid IN (SELECT id FROM _del_chr);")
    stmts.append("DELETE FROM characters WHERE id IN (SELECT id FROM _del_chr);")
    stmts.append("DELETE FROM accounts WHERE id IN (SELECT id FROM _del_acc);")
    stmts.append("DROP TEMPORARY TABLE _del_acc; DROP TEMPORARY TABLE _del_chr;")
    return stmts


def character_skills(job_imgs):
    merged = {sid: lvl for sid, lvl in load_beginner_skills()}
    for sid, lvl in load_job_line_skills(job_imgs):
        merged[sid] = lvl
    return sorted(merged.items())


def chunked(seq, size):
    for start in range(0, len(seq), size):
        yield seq[start:start + size]


def character_sql(name, char_index, blueprint, quest_ids, world=4):
    gender = char_index % 2
    hair, face = (30000, 20000) if gender == 0 else (31000, 21000)
    stats = blueprint["stats"]
    stmts = [(
        "INSERT INTO characters (accountid, world, name, level, exp, str, dex, `int`, luk,"
        " hp, mp, maxhp, maxmp, meso, fame, job, skincolor, gender, hair, face, ap, sp,"
        " map, spawnpoint, gm)"
        " VALUES (@acc, {world}, '{name}', 200, 0, {str}, {dex}, {int}, {luk},"
        " {hp}, {mp}, {hp}, {mp}, {meso}, {fame}, {job}, 0, {gender},"
        " {hair}, {face}, 0, '0,0,0,0,0,0,0,0,0,0', {map}, 0, 0);".format(
            name=name, str=stats["str"], dex=stats["dex"], int=stats["int"],
            luk=stats["luk"], hp=blueprint["maxhp"], mp=blueprint["maxmp"],
            meso=STARTER_MESO, fame=STARTER_FAME, job=blueprint["job"],
            gender=gender, hair=hair, face=face, map=CHARACTER_MAP, world=world,
        )
    ), "SET @chr = LAST_INSERT_ID();"]

    skills = character_skills(blueprint["skill_imgs"])
    for chunk in chunked(skills, INSERT_CHUNK):
        values = ",".join("(%d, @chr, %d, %d, -1)" % (sid, lv, lv) for sid, lv in chunk)
        stmts.append(
            "INSERT INTO skills (skillid, characterid, skilllevel, masterlevel, expiration)"
            " VALUES %s;" % values
        )

    now = int(time.time())
    for chunk in chunked(quest_ids, INSERT_CHUNK):
        values = ",".join("(@chr, %d, 2, %d, 0, 0, 0)" % (qid, now) for qid in chunk)
        stmts.append(
            "INSERT INTO queststatus (characterid, quest, status, time, expires,"
            " forfeited, info) VALUES %s;" % values
        )

    keymap_values = ",".join("(@chr, %d, %d, %d)" % row for row in DEFAULT_KEYMAP)
    stmts.append(
        "INSERT INTO keymap (characterid, `key`, type, action) VALUES %s;"
        % keymap_values
    )
    return stmts


def equipped_item_sql(equip):
    slot, itemid, tuc, bonus = equip
    bonus = bonus or {}
    inc = {col: int(bonus.get(col, 0)) for col in EQUIP_BONUS_COLUMNS}
    return [
        "INSERT INTO inventoryitems (type, characterid, accountid, itemid, inventorytype,"
        " position, quantity, owner, petid, flag, expiration, giftFrom)"
        " VALUES (1, @chr, @acc, %d, -1, %d, 1, '', -1, 0, -1, '');" % (
            itemid, SLOT_POSITION[slot]
        ),
        "SET @item = LAST_INSERT_ID();",
        ("INSERT INTO inventoryequipment (inventoryitemid, upgradeslots, level, str, dex,"
         " `int`, luk, hp, mp, watk, matk, wdef, mdef, acc, avoid, hands, speed, jump,"
         " locked, vicious, itemlevel, itemexp, ringid)"
         " VALUES (@item, 0, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d,"
         " 0, 0, 1, 0, -1);" % (
             tuc, inc["str"], inc["dex"], inc["int"], inc["luk"], inc["hp"], inc["mp"],
             inc["watk"], inc["matk"], inc["wdef"], inc["mdef"], inc["acc"], inc["avoid"],
             inc["hands"], inc["speed"], inc["jump"],
         )),
    ]


def use_items_sql(use_items):
    rows = []
    for position, (itemid, quantity) in enumerate(use_items, start=1):
        slot_max = load_consume_slot_max(itemid)
        rows.append(
            "(1, @chr, @acc, %d, 2, %d, %d, '', -1, 0, -1, '')" % (
                itemid, position, min(quantity, slot_max)
            )
        )
    return ("INSERT INTO inventoryitems (type, characterid, accountid, itemid,"
            " inventorytype, position, quantity, owner, petid, flag, expiration, giftFrom)"
            " VALUES %s;" % ",".join(rows))


def verify_sql(account_names):
    names = ",".join("'%s'" % n for n in account_names)
    return (
        "SELECT a.name AS account, COUNT(c.id) AS chars,"
        " SUM(c.level = 200) AS lv200, SUM(c.job IN (112,122,132,212,222,232,312,322,412,422,512,522)) AS jobs4th"
        " FROM accounts a LEFT JOIN characters c ON c.accountid = a.id"
        " WHERE a.name IN (%s) GROUP BY a.name;\n"
        "SELECT c.name, c.job, c.level, c.str, c.dex, c.`int`, c.luk,"
        " (SELECT COUNT(*) FROM skills s WHERE s.characterid = c.id) AS skills,"
        " (SELECT COUNT(*) FROM queststatus q WHERE q.characterid = c.id AND q.status = 2) AS quests_done,"
        " (SELECT COUNT(*) FROM inventoryitems i WHERE i.characterid = c.id AND i.inventorytype = -1) AS equipped"
        " FROM characters c JOIN accounts a ON a.id = c.accountid"
        " WHERE a.name IN (%s) ORDER BY c.accountid, c.id;\n" % (names, names)
    )


def build_sql(account_names, blueprints, quest_ids, password=None, chars_per_account=4, world=4):
    stmts = ["SET NAMES utf8mb4;"]
    stmts.extend(cleanup_sql(account_names))
    for account_name in account_names:
        account_password = password if password is not None else account_name
        stmts.append(
            "INSERT INTO accounts (name, password, characterslots, gender, birthday,"
            " nxCredit, maplePoint, nxPrepaid, tos)"
            " VALUES ('%s', '%s', %d, 0, '1990-01-01', 0, 0, 0, 1);" % (
                account_name, account_password_hash(account_password), chars_per_account
            )
        )
        stmts.append("SET @acc = LAST_INSERT_ID();")
        # Blueprint block and character names follow the account's own number
        # (test2 -> test2Hero...), so a --skip-existing subset keeps its slots.
        base, slot = account_slot(account_name)
        chunk = blueprints[(slot - 1) * chars_per_account : slot * chars_per_account]
        for char_index, blueprint in enumerate(chunk):
            name = "%s%d%s" % (base, slot, blueprint["suffix"])
            stmts.extend(character_sql(name, char_index, blueprint, quest_ids, world))
            for equip in blueprint["equips"]:
                stmts.extend(equipped_item_sql(equip))
            stmts.append(use_items_sql(blueprint["use_items"]))
    return "\n".join(stmts)


def main():
    parser = argparse.ArgumentParser(description="Create ready-to-test accounts.")
    parser.add_argument("--prefix", default="test")
    parser.add_argument("--count", type=int, default=3)
    parser.add_argument("--world", type=int, default=int(os.environ.get("MS_TEST_WORLD", "4")))
    parser.add_argument("--chars-per-account", type=int, default=int(os.environ.get("MS_TEST_CHARS_PER_ACCOUNT", "4")))
    parser.add_argument("--config", default="/data/maplestory-wasm/link_repos/MapleStory-Server/configuration.ini")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--skip-existing", action="store_true",
                        help="only create accounts that do not exist yet;"
                             " existing ones (and their characters) stay untouched")
    args = parser.parse_args()

    password = os.environ.get("MS_TEST_ACCOUNT_PASSWORD")
    account_names = ["%s%d" % (args.prefix, i) for i in range(1, args.count + 1)]
    blueprints = [character_blueprint(job) for job in JOBS]

    cfg = None
    if args.skip_existing:
        cfg = read_db_config(args.config)
        existing = existing_accounts(cfg, account_names)
        skipped = [name for name in account_names if name in existing]
        account_names = [name for name in account_names if name not in existing]
        if skipped:
            print("skip-existing: skipped: %s (already exist, left untouched)" % ",".join(skipped))
        if not account_names:
            print("skip-existing: nothing to create, all accounts already exist")
            return

    # Guard counts only the accounts actually being created.
    if len(blueprints) > len(account_names) * args.chars_per_account:
        print("error: %d blueprints but %d accounts * %d chars each;"
              " need --count >= ceil(%d / chars-per-account) = %d" % (
                  len(blueprints), len(account_names), args.chars_per_account,
                  len(blueprints), -(-len(blueprints) // args.chars_per_account)))
        raise SystemExit(2)
    quest_ids = load_quest_ids()

    if args.dry_run:
        for bp in blueprints:
            print(bp["code"], bp["stats"], "equips=%d" % len(bp["equips"]))
        print("accounts: %s quests: %d chars-per-account: %d" % (
            ",".join(account_names), len(quest_ids), args.chars_per_account))
        return

    if cfg is None:
        cfg = read_db_config(args.config)
    sql = build_sql(account_names, blueprints, quest_ids, password, world=args.world,
                    chars_per_account=args.chars_per_account)
    mysql_run(cfg, sql)
    print(mysql_run(cfg, verify_sql(account_names)))
    if password is not None:
        print("login password: %s (from MS_TEST_ACCOUNT_PASSWORD)" % password)
    else:
        print("login password: each account's own name")


if __name__ == "__main__":
    main()
