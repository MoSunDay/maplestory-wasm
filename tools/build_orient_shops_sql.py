#!/usr/bin/env python3
"""为东方神州/泰国商店 NPC 生成 shops/shopitems SQL（含双侧物品存在性校验）。

数据源：079 参考库 CongMS干净数据.sql 的模板商店（新加坡武器/防具/药品商），
映射到神州/泰国本地商店 NPC，shopid == npcid，position 采用 hub 惯例 104+4k。

校验（任一侧缺失则丢弃该物品并报告）：
  * 服务端 Item.wz/Consume/<02XX>.img.xml 含 name="<iid:08d>"（药品/消耗品）
  * 服务端 Character.wz/<Type>/<iid:08d>.img.xml 存在（装备）
  * 服务端 String.wz Consume.img.xml / Eqp.img.xml 含 name="<iid>"（裸 id）
  * 客户端 assets/Item.nx 节点名含 "<iid:08d>"、Character.nx 含 "<iid:08d>.img"
"""
import argparse
import re
import sys
from pathlib import Path

sys.path[:0] = ['tools/nx']
from merge_skill_nx import parse_nx  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
REF_SQL = ROOT / 'link_repos/.ref-079max4/079MAX4-2023年度版/怀旧岛079MAX4服务端/SQL/CongMS干净数据.sql'
SERVER_WZ = ROOT / 'link_repos/MapleStory-Server/wz'
OUT_SQL = ROOT / 'link_repos/MapleStory-Server/sql/custom_orient_shops.sql'

# 新 shopid(=npcid) -> 079 模板 shopid
SHOP_MAP = {
    9310001: 78,   # 上海 李女士 武器商  <- 079 78 (9330048, 48 武器)
    9310002: 79,   # 上海 张老板 防具商  <- 079 79 (9330047, 86 防具)
    9310003: 95,   # 上海 金女士 杂货    <- 079 95 (9270021, 30 药品)
    9250007: 62,   # 泰国 丹 武器商      <- 079 62 (9270019, 42 武器)
    9250008: 60,   # 泰国 帕恩 药品      <- 079 60 (9270022, 30 药品/杂货)
    9250009: 63,   # 泰国 诺伊 防具      <- 079 63 (9270020, 83 防具)
}

ROW_RE = re.compile(
    r"INSERT INTO `shopitems` VALUES \('(\d+)', '(\d+)', '(\d+)', '(\d+)', "
    r"'(\d+)', '(\d+)', '\d+', '\d+'\);")


def parse_079_shopitems(sql_path, template_ids):
    """解析 079 shopitems，返回 {shopid: [(pos, itemid, price), ...]}（按 pos 排序）。"""
    shops = {sid: [] for sid in template_ids}
    for line in sql_path.read_text(encoding='utf-8', errors='replace').splitlines():
        m = ROW_RE.match(line.strip())
        if not m:
            continue
        _, shopid, itemid, price, _, pos = m.groups()
        sid = int(shopid)
        if sid in shops:
            shops[sid].append((int(pos), int(itemid), int(price)))
    return {sid: sorted(rows) for sid, rows in shops.items()}


def load_server_consume_index():
    """{零填充节点名 -> True}，扫描 Item.wz/Consume/*.img.xml 顶层条目。"""
    names = {}
    consume_dir = SERVER_WZ / 'Item.wz/Consume'
    for f in consume_dir.glob('*.img.xml'):
        text = f.read_text(encoding='utf-8', errors='replace')
        for m in re.finditer(r'name="(\d{8})"', text):
            names.setdefault(m.group(1), True)
    return names


def load_server_equip_index():
    """{零填充文件基名 -> 分类目录}，扫描 Character.wz/<Type>/。"""
    index = {}
    for cat_dir in (SERVER_WZ / 'Character.wz').iterdir():
        if not cat_dir.is_dir():
            continue
        for f in cat_dir.glob('*.img.xml'):
            index.setdefault(f.name[:-len('.img.xml')], cat_dir.name)
    return index


def load_server_string_index():
    """{裸 itemid} 集合，取自 String.wz Consume/Eqp 两表。"""
    ids = set()
    for img in ('Consume.img.xml', 'Eqp.img.xml'):
        text = (SERVER_WZ / 'String.wz' / img).read_text(
            encoding='utf-8', errors='replace')
        ids.update(int(m.group(1)) for m in re.finditer(r'name="(\d+)"', text))
    return ids


def nx_node_names(nx_path):
    nx = parse_nx(str(nx_path))
    strings = nx['strings']

    def name_of(node):
        raw = strings[node[0]]
        return raw.decode('utf-8', 'replace') if isinstance(raw, bytes) else raw

    return {name_of(n) for n in nx['nodes']}


def build_validators():
    """返回 (validate, report)：validate(iid) -> None 或缺失原因字符串。"""
    consume_idx = load_server_consume_index()
    equip_idx = load_server_equip_index()
    string_idx = load_server_string_index()
    item_nx = nx_node_names(ROOT / 'assets/Item.nx')
    char_nx = nx_node_names(ROOT / 'assets/Character.nx')

    def validate(iid):
        padded = '%08d' % iid
        if iid not in string_idx:
            return 'no String.wz name'
        if str(iid).startswith('1'):
            if padded not in equip_idx:
                return 'no server Character.wz'
            if padded + '.img' not in char_nx:
                return 'no client Character.nx'
            return None
        if str(iid).startswith('2'):
            if padded not in consume_idx:
                return 'no server Item.wz/Consume'
            if padded not in item_nx:
                return 'no client Item.nx'
            return None
        return 'unknown item prefix'

    return validate


def build_shop_rows(template_shops, validate):
    """过滤双侧缺失物品，返回 {new_shopid: [(itemid, price, position), ...]}。"""
    result, dropped = {}, []
    for new_sid, tpl_sid in sorted(SHOP_MAP.items()):
        rows, pos = [], 104
        for _, iid, price in template_shops[tpl_sid]:
            reason = validate(iid)
            if reason is not None:
                dropped.append((new_sid, iid, reason))
                continue
            rows.append((iid, price, pos))
            pos += 4
        result[new_sid] = rows
    return result, dropped


HEADER = """\
-- 东方神州（上海）与泰国本地商店 NPC 的 shops/shopitems 数据。
-- shopid == npcid；物品与价格取自 079 参考库（CongMS干净数据.sql）的
-- 新加坡同类商店模板，双侧（服务端 WZ + 客户端 NX）存在性已校验。
-- 映射：{map_lines}
-- 本脚本可重复执行：先清空六家商店物品，再幂等写入商店和物品。
-- 注意：MapleShopFactory.getShopForNPC 是懒加载 + 负缓存，
--       执行后必须 docker restart ms-game-server 才会生效。
"""


def emit_sql(path, shop_rows):
    map_lines = '\n-- '.join(
        '%d <- 079 shop %d（%d 件商品）' % (sid, SHOP_MAP[sid], len(rows))
        for sid, rows in sorted(shop_rows.items()))
    out = [HEADER.format(map_lines=map_lines)]
    shop_ids = ', '.join(str(sid) for sid in sorted(shop_rows))
    out.append('DELETE FROM `shopitems` WHERE `shopid` IN (%s);\n' % shop_ids)
    out.append('DELETE FROM `shops` WHERE `shopid` IN (%s);\n' % shop_ids)
    values = ['(%d, %d)' % (sid, sid) for sid in sorted(shop_rows)]
    out.append('INSERT INTO `shops` (`shopid`, `npcid`) VALUES\n%s\n'
               'ON DUPLICATE KEY UPDATE `npcid` = VALUES(`npcid`);\n'
               % ',\n'.join(values))
    for sid, rows in sorted(shop_rows.items()):
        if not rows:
            continue
        item_values = ',\n'.join(
            '(%d, %d, %d, 0, %d)' % (sid, iid, price, pos)
            for iid, price, pos in rows)
        out.append('INSERT INTO `shopitems` (`shopid`, `itemid`, `price`, '
                   '`pitch`, `position`) VALUES\n%s;\n' % item_values)
    path.write_text('\n'.join(out), encoding='utf-8')
    return len('\n'.join(out).splitlines())


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--dry-run', action='store_true',
                    help='只校验并打印报告，不写 SQL')
    args = ap.parse_args()

    template_shops = parse_079_shopitems(REF_SQL, set(SHOP_MAP.values()))
    for sid in sorted(SHOP_MAP.values()):
        if not template_shops.get(sid):
            print('FATAL: 079 模板商店 %d 未找到任何物品' % sid)
            return 1

    shop_rows, dropped = build_shop_rows(template_shops, build_validators())
    for sid, rows in sorted(shop_rows.items()):
        src = len(template_shops[SHOP_MAP[sid]])
        print('shop %d: 保留 %d / %d' % (sid, len(rows), src))
    for sid, iid, reason in dropped:
        print('  DROP %d item %d: %s' % (sid, iid, reason))

    total = sum(len(r) for r in shop_rows.values())
    print('总计保留 %d 件（丢弃 %d 件）' % (total, len(dropped)))
    if not args.dry_run:
        out = OUT_SQL
        lines = emit_sql(out, shop_rows)
        print('已写入 %s（%d 行）' % (out, lines))
    return 0


if __name__ == '__main__':
    sys.exit(main())
