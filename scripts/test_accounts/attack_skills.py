#!/usr/bin/env python3
"""Attack-skill manifest generator for the 12 explorer job lines.

Enumerates every skill of each job line's Skill.wz imgs (same enumeration
as plans.py: 1st..4th job img dirs), keeps only ATTACK skills - a skill
counts as an attack when its highest-level entry carries a damage key
("damage" for physical lines, "mad" for magic lines, mirroring the
server's MapleStatEffect damage/matk readers) - and records the
packet-shape metadata needed to synthesize attack
packets for the level-200 test characters:

  melee  -> CLOSE_RANGE_ATTACK (0x2C): jobs 112 122 132 422 512
  ranged -> RANGED_ATTACK      (0x2D): jobs 312 322 412 522
  magic  -> MAGIC_ATTACK       (0x2E): jobs 212 222 232

Two packet-layout quirks are flagged per skill (mirroring the server's
AbstractDealDamageHandler#parseDamage):
  - charge skills read one extra int32 before the rest of the payload;
  - Meso Explosion (4211006) has its own meso-id based layout, so it is
    marked kind "skip" / special "meso_explosion" (entry-level "kind" key
    is only emitted when it overrides the job default).
Skills the heuristic cannot see (no wz damage key) but the server treats
as attacks are force-included: Snipe (3221007, server-fixed damage) and
Meso Explosion itself.

Skill names: Skill.wz carries no <string name="name"> in this dump, so the
String.wz/Skill.img.xml table is used as fallback. Output is written to
attack_skills.json next to this script, plus a stdout summary table.
"""
import json
import os
import sys
import xml.etree.ElementTree as ET
from functools import lru_cache

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from plans import JOBS
from wzdata import WZ_ROOT, _to_int, imgdir, scalar_kv

OUT_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "attack_skills.json")

KIND_BY_JOB = {
    112: "melee", 122: "melee", 132: "melee", 422: "melee", 512: "melee",
    312: "ranged", 322: "ranged", 412: "ranged", 522: "ranged",
    212: "magic", 222: "magic", 232: "magic",
}
OPCODE_BY_KIND = {"melee": "0x2C", "ranged": "0x2D", "magic": "0x2E"}

# Server reads one extra charge int32 for these skill ids; the non-explorer
# entries (ThunderBreaker/NightWalker/Evan) can never match a line here but
# are kept so the set mirrors the server exactly.
CHARGE_SKILLS = frozenset({
    2121001, 2221001, 2321001,  # BIG_BANG (FP/IL/Bishop)
    5201002,                    # GRENADE (Gunslinger, Corsair line)
    5101004,                    # CORKSCREW_BLOW (Brawler, Buccaneer line)
    15101003, 14111006, 22121000, 22151001,
})

# skill id -> special marker. 4211006 Meso Explosion uses its own meso-id
# packet layout ("skip" kind); 3221007 Snipe is server-fixed damage and
# carries no wz damage key. Both are force-included as attacks.
SPECIAL_BY_SKILL = {4211006: "meso_explosion", 3221007: "snipe_fixed_damage"}
KIND_OVERRIDE_BY_SKILL = {4211006: "skip"}
FORCE_ATTACK_SKILLS = frozenset(SPECIAL_BY_SKILL)
SKIP_KIND = "skip"


def load_string_skill_names():
    """{zero-stripped skillid -> name} from String.wz/Skill.img.xml."""
    path = os.path.join(WZ_ROOT, "String.wz", "Skill.img.xml")
    root = ET.parse(path).getroot()
    names = {}
    for node in root:
        nm = node.find('string[@name="name"]')
        if nm is not None:
            names[node.get("name", "").lstrip("0")] = nm.get("value", "")
    return names


@lru_cache(maxsize=None)
def img_skill_nodes(img_name):
    """((skillid, skill element)) for one Skill.wz img, parsed once."""
    path = os.path.join(WZ_ROOT, "Skill.wz", "%s.img.xml" % img_name)
    root = ET.parse(path).getroot()
    skills = imgdir(root, "skill")
    return tuple((_to_int(s.get("name")), s) for s in skills) if skills is not None else ()


def line_skills(img_names):
    """All (skillid, element) of a job line, in img order (as plans.py)."""
    out = []
    for img in img_names:
        out.extend(img_skill_nodes(img))
    return out


def highest_level(skill):
    """(level number, scalar kv) of the skill's highest level entry."""
    levels = imgdir(skill, "level")
    if levels is None:
        return 0, {}
    entries = list(levels)
    if not entries:
        return 0, {}
    top = max(entries, key=lambda c: _to_int(c.get("name")))
    return _to_int(top.get("name")), scalar_kv(top)


def skill_name(skill, string_names):
    """Skill.wz root name if present, else the String.wz table, else ''."""
    node = skill.find('string[@name="name"]')
    if node is not None:
        return node.get("value", "")
    return string_names.get(str(skill.get("name", "")).lstrip("0"), "")


def is_attack(kv):
    # physical attacks carry "damage", magic attacks carry "mad" (the
    # server's MapleStatEffect reads both: damage / matk).
    return "damage" in kv or "mad" in kv


def attack_record(sid, name, level, kv, kind, default_kind):
    """One manifest entry; "kind" only kept when it overrides the default."""
    special = SPECIAL_BY_SKILL.get(sid, "")
    record = {
        "id": sid,
        "name": name,
        "level": level,
        "mobCount": _to_int(kv.get("mobCount"), 1),
        "attackCount": _to_int(kv.get("attackCount"), 1),
        "mpCon": _to_int(kv.get("mpCon"), 0),
        "cooldown": _to_int(kv.get("cooldown") or kv.get("time"), 0),
        "charge": sid in CHARGE_SKILLS,
        "special": special,
    }
    if kind != default_kind:
        record["kind"] = kind
    return record


def job_entry(job, string_names):
    """(jobcode, {"suffix","kind","skills"}, total skills in line)."""
    _code, job_id, imgs = job[0], job[1], job[2]
    suffix = job[-1]
    default_kind = KIND_BY_JOB[job_id]
    attacks = []
    total = 0
    for sid, skill in line_skills(imgs):
        total += 1
        level, kv = highest_level(skill)
        kind = KIND_OVERRIDE_BY_SKILL.get(sid, default_kind)
        if not is_attack(kv) and sid not in FORCE_ATTACK_SKILLS:
            continue
        record = attack_record(sid, skill_name(skill, string_names), level, kv, kind, default_kind)
        attacks.append(record)
    attacks.sort(key=lambda r: r["id"])
    return str(job_id), {
        "suffix": suffix,
        "kind": default_kind,
        "skills": attacks,
    }, total


def build_manifest(jobs):
    """Ordered {jobcode -> entry} plus per-job totals for the summary."""
    string_names = load_string_skill_names()
    manifest, totals = {}, {}
    for job in jobs:
        code, entry, total = job_entry(job, string_names)
        manifest[code] = entry
        totals[code] = total
    return manifest, totals


def print_summary(manifest, totals):
    """job code | suffix | total skills | attack skills table on stdout."""
    header = ("job", "suffix", "total", "attacks")
    print("%-5s %-6s %6s %8s" % header)
    for code, entry in manifest.items():
        print("%-5s %-6s %6d %8d" % (code, entry["suffix"], totals[code], len(entry["skills"])))
    kinds = sorted({e["kind"] for e in manifest.values()})
    ops = ", ".join("%s=%s" % (k, OPCODE_BY_KIND[k]) for k in kinds)
    print("kinds: %s" % ops)


def main():
    manifest, totals = build_manifest(JOBS)
    with open(OUT_PATH, "w") as fh:
        json.dump(manifest, fh, indent=2)
        fh.write("\n")
    print_summary(manifest, totals)
    print("wrote %s" % OUT_PATH)


if __name__ == "__main__":
    main()
