"""Per-class build plans for ready-to-test level-200 characters.

Everything here is computed from wz ground truth (see wzdata.py):
- secondary stat = max equip requirement + buffer, primary stat = the rest of
  the level-200 AP pool (16 base + 5 AP x 199 levels = 1011 points),
- Timeless (top-tier v83) gear per class, auto-picked by wz reqJob,
- scroll bonuses modelled as a perfect-scrolled item (tuc passes used).
"""
from wzdata import (
    load_equip_infos,
    load_throw_info,
)

LEVEL = 200
TOTAL_AP = 4 * 4 + (LEVEL - 1) * 5  # 1011 stat points at level 200
BASE_STAT = 4
SECONDARY_BUFFER = 10

CLASS_ID = {"warrior": 1, "mage": 2, "archer": 4, "thief": 8, "pirate": 16}

SLOT_POSITION = {
    "cap": -1, "earring": -4, "coat": -5, "shoes": -7, "glove": -8,
    "cape": -9, "shield": -10, "weapon": -11, "pendant": -17,
}

ARMOR_POOLS = {
    # Timeless 120 armour variants per slot; class picked via wz reqJob.
    "cap": [1002776, 1002777, 1002778, 1002779, 1002780],
    "coat": [1052155, 1052156, 1052157, 1052158, 1052159],
    "glove": [1082234, 1082235, 1082236, 1082237, 1082238],
    "shoes": [1072355, 1072356, 1072357],
}
ANY_JOB_GEARS = {"cape": 1102172, "earring": 1032031, "pendant": 1122012}
FALLBACK_GEARS = {"shoes": 1072344}  # any-job Facestompers when no class variant exists

HPMP_BY_CLASS = {
    "warrior": (30000, 12000),
    "mage": (15000, 30000),
    "archer": (18000, 15000),
    "thief": (18000, 15000),
    "pirate": (18000, 15000),
}

# code, job id, skill img line, timeless weapon, shield, stat plan, char suffix
JOBS = [
    ("Hero", 112, ("100", "110", "111", "112"), 1402046, None, "warrior", "str", "dex", "Hero"),
    ("Paladin", 122, ("100", "120", "121", "122"), 1322060, 1092058, "warrior", "str", "dex", "Pala"),
    ("DarkKnight", 132, ("100", "130", "131", "132"), 1432047, None, "warrior", "str", "dex", "DK"),
    ("FPArchMage", 212, ("200", "210", "211", "212"), 1382057, None, "mage", "int", "luk", "FPA"),
    ("ILArchMage", 222, ("200", "220", "221", "222"), 1382057, None, "mage", "int", "luk", "ILA"),
    ("Bishop", 232, ("200", "230", "231", "232"), 1382057, None, "mage", "int", "luk", "Bish"),
    ("Bowmaster", 312, ("300", "310", "311", "312"), 1452057, None, "archer", "dex", "str", "BM"),
    ("Marksman", 322, ("300", "320", "321", "322"), 1462050, None, "archer", "dex", "str", "MM"),
    ("NightLord", 412, ("400", "410", "411", "412"), 1472068, None, "thief", "luk", "dex", "NL"),
    ("Shadower", 422, ("400", "420", "421", "422"), 1332074, 1092059, "thief", "luk", "dex", "Shad"),
    ("Buccaneer", 512, ("500", "510", "511", "512"), 1482023, None, "pirate", "str", "dex", "Bucc"),
    ("Corsair", 522, ("500", "520", "521", "522"), 1492023, None, "pirate", "dex", "str", "CS"),
]

POTION_IDS = [2000005, 2000005, 2050004, 2022179]  # elixir x2, allcure, onyx apple
POTION_DEFAULT_QTY = {2000005: 200, 2050004: 100, 2022179: 100}


def _stat_key_to_req(info, key):
    return info.get("req_%s" % key, 0)


def _pick_armor(pools, cls, infos):
    chosen = {}
    for slot, itemids in pools.items():
        match = [
            iid for iid in itemids
            if infos.get(iid) and infos[iid]["req_job"] == CLASS_ID[cls]
        ]
        if match:
            chosen[slot] = match[0]
        elif slot in FALLBACK_GEARS and infos.get(FALLBACK_GEARS[slot]):
            chosen[slot] = FALLBACK_GEARS[slot]
        else:
            chosen[slot] = None
    return chosen


def _weapon_bonus(tuc, cls):
    if cls == "mage":
        return {"matk": tuc * 5}
    return {"watk": tuc * 5}


def _glove_bonus(tuc, cls):
    return {"matk": tuc * 3} if cls == "mage" else {"watk": tuc * 3}


def _armor_bonus(tuc, primary):
    bonus = {primary: tuc * 2, "wdef": tuc * 10, "mdef": tuc * 5}
    return bonus


def plan_equips(job):
    """List of (slot, itemid, tuc, bonus dict) for one class, wz-driven."""
    code, _job_id, _imgs, weapon, shield, cls, primary, _secondary, _suffix = job
    armor = _pick_armor(ARMOR_POOLS, cls, ARMOR_INFOS)
    layout = [
        ("weapon", weapon, _weapon_bonus(WEAPON_INFOS[weapon]["tuc"], cls)),
        ("glove", armor["glove"], _glove_bonus(ARMOR_INFOS[armor["glove"]]["tuc"], cls) if armor["glove"] else None),
        ("coat", armor["coat"], _armor_bonus(ARMOR_INFOS[armor["coat"]]["tuc"], primary) if armor["coat"] else None),
        ("cap", armor["cap"], _armor_bonus(ARMOR_INFOS[armor["cap"]]["tuc"], primary) if armor["cap"] else None),
        ("shoes", armor["shoes"], {**_armor_bonus(ARMOR_INFOS[armor["shoes"]]["tuc"], primary), "speed": 8, "jump": 7} if armor["shoes"] else None),
        ("cape", ANY_JOB_GEARS["cape"], _armor_bonus(ARMOR_INFOS[ANY_JOB_GEARS["cape"]]["tuc"], primary)),
        ("earring", ANY_JOB_GEARS["earring"], {"mdef": 20}),
        ("pendant", ANY_JOB_GEARS["pendant"], {"wdef": 15, "mdef": 15}),
    ]
    if shield is not None:
        layout.append(("shield", shield, _armor_bonus(SHIELD_INFOS[shield]["tuc"], primary)))
    out = []
    for slot, iid, bonus in layout:
        if iid is None:
            continue
        tuc = (WEAPON_INFOS if slot == "weapon" else
               SHIELD_INFOS if slot == "shield" else ARMOR_INFOS)[iid]["tuc"]
        out.append((slot, iid, tuc, bonus))
    return out


def compute_stats(job):
    """(str, dex, int, luk) for one class: secondary = equip req + buffer."""
    _code, _job_id, _imgs, _weapon, _shield, _cls, primary, secondary, _suffix = job
    equips = plan_equips(job)
    infos = load_equip_infos([iid for _slot, iid, _t, _b in equips])
    sec_needed = max(
        [BASE_STAT] + [_stat_key_to_req(infos[iid], secondary) for _slot, iid, _t, _b in equips]
    )
    secondary_value = sec_needed + SECONDARY_BUFFER
    primary_value = TOTAL_AP - secondary_value - 2 * BASE_STAT
    stats = {key: BASE_STAT for key in ("str", "dex", "int", "luk")}
    stats[primary] = primary_value
    stats[secondary] = secondary_value
    return stats


def plan_use_items(job):
    """(itemid, quantity) consumables; stars/bullets for throwers."""
    code, _job_id, _imgs, _weapon, _shield, _cls, _p, _s, _suffix = job
    items = [(iid, POTION_DEFAULT_QTY.get(iid, 100)) for iid in POTION_IDS]
    if code == "NightLord":
        star = load_throw_info("star")
        items.append((star[0], star[2]))
    elif code == "Corsair":
        bullet = load_throw_info("bullet")
        items.append((bullet[0], bullet[2]))
    return items


def character_blueprint(job):
    """Full character blueprint for one class."""
    code, job_id, imgs, weapon, shield, cls, primary, secondary, suffix = job
    maxhp, maxmp = HPMP_BY_CLASS[cls]
    return {
        "code": code,
        "suffix": suffix,
        "job": job_id,
        "skill_imgs": imgs,
        "stats": compute_stats(job),
        "maxhp": maxhp,
        "maxmp": maxmp,
        "equips": plan_equips(job),
        "use_items": plan_use_items(job),
    }


def all_itemids():
    ids = set()
    for job in JOBS:
        for _slot, iid, _t, _b in plan_equips(job):
            ids.add(iid)
        for iid, _q in plan_use_items(job):
            if iid > 3000000 or iid // 1000 == 207:
                ids.add(iid)
    return sorted(iids)


def _prime_infos():
    global ARMOR_INFOS, WEAPON_INFOS, SHIELD_INFOS
    armor_ids = [
        iid for pool in list(ARMOR_POOLS.values())
        + [list(ANY_JOB_GEARS.values()), list(FALLBACK_GEARS.values())]
        for iid in pool
    ]
    ARMOR_INFOS = load_equip_infos(armor_ids)
    weapon_ids = [job[3] for job in JOBS]
    shield_ids = [job[4] for job in JOBS if job[4]]
    WEAPON_INFOS = load_equip_infos(weapon_ids)
    SHIELD_INFOS = load_equip_infos(shield_ids)


_prime_infos()


if __name__ == "__main__":
    for job in JOBS:
        bp = character_blueprint(job)
        print(bp["code"], bp["stats"], "wep=%d" % bp["equips"][0][1], "hp/mp=%d/%d" % (bp["maxhp"], bp["maxmp"]))
