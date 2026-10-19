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
  meso   -> CLOSE_RANGE_ATTACK (0x2C) with Meso Explosion's meso-id layout

Two packet-layout quirks are flagged per skill (mirroring the server's
AbstractDealDamageHandler#parseDamage):
  - charge skills read one extra int32 before the rest of the payload;
  - Meso Explosion (4211006) has its own meso-id based layout, so it is
    marked kind "meso" / special "meso_explosion" (entry-level "kind" key
    is only emitted when it overrides the job default).
Skills the heuristic cannot see (no wz damage key) but the server treats
as attacks are force-included: Snipe (3221007, server-fixed damage) and
Meso Explosion itself.

Every entry also carries "serverBuff": true when the server's own
SkillFactory isBuff switch (link_repos/MapleStory-Server/src/client/
SkillFactory.java, ids resolved from src/constants/skills/*.java) forces
the skill to load as a buff - parsed at runtime via server_buff_ids(),
never hardcoded here.

Skill names: Skill.wz carries no <string name="name"> in this dump, so the
String.wz/Skill.img.xml table is used as fallback. Output is written to
attack_skills.json next to this script, plus a stdout summary table.
"""
import json
import os
import re
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
OPCODE_BY_KIND = {"melee": "0x2C", "ranged": "0x2D", "magic": "0x2E", "meso": "0x2C"}

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
# packet layout (kind "meso"); 3221007 Snipe is server-fixed damage and
# carries no wz damage key. Both are force-included as attacks.
SPECIAL_BY_SKILL = {4211006: "meso_explosion", 3221007: "snipe_fixed_damage"}
KIND_OVERRIDE_BY_SKILL = {4211006: "meso"}
FORCE_ATTACK_SKILLS = frozenset(SPECIAL_BY_SKILL)

# ---------------------------------------------------------------------------
# Server-buff classification - parsed at runtime from the server source so
# this manifest stays the single source of truth (no hardcoded id sets).
# SkillFactory.loadFromData computes `isBuff` for every skill; the
# authoritative part is its `switch (id)` override right before the level
# effects are loaded, whose case labels are constants/skills/*.java ids.
# ---------------------------------------------------------------------------
SERVER_REPO = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "..",
    "link_repos", "MapleStory-Server",
)
SKILL_FACTORY_JAVA = os.path.join(SERVER_REPO, "src", "client", "SkillFactory.java")
SKILLS_CONSTANTS_DIR = os.path.join(SERVER_REPO, "src", "constants", "skills")
MAPLE_STAT_EFFECT_JAVA = os.path.join(SERVER_REPO, "src", "server", "MapleStatEffect.java")
CLIENT_SKILLID_H = "/data/maplestory-wasm/src/client/Character/SkillId.h"
CLIENT_SKILLDATA_CPP = "/data/maplestory-wasm/src/client/Data/SkillData.cpp"

_SWITCH_HEAD_RE = re.compile(r"switch\s*\(id\)\s*\{")
_CASE_OR_ASSIGN_RE = re.compile(
    r"case\s+([A-Za-z_][A-Za-z0-9_.]*)\s*:|isBuff\s*=\s*(true|false)\s*;"
)
_CONST_DEF_RE = re.compile(r"\b([A-Z][A-Z0-9_]*)\s*=\s*(\d+)\s*;")


@lru_cache(maxsize=None)
def _skill_constants():
    """{"Class.CONSTANT": skill id} from every constants/skills/*.java."""
    consts = {}
    for name in sorted(os.listdir(SKILLS_CONSTANTS_DIR)):
        if not name.endswith(".java"):
            continue
        path = os.path.join(SKILLS_CONSTANTS_DIR, name)
        with open(path, "r", encoding="utf-8") as handle:
            src = handle.read()
        for match in _CONST_DEF_RE.finditer(src):
            consts["%s.%s" % (name[:-5], match.group(1))] = int(match.group(2))
    return consts


@lru_cache(maxsize=None)
def _isbuff_switch_groups():
    """(forced-true ids, forced-false ids) of the isBuff switch.

    The switch sits between `switch (id) {` and the `MapData level` read
    that follows it; case labels accumulate until the next `isBuff = ...;`
    assignment, which closes the group.
    """
    with open(SKILL_FACTORY_JAVA, "r", encoding="utf-8") as handle:
        src = handle.read()
    # `switch (id)` appears twice (a small action switch sits above); the
    # isBuff override is the tightest switch whose body, up to the
    # `MapData level` read that terminates loadFromData's classification
    # block, holds the isBuff assignments.
    candidates = []
    for head in _SWITCH_HEAD_RE.finditer(src):
        end = src.find("MapData level", head.end())
        body = src[head.end():end if end > 0 else len(src)]
        if re.search(r"isBuff\s*=", body):
            candidates.append((len(body), body))
    if not candidates:
        raise RuntimeError("no isBuff switch in %s" % SKILL_FACTORY_JAVA)
    _len, body = min(candidates)
    consts = _skill_constants()
    groups, labels = [], []
    for match in _CASE_OR_ASSIGN_RE.finditer(body):
        if match.group(1) is not None:
            labels.append(match.group(1))
            continue
        groups.append((match.group(2), labels))
        labels = []
    if labels or sorted(value for value, _ in groups) != ["false", "true"]:
        raise RuntimeError(
            "unexpected isBuff switch shape in %s (groups=%r, dangling=%r)"
            % (SKILL_FACTORY_JAVA, [value for value, _ in groups], labels)
        )
    resolved = {}
    for value, case_labels in groups:
        ids = {consts[label] for label in case_labels}
        missing = [label for label in case_labels if label not in consts]
        if missing:
            raise RuntimeError("unresolved skill constants: %s" % missing)
        resolved[value] = frozenset(ids)
    return resolved["true"], resolved["false"]


def server_buff_ids():
    """Skill ids the server's SkillFactory isBuff switch forces to true.

    MapleStatEffects of these skills are loaded as buffs, so an "attack"
    packet for them never settles damage on the server.
    """
    return _isbuff_switch_groups()[0]


@lru_cache(maxsize=None)
def summon_ids():
    """Skill ids that spawn a summon (MapleStatEffect statup switch groups
    containing MapleBuffStat.SUMMON; case labels resolve via constants)."""
    with open(MAPLE_STAT_EFFECT_JAVA, "r", encoding="utf-8") as handle:
        src = handle.read()
    consts = _skill_constants()
    found = set()
    for segment in src.split("break;"):
        if "MapleBuffStat.SUMMON" not in segment:
            continue
        for match in re.finditer(r"case\s+([A-Za-z_][A-Za-z0-9_.]*)\s*:", segment):
            label = match.group(1)
            if label not in consts:
                raise RuntimeError("unresolved skill constant: %s" % label)
            found.add(consts[label])
    if not found:
        raise RuntimeError("no summon groups in %s" % MAPLE_STAT_EFFECT_JAVA)
    return frozenset(found)


@lru_cache(maxsize=None)
def client_attack_ids():
    """Skill ids the WASM client itself plays as a local attack
    (SkillData::flags_of ATTACK table; SkillId.h resolves the constants)."""
    with open(CLIENT_SKILLID_H, "r", encoding="utf-8") as handle:
        enum_src = handle.read()
    ids = {m.group(1): int(m.group(2))
           for m in re.finditer(r"([A-Z_0-9]+)\s*=\s*(\d+)", enum_src)}
    with open(CLIENT_SKILLDATA_CPP, "r", encoding="utf-8") as handle:
        table_src = handle.read()
    found = {ids[m.group(1)]
             for m in re.finditer(r"\{\s*SkillId::([A-Z_0-9]+),\s*ATTACK\b[^}]*\}", table_src)
             if m.group(1) in ids}
    if not found:
        raise RuntimeError("empty client ATTACK table in %s" % CLIENT_SKILLDATA_CPP)
    if len(found) < 30:
        sys.stderr.write("warning: client ATTACK table shrank to %d ids (expect ~59); "
                         "skip counts may be inflated\n" % len(found))
    return frozenset(found)


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


def attack_record(sid, name, level, kv, kind, default_kind, buff_ids, summon_set, client_atk):
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
        # SkillFactory.isBuff()==true: server loads the effect as a buff,
        # so an attack packet for it cannot settle damage.
        "serverBuff": sid in buff_ids,
        # MapleStatEffect adds a MapleBuffStat.SUMMON statup: the damage
        # comes from the summon's own attacks, not the player's packet.
        "summon": sid in summon_set,
        "clientAttack": sid in client_atk or sid in FORCE_ATTACK_SKILLS,
    }
    if kind != default_kind:
        record["kind"] = kind
    return record


def job_entry(job, string_names):
    """(jobcode, {"suffix","kind","skills"}, total skills in line)."""
    _code, job_id, imgs = job[0], job[1], job[2]
    suffix = job[-1]
    default_kind = KIND_BY_JOB[job_id]
    buff_ids = server_buff_ids()
    summon_set = summon_ids()
    client_set = client_attack_ids()
    attacks = []
    total = 0
    for sid, skill in line_skills(imgs):
        total += 1
        level, kv = highest_level(skill)
        kind = KIND_OVERRIDE_BY_SKILL.get(sid, default_kind)
        if not is_attack(kv) and sid not in FORCE_ATTACK_SKILLS:
            continue
        record = attack_record(sid, skill_name(skill, string_names), level, kv, kind, default_kind, buff_ids, summon_set, client_set)
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
    """job code | suffix | total | attacks | serverBuff marks on stdout."""
    header = ("job", "suffix", "total", "attacks", "buffs")
    print("%-5s %-6s %6s %8s %6s" % header)
    for code, entry in manifest.items():
        buffs = sum(1 for s in entry["skills"] if s["serverBuff"])
        print("%-5s %-6s %6d %8d %6d" % (code, entry["suffix"], totals[code], len(entry["skills"]), buffs))
    kinds = sorted({e["kind"] for e in manifest.values()}
                   | {s.get("kind", e["kind"]) for e in manifest.values() for s in e["skills"]})
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
