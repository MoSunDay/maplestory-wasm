#!/usr/bin/env python3
"""Extract the full cosmetic style pools from the server wz XML dumps.

Sources of truth (under link_repos/MapleStory-Server/wz):
  - Character.wz/Hair/*.img.xml, Character.wz/Face/*.img.xml
    -> asset existence (a style without an image dump cannot be rendered)
  - String.wz/Eqp.img.xml sections Eqp/Hair and Eqp/Face
    -> item names; the server NPC helper cm.getCosmeticItem() only returns
       ids that have a name entry, so only named ids are usable in pools.

Output: beauty_styles.json next to this script, with four sorted lists:
  male_hair   / female_hair : base hair ids (id % 10 == 0)
  male_face   / female_face : one representative id per face style
                               (style = id % 100, eye color = hundreds digit)

ID scheme (v83):
  hair = base*10 + color(0-7); male 30xxx/33xxx, female 31xxx/34xxx
  face = 2<gender><eyecolor 0-8><style 00-32>; male 20xxx, female 21xxx
NPC scripts re-add the player's own hair color (hair % 10) and face eye
color (face % 1000 - face % 100) themselves, so pools only carry the
base/representative ids.
"""

import json
import os
import re
import sys
import xml.etree.ElementTree as ET

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
DEFAULT_SERVER_ROOT = os.path.join(REPO_ROOT, "link_repos", "MapleStory-Server")
OUTPUT_JSON = os.path.join(SCRIPT_DIR, "beauty_styles.json")

MALE_HAIR_RANGES = ((30000, 31000), (33000, 34000))
FEMALE_HAIR_RANGES = ((31000, 32000), (34000, 35000))
MALE_FACE_RANGES = ((20000, 21000),)
FEMALE_FACE_RANGES = ((21000, 22000),)


def list_asset_ids(assets_dir):
    """Return the set of numeric ids present in a Character.wz subdirectory."""
    ids = set()
    for entry in os.listdir(assets_dir):
        m = re.match(r"^0*(\d+)\.img\.xml$", entry)
        if m:
            ids.add(int(m.group(1)))
    return ids


def load_named_ids(eqp_xml_path, section):
    """Return the set of ids that have a name string inside Eqp.img/<section>."""
    tree = ET.parse(eqp_xml_path)
    root = tree.getroot()
    found = set()
    for eqp in root.iter("imgdir"):
        if eqp.get("name") != "Eqp":
            continue
        for child in eqp.findall("imgdir"):
            if child.get("name") != section:
                continue
            for item in child.findall("imgdir"):
                name = next(
                    (s.get("value") for s in item.findall("string")
                     if s.get("name") == "name"),
                    None,
                )
                if name and re.match(r"^\d+$", item.get("name") or ""):
                    found.add(int(item.get("name")))
    if not found:
        raise RuntimeError("no named ids parsed from %s section %s"
                           % (eqp_xml_path, section))
    return found


def in_ranges(item_id, ranges):
    return any(lo <= item_id < hi for lo, hi in ranges)


def extract_hairs(hair_dir, named):
    """Base hair ids (trailing 0) that exist as assets and carry a name."""
    existing = list_asset_ids(hair_dir)
    usable = sorted(
        item_id for item_id in (existing & named)
        if item_id % 10 == 0
    )
    male = [i for i in usable if in_ranges(i, MALE_HAIR_RANGES)]
    female = [i for i in usable if in_ranges(i, FEMALE_HAIR_RANGES)]
    other = [i for i in usable
             if not in_ranges(i, MALE_HAIR_RANGES + FEMALE_HAIR_RANGES)]
    return male, female, other


def extract_faces(face_dir, named):
    """One representative face id per style (last two digits).

    Preference: the eye-color-0 variant (hundreds digit 0) when it is both
    an existing asset and a named item; otherwise the smallest usable id of
    that style.
    """
    existing = list_asset_ids(face_dir)
    pool = sorted(existing & named)

    def pick(ids):
        for item_id in ids:
            if (item_id // 100) % 10 == 0:
                return item_id
        return min(ids)

    def by_gender(ranges):
        styles = {}
        for item_id in pool:
            if in_ranges(item_id, ranges):
                styles.setdefault(item_id % 100, []).append(item_id)
        return [pick(styles[s]) for s in sorted(styles)]

    male = sorted(by_gender(MALE_FACE_RANGES))
    female = sorted(by_gender(FEMALE_FACE_RANGES))
    other = [i for i in pool
             if not in_ranges(i, MALE_FACE_RANGES + FEMALE_FACE_RANGES)]
    return male, female, other


def main():
    server_root = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SERVER_ROOT
    wz_root = os.path.join(server_root, "wz")
    eqp_xml = os.path.join(wz_root, "String.wz", "Eqp.img.xml")
    hair_dir = os.path.join(wz_root, "Character.wz", "Hair")
    face_dir = os.path.join(wz_root, "Character.wz", "Face")

    hair_named = load_named_ids(eqp_xml, "Hair")
    face_named = load_named_ids(eqp_xml, "Face")

    male_hair, female_hair, hair_other = extract_hairs(hair_dir, hair_named)
    male_face, female_face, face_other = extract_faces(face_dir, face_named)

    styles = {
        "male_hair": male_hair,
        "female_hair": female_hair,
        "male_face": male_face,
        "female_face": female_face,
    }
    with open(OUTPUT_JSON, "w", encoding="utf-8") as fh:
        json.dump(styles, fh, indent=2)
        fh.write("\n")

    print("wrote %s" % OUTPUT_JSON)
    print("male_hair   %3d ids: %s ... %s"
          % (len(male_hair), male_hair[0], male_hair[-1]))
    print("female_hair %3d ids: %s ... %s"
          % (len(female_hair), female_hair[0], female_hair[-1]))
    print("male_face   %3d styles: %s" % (len(male_face), male_face))
    print("female_face %3d styles: %s" % (len(female_face), female_face))
    if hair_other:
        print("note: hair ids outside male/female ranges skipped: %s" % hair_other)
    if face_other:
        print("note: face ids outside male/female ranges skipped: %s" % face_other)


if __name__ == "__main__":
    main()
