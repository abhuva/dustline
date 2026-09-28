"""Validate per-map art-bank identity, budgets and binding rules."""
import copy
import hashlib
import json

from art_profiles import load_catalog, profiles_for_entries, validate_profile
from compile_recipe import load_library

catalog=load_catalog()
library,enabled,_=load_library()
profiles,indices,keys=profiles_for_entries(enabled,catalog)
assert len(keys)==len(enabled)
assert len(profiles)==len(set(keys))
assert set(indices.values())==set(range(len(profiles)))

art=json.loads((open('tools/map_editor/generated/art.json')).read())
assert set(art['mapBanks'])=={entry['id'] for entry in library['maps']}
assert len(art['banks'])>=len(profiles)
signatures=set()
for bank in art['banks']:
    assert bank['uniqueTiles']<=bank['tileSlots']<=256
    assert len(bank['materialTextures'])==len(bank['materialSurfaces'])==len(bank['materialBehaviors'])==256
    assert len(bank['materialNames'])==4 and all(bank['materialNames'])
    assert len(bank['decoration'])==17*64
    signatures.add(hashlib.sha256(bytes(bank['tiles'])+bytes(bank['palette'])).digest())
assert len(signatures)==len(art['banks'])

custom=copy.deepcopy(profiles[0]);custom['materials'][1]['id']=219;custom['decorations'][2]['enabled']=False
assert validate_profile(custom,catalog)['materials'][1]['id']==219
duplicate=copy.deepcopy(custom);duplicate['materials'][1]['id']=duplicate['materials'][0]['id']
try:
    validate_profile(duplicate,catalog)
    raise AssertionError('Duplicate material IDs were accepted')
except ValueError as error:
    assert 'Duplicate' in str(error)

print(f'PASS {len(profiles)} distinct runtime art banks, per-map reuse, budgets, arbitrary IDs and binding validation.')
