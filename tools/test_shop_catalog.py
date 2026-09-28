"""Host regression tests for shared deterministic shop catalog rules."""
import copy
import json

from compile_recipe import ROOT
from shop_catalog import DEFAULT_PROFILE, load_catalog, normalize_profile, resolve_inventory, validate_catalog


library = json.loads((ROOT / 'maps/map-library.json').read_text())
map_ids = {entry['id'] for entry in library['maps']}
catalog = load_catalog(map_ids=map_ids)
assert len(catalog['items']) == 9
assert [item['saveId'] for item in catalog['items']] == list(range(9))

first = resolve_inventory(catalog, 'wasteland', 12648431, 0, DEFAULT_PROFILE)
second = resolve_inventory(copy.deepcopy(catalog), 'wasteland', 12648431, 0, copy.deepcopy(DEFAULT_PROFILE))
assert first == second and len(first['saveIds']) == 9 and len(set(first['saveIds'])) == 9
assert set(first['saveIds']) == set(range(9))

reordered = copy.deepcopy(catalog)
reordered['items'].reverse()
assert resolve_inventory(reordered, 'wasteland', 12648431, 0, DEFAULT_PROFILE) == first

regional = copy.deepcopy(catalog)
regional['items'][0]['regions'] = {'mode': 'only', 'ids': ['wasteland']}
regional['items'][1]['regions'] = {'mode': 'except', 'ids': ['twin-cities']}
validate_catalog(regional, map_ids)

for mutation, fragment in (
    (lambda value: value['items'][1].update(saveId=0), 'Duplicate shop save ID'),
    (lambda value: value['items'][0].update(saveId=10), 'unreserved save ID'),
    (lambda value: value['items'][0]['regions'].update(mode='only', ids=['missing']), 'unknown map'),
    (lambda value: value['items'][3].update(family='front'), 'incompatible'),
):
    broken = copy.deepcopy(catalog)
    mutation(broken)
    try:
        validate_catalog(broken, map_ids)
        raise AssertionError('Invalid shop catalog passed validation')
    except ValueError as error:
        assert fragment in str(error), error

profile = normalize_profile({'tierFloor': 1, 'townTierRange': [1, 2], 'stockSize': 4,
                             'mixWeights': {'upgrade': 1, 'front': 1, 'side': 1, 'top': 1},
                             'townModifiers': [{'town': 2, 'stockDelta': -1,
                                                'mixWeights': {'top': 5}}]})
assert resolve_inventory(catalog, 'wasteland', 7, 2, profile)['stockSize'] == 3
print('PASS shared shop catalog validation, immutable save IDs and deterministic resolution.')
