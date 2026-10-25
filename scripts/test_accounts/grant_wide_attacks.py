#!/usr/bin/env python3
"""Grant the 12 wide attack skills to the ready-to-test characters.

Companion of create_accounts.py: create_accounts.py only inserts the server's
job-line skills (wzdata.load_job_line_skills), so the wide attack skills that
the client/NX side ships get their `skills` rows here (skilllevel 10,
masterlevel 10, expiration -1), exactly once per character.

Targets are the 12 explorer 4th-job characters in plans.JOBS order, grouped
4 per account like create_accounts.py names them:
  test1: Hero, Pala, DK, FPA
  test2: ILA, Bish, BM, MM
  test3: NL, Shad, Bucc, CS

Usage:
  python3 grant_wide_attacks.py               # dry-run: ids + SQL, no writes
  python3 grant_wide_attacks.py --apply       # delete stale row, insert, verify

DB credentials are read at runtime from the server configuration.ini
(never hardcoded), same as create_accounts.py.
"""
import argparse
import sys

from create_accounts import mysql_run, read_db_config
from plans import JOBS

DEFAULT_CONFIG = "/data/maplestory-wasm/link_repos/MapleStory-Server/configuration.ini"
CHARS_PER_ACCOUNT = 4   # same grouping as create_accounts.py's --chars-per-account
SKILL_LEVEL = 10
MASTER_LEVEL = 10
EXPIRATION = -1

# One wide attack skill per explorer 4th-job line, keyed by plans.JOBS suffix.
WIDE_ATTACK_SKILL = {
    "Hero": 1121012,
    "Pala": 1221013,
    "DK": 1321011,
    "FPA": 2121009,
    "ILA": 2221009,
    "Bish": 2321010,
    "BM": 3121011,
    "MM": 3221009,
    "NL": 4121010,
    "Shad": 4221009,
    "Bucc": 5121011,
    "CS": 5221011,
}


def fail(message, code=2):
    """Print one clear error line and exit non-zero."""
    print("error: %s" % message, file=sys.stderr)
    raise SystemExit(code)


def sql_quoted(value):
    """Single-quoted SQL string literal."""
    return "'%s'" % str(value).replace("'", "''")


def build_targets(prefix):
    """[{name, suffix, skill}] for the 12 test characters, plans.JOBS order.

    Account numbering mirrors create_accounts.py:
    prefix + (JOBS index // CHARS_PER_ACCOUNT + 1) + suffix.
    """
    targets = []
    for index, job in enumerate(JOBS):
        suffix = job[-1]
        account = "%s%d" % (prefix, index // CHARS_PER_ACCOUNT + 1)
        targets.append({
            "name": account + suffix,
            "suffix": suffix,
            "skill": WIDE_ATTACK_SKILL.get(suffix),
        })
    return targets


def unmapped_suffixes(targets):
    """Suffixes of plans.JOBS that have no wide attack skill id."""
    return [target["suffix"] for target in targets if target["skill"] is None]


def missing_names(targets, ids):
    """Target character names absent from the characters table."""
    return [target["name"] for target in targets if target["name"] not in ids]


def fetch_character_ids(cfg, world, names):
    """{name: character id} for names that exist in `characters`, world-scoped."""
    sql = "SELECT name, id FROM characters WHERE world = %d AND name IN (%s);" % (
        world, ",".join(sql_quoted(name) for name in names))
    try:
        out = mysql_run(cfg, sql, flags=("-N", "-B"))
    except RuntimeError as error:
        fail("character lookup failed: %s" % error)
    ids = {}
    for line in out.splitlines():
        if not line.strip():
            continue
        name, _, character_id = line.partition("\t")
        ids[name.strip()] = int(character_id.strip())
    return ids


def grant_sql(character_id, skill_id):
    """One mysql stream per character: drop any stale row, then insert 10/10/-1.

    Column order matches create_accounts.py:
    INSERT INTO skills (skillid, characterid, skilllevel, masterlevel, expiration).
    """
    return (
        "DELETE FROM skills WHERE characterid=%d AND skillid=%d;\n"
        "INSERT INTO skills (skillid, characterid, skilllevel, masterlevel, expiration)"
        " VALUES (%d, %d, %d, %d, %d);" % (
            character_id, skill_id,
            skill_id, character_id, SKILL_LEVEL, MASTER_LEVEL, EXPIRATION)
    )


def verify_sql(character_id, skill_id):
    """Rows proving the grant landed exactly once with the expected values."""
    return ("SELECT skillid, skilllevel, masterlevel, expiration FROM skills"
            " WHERE characterid=%d AND skillid=%d;" % (character_id, skill_id))


def parse_rows(out):
    """Tab-separated mysql output (`-N -B`) into tuples of ints."""
    return [
        tuple(int(cell) for cell in line.split("\t"))
        for line in out.splitlines() if line.strip()
    ]


def verify_grant(cfg, character_id, skill_id):
    """(ok, rows): ok only when exactly one 10/10/-1 row exists for the skill."""
    expected = (skill_id, SKILL_LEVEL, MASTER_LEVEL, EXPIRATION)
    rows = parse_rows(mysql_run(cfg, verify_sql(character_id, skill_id),
                                flags=("-N", "-B")))
    return rows == [expected], rows


def print_dry_run(targets, ids):
    """Show what --apply would do: character id plus the exact SQL stream."""
    for target in targets:
        character_id = ids[target["name"]]
        print("dry-run %s (id %d) skill %d:" % (
            target["name"], character_id, target["skill"]))
        for line in grant_sql(character_id, target["skill"]).splitlines():
            print("  " + line)
    print("dry-run: %d character(s) untouched; rerun with --apply to write"
          % len(targets))


def apply_grants(cfg, targets, ids):
    """Execute each character's grant, then verify the resulting row."""
    expected_text = "[(skillid, %d, %d, %d)]" % (
        SKILL_LEVEL, MASTER_LEVEL, EXPIRATION)
    for target in targets:
        character_id = ids[target["name"]]
        skill_id = target["skill"]
        try:
            mysql_run(cfg, grant_sql(character_id, skill_id))
            ok, rows = verify_grant(cfg, character_id, skill_id)
        except RuntimeError as error:
            fail("grant failed for %s (id %d, skill %d): %s" % (
                target["name"], character_id, skill_id, error))
        if not ok:
            fail("verify failed for %s (id %d, skill %d): want exactly %s, got %r"
                 % (target["name"], character_id, skill_id, expected_text, rows))
        print("applied %s (id %d): skill %d at level %d/%d verified" % (
            target["name"], character_id, skill_id, SKILL_LEVEL, MASTER_LEVEL))
    print("applied: %d character(s) granted and verified" % len(targets))


def build_parser():
    parser = argparse.ArgumentParser(
        description="Grant the 12 wide attack skills to the test characters"
                    " (dry-run unless --apply).")
    parser.add_argument("--world", type=int, default=4,
                        help="world the test characters live in (default 4)")
    parser.add_argument("--prefix", default="test",
                        help="account name prefix (default test)")
    parser.add_argument("--config", default=DEFAULT_CONFIG,
                        help="game server configuration.ini path")
    parser.add_argument("--apply", action="store_true",
                        help="write the skills rows (default: dry-run)")
    return parser


def main():
    args = build_parser().parse_args()
    targets = build_targets(args.prefix)
    if not targets:
        fail("plans.JOBS is empty; nothing to grant")
    unmapped = unmapped_suffixes(targets)
    if unmapped:
        fail("no wide attack skill id mapped for suffix(es): %s"
             % ",".join(unmapped))

    cfg = read_db_config(args.config)
    ids = fetch_character_ids(cfg, args.world, [t["name"] for t in targets])
    missing = missing_names(targets, ids)
    if missing:
        fail("%d character(s) not found in world %d: %s"
             % (len(missing), args.world, ",".join(missing)))

    if args.apply:
        apply_grants(cfg, targets, ids)
    else:
        print_dry_run(targets, ids)


if __name__ == "__main__":
    main()
