"""Shared map catalog validation and loopback persistence regression test."""
import copy
import json
import tempfile
import threading
from http.server import ThreadingHTTPServer
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import Request, urlopen

import serve_map_editor
from compile_recipe import ROOT, validate_library


library = json.loads((ROOT / 'maps/map-library.json').read_text())
enabled = validate_library(copy.deepcopy(library))
assert len(enabled) == sum(entry['includeInGame'] for entry in library['maps'])
assert library['world']['start'] == {'map': 'wasteland', 'spawn': 'start'}
assert len(library['world']['nodes']) == len(enabled)
connection_count = len(library['world']['connections'])
assert connection_count > 0

broken_world = copy.deepcopy(library)
broken_world['world']['connections'].pop()
validate_library(broken_world, release=False)
assert len(broken_world['world']['connections']) == connection_count - 1

bad_side = copy.deepcopy(library)
bad_side['world']['connections'][0]['b']['side'] = 'north'
try:
    validate_library(bad_side, release=False)
    raise AssertionError('Non-complementary cardinal connection passed validation')
except ValueError as error:
    assert 'complementary' in str(error)

disabled = copy.deepcopy(library)
disabled_id = 'two-rules-radial'
next(entry for entry in disabled['maps'] if entry['id'] == disabled_id)['includeInGame'] = False
validate_library(disabled)
assert disabled_id not in {node['map'] for node in disabled['world']['nodes']}
assert all(link['a']['map'] != disabled_id and link['b']['map'] != disabled_id
           for link in disabled['world']['connections'])

disabled_start = copy.deepcopy(library)
next(entry for entry in disabled_start['maps'] if entry['id'] == 'wasteland')['includeInGame'] = False
validate_library(disabled_start, release=False)
assert disabled_start['world']['start']['map'] != 'wasteland'
assert disabled_start['world']['start']['map'] in {entry['id'] for entry in disabled_start['maps']
                                                    if entry['includeInGame']}

new_region = copy.deepcopy(library)
new_entry = copy.deepcopy(next(entry for entry in library['maps'] if entry['id'] == 'wasteland'))
new_entry['id'] = 'new-region'
new_entry['recipe']['name'] = 'New region'
new_region['maps'].append(new_entry)
validate_library(new_region, release=False)
assert any(node['map'] == 'new-region' for node in new_region['world']['nodes'])
assert all(link['a']['map'] != 'new-region' and link['b']['map'] != 'new-region'
           for link in new_region['world']['connections'])
try:
    validate_library(copy.deepcopy(new_region))
    raise AssertionError('Disconnected release world passed validation')
except ValueError as error:
    assert 'connect every' in str(error)

draft = {
    'id': 'empty-draft',
    'includeInGame': False,
    'recipe': {'version': 6, 'name': 'Empty draft', 'seed': 42, 'nodes': [], 'portals': [], 'playerSpawns': []},
}
with_draft = copy.deepcopy(library)
with_draft['maps'].append(copy.deepcopy(draft))
validate_library(with_draft)
with_draft['maps'][-1]['includeInGame'] = True
try:
    validate_library(with_draft)
    raise AssertionError('Enabled empty draft passed validation')
except ValueError as error:
    assert 'graph is empty' in str(error)

with tempfile.TemporaryDirectory() as folder:
    path = Path(folder) / 'map-library.json'
    path.write_text(json.dumps(library, indent=2) + '\n')
    serve_map_editor.LIBRARY_PATH = path
    server = ThreadingHTTPServer(('127.0.0.1', 0), serve_map_editor.Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    url = f'http://127.0.0.1:{server.server_port}/api/library'
    try:
        envelope = json.loads(urlopen(url).read())
        preview_body = json.dumps({'mapId': 'wasteland', 'seed': 12648431,
                                   'shopProfile': library['maps'][0]['recipe'].get('shopProfile')}).encode()
        preview = json.loads(urlopen(Request(f'http://127.0.0.1:{server.server_port}/api/shop-preview',
                                             data=preview_body, method='POST',
                                             headers={'Content-Type': 'application/json'})).read())
        assert [[item['saveId'] for item in town['items']] for town in preview['towns']] == [
            [0, 8, 4, 5, 1], [4, 5, 8, 1], [1, 5, 8, 4],
            [3, 1, 8, 4], [4, 0, 8, 5], [8, 3, 4, 0],
        ]
        assert preview['towns'][0]['name'] == 'SUNDER'
        updated = copy.deepcopy(envelope['library'])
        updated['maps'].append(draft)
        body = json.dumps({'revision': envelope['revision'], 'library': updated}).encode()
        try:
            result = json.loads(urlopen(Request(url, data=body, method='PUT', headers={'Content-Type': 'application/json'})).read())
        except HTTPError as error:
            raise AssertionError(error.read().decode()) from error
        assert result['library']['maps'][-1]['id'] == 'empty-draft'
        assert json.loads(path.read_text()) == updated
        try:
            urlopen(Request(url, data=body, method='PUT', headers={'Content-Type': 'application/json'}))
            raise AssertionError('Stale library revision overwrote the file')
        except HTTPError as error:
            assert error.code == 409
    finally:
        server.shutdown()
        server.server_close()
        thread.join()

print(f'PASS shared map library: {len(enabled)} compiled maps, partial/pruned world graph, draft persistence, atomic API and conflict guard.')
