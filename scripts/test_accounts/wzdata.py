"""Read-only parsers for the MapleStory server XML wz dumps.

Provides the ground-truth data needed to build level-200 test characters:
quest id list (Quest.wz), per-job skill rosters with max levels (Skill.wz),
equip requirements/stat lines (Character.wz) and throwing-star/bullet data
(Item.wz). All functions are pure readers; nothing is written back.
"""
import os
import xml.etree.ElementTree as ET

WZ_ROOT = os.environ.get(
    "MS_WZ_PATH", "/data/maplestory-wasm/link_repos/MapleStory-Server/wz"
)

EQUIP_DIRS = ("Weapon", "Shield", "Cap", "Longcoat", "Glove", "Shoes", "Cape", "Accessory")

_KEEP_TAGS = ("int", "string", "short", "double")


def imgdir(node, name):
    return node.find(f'imgdir[@name="{name}"]') if node is not None else None


def scalar_kv(node):
    if node is None:
        return {}
    return {c.get("name"): c.get("value") for c in node if c.tag in _KEEP_TAGS}


def _to_int(value, fallback=0):
    try:
        return int(float(value))
    except (TypeError, ValueError):
        return fallback


def equip_path(itemid):
    for sub in EQUIP_DIRS:
        path = os.path.join(WZ_ROOT, "Character.wz", sub, "0%d.img.xml" % itemid)
        if os.path.isfile(path):
            return path
    return None


def load_equip_info(itemid):
    """Return wz info for one equip: islot/tuc, req* fields and inc* stat lines."""
    path = equip_path(itemid)
    if path is None:
        return None
    root = ET.parse(path).getroot()
    info = imgdir(root, "info")
    kv = scalar_kv(info)
    req = imgdir(info, "req")
    kv.update(scalar_kv(req))
    return {
        "itemid": itemid,
        "islot": kv.get("islot", ""),
        "tuc": _to_int(kv.get("tuc"), 0),
        "req_job": _to_int(kv.get("reqJob")),
        "req_level": _to_int(kv.get("reqLevel")),
        "req_str": _to_int(kv.get("reqSTR")),
        "req_dex": _to_int(kv.get("reqDEX")),
        "req_int": _to_int(kv.get("reqINT")),
        "req_luk": _to_int(kv.get("reqLUK")),
        "inc_str": _to_int(kv.get("incSTR")),
        "inc_dex": _to_int(kv.get("incDEX")),
        "inc_int": _to_int(kv.get("incINT")),
        "inc_luk": _to_int(kv.get("incLUK")),
        "inc_pad": _to_int(kv.get("incPAD")),
        "inc_mad": _to_int(kv.get("incMAD")),
    }


def load_equip_infos(itemids):
    return {iid: load_equip_info(iid) for iid in itemids}


def load_quest_ids():
    """All quest ids defined in Quest.wz/QuestInfo.img.xml."""
    path = os.path.join(WZ_ROOT, "Quest.wz", "QuestInfo.img.xml")
    root = ET.parse(path).getroot()
    return sorted(
        int(d.get("name"))
        for d in root
        if d.get("name", "").isdigit()
    )


def _skills_in_img(img_name, id_filter=None):
    path = os.path.join(WZ_ROOT, "Skill.wz", "%s.img.xml" % img_name)
    root = ET.parse(path).getroot()
    skills = []
    for skill in imgdir(root, "skill") or []:
        sid = _to_int(skill.get("name"))
        if id_filter is not None and not id_filter(sid):
            continue
        levels = imgdir(skill, "level")
        max_level = len(list(levels)) if levels is not None else 1
        skills.append((sid, max(1, max_level)))
    return skills


def load_job_line_skills(img_names):
    """[(skillid, max_level)] for every skill of a job's 1st..4th job imgs."""
    skills = []
    for img in img_names:
        skills.extend(_skills_in_img(img))
    return skills


def load_beginner_skills():
    """Common explorer beginner skills from 000.img (GM-range ids excluded)."""
    return _skills_in_img("000", id_filter=lambda sid: 1000 <= sid < 8000)


def load_throw_info(kind):
    """Best attack throwing star ('star') or bullet ('bullet') from Item.wz."""
    file_map = {"star": "0207", "bullet": "0233"}
    path = os.path.join(WZ_ROOT, "Item.wz", "Consume", "%s.img.xml" % file_map[kind])
    root = ET.parse(path).getroot()
    best = None
    for node in root:
        info = imgdir(node, "info")
        kv = scalar_kv(info)
        pad = _to_int(kv.get("incPAD"))
        req_level = _to_int(kv.get("reqLevel"))
        slot_max = max(1, _to_int(kv.get("slotMax"), 1))
        itemid = _to_int(node.get("name"))
        if pad <= 0 or req_level > 200:
            continue
        if best is None or pad > best[1]:
            best = (itemid, pad, slot_max)
    return best


def load_consume_slot_max(itemid):
    """slotMax of a consumable from Item.wz/Consume (files group 10k ids)."""
    prefix = itemid // 10000
    path = os.path.join(WZ_ROOT, "Item.wz", "Consume", "%04d.img.xml" % prefix)
    root = ET.parse(path).getroot()
    key = str(itemid)
    for node in root:
        if node.get("name", "").lstrip("0") != key:
            continue
        kv = scalar_kv(imgdir(node, "info"))
        return max(1, _to_int(kv.get("slotMax"), 100))
    return 100
