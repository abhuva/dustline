"""Validate shop content and resolve deterministic per-town inventories."""
from __future__ import annotations

import hashlib
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CATALOG_PATH = ROOT / 'data/shop-items.json'
FAMILIES = ('upgrade', 'front', 'side', 'top')
MAX_STOCK = 9
MAX_SAVE_ID = 127
TOWN_COUNT = 6
CONTENT_ID = re.compile(r'^[a-z][a-z0-9_]*$')
UPGRADES = {'salvage_magnet': 0, 'tuned_injector': 1, 'reinforced_plating': 2}
WEAPONS = {
    'sides': ('side', 2),
    'missile': ('top', 3),
    'trap': ('top', 4),
    'radio': ('top', 6),
    'sniper': ('front', 7),
    'front_shooter': ('side', 8),
}
WEAPON_ICON_FRAMES = {'sides': 2, 'missile': 3, 'trap': 4, 'radio': 6,
                      'sniper': 7, 'front_shooter': 8}
DEFAULT_PROFILE = {
    'tierFloor': 1,
    'townTierRange': [1, 1],
    'stockSize': 9,
    'mixWeights': {'upgrade': 1, 'front': 1, 'side': 1, 'top': 1},
    'townModifiers': [],
}
TOWN_NAMES = (
    'DUSTHAVEN', 'IRONWELL', 'RED MESA', 'ASH CROSS', 'RUSTPOINT', 'DRY CREEK',
    'CINDER REST', 'GREYRIDGE', 'SALT YARD', 'COPPER RUN', 'BLACK PUMP', 'OLD SPAN',
    'TIN ROOF', 'HOLLOW WELL', 'WEST RELAY', 'BRASS GATE', 'LOW RIDGE', 'SCRAPFORD',
    'BONE ROAD', 'NIGHT POST', 'SUNDER', 'DEAD RADIO', 'BURNT FORD', 'LAST LIGHT',
)


def _integer(value, low, high, label):
    if type(value) is not int or not low <= value <= high:
        raise ValueError(f'{label} must be an integer between {low} and {high}')
    return value


def _text(value, maximum, label, pattern=None):
    if not isinstance(value, str) or not value or len(value) > maximum or (pattern and not pattern.fullmatch(value)):
        raise ValueError(f'{label} is invalid')
    return value


def load_catalog(path=CATALOG_PATH, map_ids=None):
    catalog = json.loads(Path(path).read_text(encoding='utf8'))
    validate_catalog(catalog, map_ids)
    return catalog


def validate_catalog(catalog, map_ids=None):
    if not isinstance(catalog, dict) or catalog.get('version') != 1:
        raise ValueError('Unsupported shop catalog version')
    _integer(catalog.get('generationVersion'), 1, 65535, 'Shop generation version')
    reserved = catalog.get('reservedSaveIds')
    if not isinstance(reserved, list) or not reserved:
        raise ValueError('Shop catalog needs reservedSaveIds')
    for value in reserved:
        _integer(value, 0, MAX_SAVE_ID, 'Reserved shop save ID')
    if len(set(reserved)) != len(reserved):
        raise ValueError('Reserved shop save IDs must be unique')
    items = catalog.get('items')
    if not isinstance(items, list) or not items:
        raise ValueError('Shop catalog needs at least one item')
    ids, save_ids = set(), set()
    known_maps = set(map_ids) if map_ids is not None else None
    for index, item in enumerate(items):
        if not isinstance(item, dict):
            raise ValueError(f'Shop item {index} must be an object')
        identity = _text(item.get('id'), 32, f'Shop item {index} ID', CONTENT_ID)
        if identity in ids:
            raise ValueError(f'Duplicate shop item ID: {identity}')
        ids.add(identity)
        save_id = _integer(item.get('saveId'), 0, MAX_SAVE_ID, f'Shop item {identity} saveId')
        if save_id in save_ids:
            raise ValueError(f'Duplicate shop save ID: {save_id}')
        if save_id not in reserved:
            raise ValueError(f'Shop item {identity} uses unreserved save ID {save_id}')
        save_ids.add(save_id)
        _text(item.get('name'), 20, f'Shop item {identity} name')
        _text(item.get('detail'), 24, f'Shop item {identity} detail')
        _integer(item.get('credits'), 0, 99999, f'Shop item {identity} credits')
        _integer(item.get('scrap'), 0, 999, f'Shop item {identity} scrap')
        _integer(item.get('tier'), 1, 255, f'Shop item {identity} tier')
        family = item.get('family')
        if family not in FAMILIES:
            raise ValueError(f'Shop item {identity} has an invalid family')
        kind = item.get('kind')
        if kind == 'upgrade':
            effect = item.get('upgrade')
            if effect not in UPGRADES or family != 'upgrade' or item.get('icon') != effect:
                raise ValueError(f'Shop upgrade {identity} has incompatible effect, family or icon')
            if 'weapon' in item:
                raise ValueError(f'Shop upgrade {identity} must not define a weapon')
        elif kind == 'weapon':
            effect = item.get('weapon')
            if effect not in WEAPONS or family != WEAPONS[effect][0] or item.get('icon') != effect:
                raise ValueError(f'Shop weapon {identity} has incompatible effect, family or icon')
            if 'upgrade' in item:
                raise ValueError(f'Shop weapon {identity} must not define an upgrade')
        else:
            raise ValueError(f'Shop item {identity} has an invalid kind')
        policy = item.get('regions')
        if not isinstance(policy, dict) or policy.get('mode') not in ('all', 'only', 'except'):
            raise ValueError(f'Shop item {identity} has an invalid region policy')
        region_ids = policy.get('ids', [])
        if not isinstance(region_ids, list) or any(not isinstance(value, str) for value in region_ids) or \
           len(region_ids) != len(set(region_ids)):
            raise ValueError(f'Shop item {identity} has invalid or duplicate region IDs')
        if policy['mode'] == 'all' and region_ids:
            raise ValueError(f'Shop item {identity} uses region IDs with mode all')
        if policy['mode'] != 'all' and not region_ids:
            raise ValueError(f'Shop item {identity} needs at least one region ID')
        if known_maps is not None:
            unknown = set(region_ids) - known_maps
            if unknown:
                raise ValueError(f'Shop item {identity} references unknown map {sorted(unknown)[0]}')
    # IDs are append-only. Missing reserved positions are intentional tombstones.
    return catalog


def normalize_profile(value=None):
    source = DEFAULT_PROFILE if value is None else value
    if not isinstance(source, dict):
        raise ValueError('Shop profile must be an object')
    required = {'tierFloor', 'townTierRange', 'stockSize', 'mixWeights'}
    if not required.issubset(source):
        raise ValueError('Shop profile is missing required fields')
    floor = _integer(source['tierFloor'], 1, 255, 'Shop tier floor')
    tier_range = source['townTierRange']
    if not isinstance(tier_range, list) or len(tier_range) != 2:
        raise ValueError('Shop townTierRange must have two values')
    low = _integer(tier_range[0], 1, 255, 'Shop minimum town tier')
    high = _integer(tier_range[1], low, 255, 'Shop maximum town tier')
    if floor > high:
        raise ValueError('Shop tier floor cannot exceed the maximum town tier')
    stock = _integer(source['stockSize'], 1, MAX_STOCK, 'Shop stock size')
    weights = _weights(source['mixWeights'], 'Shop family weights', complete=True)
    modifiers = source.get('townModifiers', [])
    if not isinstance(modifiers, list) or len(modifiers) > TOWN_COUNT:
        raise ValueError(f'Shop profile can define at most {TOWN_COUNT} town modifiers')
    normalized_modifiers, towns = [], set()
    for modifier in modifiers:
        if not isinstance(modifier, dict):
            raise ValueError('Shop town modifiers must be objects')
        town = _integer(modifier.get('town'), 0, TOWN_COUNT - 1, 'Shop modifier town')
        if town in towns:
            raise ValueError(f'Shop profile has duplicate modifier for town {town}')
        towns.add(town)
        result = {'town': town, 'tierOffset': _integer(modifier.get('tierOffset', 0), -254, 254,
                                                       f'Shop town {town} tier offset'),
                  'stockDelta': _integer(modifier.get('stockDelta', 0), -MAX_STOCK, MAX_STOCK,
                                         f'Shop town {town} stock delta')}
        if 'mixWeights' in modifier:
            result['mixWeights'] = _weights(modifier['mixWeights'], f'Shop town {town} weights', complete=False)
        normalized_modifiers.append(result)
    return {'tierFloor': floor, 'townTierRange': [low, high], 'stockSize': stock,
            'mixWeights': weights, 'townModifiers': normalized_modifiers}


def _weights(value, label, complete):
    if not isinstance(value, dict) or (complete and set(value) != set(FAMILIES)) or any(key not in FAMILIES for key in value):
        raise ValueError(f'{label} must use upgrade/front/side/top keys')
    result = {key: _integer(weight, 0, 100, f'{label} {key}') for key, weight in value.items()}
    if complete and not any(result.values()):
        raise ValueError(f'{label} cannot all be zero')
    return result


def _rank(*parts):
    digest = hashlib.sha256()
    for part in parts:
        encoded = str(part).encode('utf8')
        digest.update(len(encoded).to_bytes(2, 'little'))
        digest.update(encoded)
    return digest.digest()


def town_name(map_id, town):
    _integer(town, 0, TOWN_COUNT - 1, 'Town index')
    value = 2166136261
    for byte in map_id.encode('utf8'):
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return TOWN_NAMES[(value + town) % len(TOWN_NAMES)]


def _allowed(item, map_id):
    policy = item['regions']
    return policy['mode'] == 'all' or (map_id in policy['ids']) == (policy['mode'] == 'only')


def resolve_inventory(catalog, map_id, seed, town, profile=None):
    validate_catalog(catalog)
    profile = normalize_profile(profile)
    _integer(seed, 0, 0xffffffff, 'Map seed')
    _integer(town, 0, TOWN_COUNT - 1, 'Town index')
    generation = catalog['generationVersion']
    low, high = profile['townTierRange']
    cap = low + int.from_bytes(_rank('tier', generation, map_id, seed, town)[:8], 'little') % (high - low + 1)
    stock = profile['stockSize']
    weights = dict(profile['mixWeights'])
    modifier = next((value for value in profile['townModifiers'] if value['town'] == town), None)
    if modifier:
        cap = max(1, min(255, cap + modifier['tierOffset']))
        stock = max(0, min(MAX_STOCK, stock + modifier['stockDelta']))
        weights.update(modifier.get('mixWeights', {}))
    if not any(weights.values()):
        raise ValueError(f'Map {map_id} town {town} has no positive shop family weight')
    eligible = [item for item in catalog['items'] if _allowed(item, map_id) and
                profile['tierFloor'] <= item['tier'] <= cap]
    if len(eligible) < stock:
        raise ValueError(f'Map {map_id} town {town} requests {stock} shop items but only {len(eligible)} are eligible')
    tie_order = sorted(FAMILIES, key=lambda family: _rank('family', generation, map_id, seed, town, family))
    tie_index = {family: index for index, family in enumerate(tie_order)}
    current = {family: 0 for family in FAMILIES}
    total = sum(weights.values())
    schedule = []
    for _ in range(stock):
        for family in FAMILIES:
            current[family] += weights[family]
        family = min(FAMILIES, key=lambda value: (-current[value], tie_index[value]))
        current[family] -= total
        schedule.append(family)
    pools = {family: sorted((item for item in eligible if item['family'] == family),
                            key=lambda item: _rank('candidate', generation, map_id, seed, town, item['id']))
             for family in FAMILIES}
    selected, used = [], set()
    for family in schedule:
        candidate = next((item for item in pools[family] if item['saveId'] not in used), None)
        if candidate is None:
            remaining = [item for item in eligible if item['saveId'] not in used]
            candidate = min(remaining, key=lambda item: _rank('fallback', generation, map_id, seed, town, item['id']))
        selected.append(candidate)
        used.add(candidate['saveId'])
    return {'tierCap': cap, 'stockSize': stock, 'schedule': schedule,
            'items': [item['id'] for item in selected],
            'saveIds': [item['saveId'] for item in selected]}


def generate_header(catalog=None, path=None):
    catalog = load_catalog() if catalog is None else validate_catalog(catalog)
    path = ROOT / 'include/generated/shop_catalog.h' if path is None else Path(path)
    rows = []
    for frame, item in enumerate(catalog['items']):
        if item['kind'] == 'upgrade':
            value = f'int(upgrade::{item["upgrade"]})'
        else:
            value = f'int(combat::Weapon::{item["weapon"]})'
        rows.append('    {' + ','.join((json.dumps(item['id']), json.dumps(item['name']),
                    json.dumps(item['detail']), str(item['credits']), str(item['scrap']),
                    str(item['saveId']), str(frame), f'kind::{item["kind"]}', value,
                    f'family::{item["family"]}')) + '},')
    text = '''// Generated from data/shop-items.json. Do not edit.
#pragma once
#include <cstdint>
#include "combat.h"
namespace garage_shop {
enum class kind : uint8_t { upgrade,weapon };
enum class family : uint8_t { upgrade,front,side,top };
enum class upgrade : uint8_t { salvage_magnet,tuned_injector,reinforced_plating };
struct item {
    const char* id; const char* name; const char* detail;
    int credits; int scrap; uint8_t save_id; uint8_t icon_frame;
    kind type; int value; family group;
};
inline constexpr item catalog[]={
''' + '\n'.join(rows) + '''
};
inline constexpr int count=sizeof(catalog)/sizeof(catalog[0]);
inline constexpr int max_save_id=''' + str(max(catalog['reservedSaveIds'])) + ''';
inline constexpr int generation_version=''' + str(catalog['generationVersion']) + ''';
}
'''
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding='utf8')
    return path


if __name__ == '__main__':
    generate_header()
