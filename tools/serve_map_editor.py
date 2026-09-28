"""Loopback-only map workshop server with one bounded library write API."""
import argparse
import importlib
import json
import os
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit

import compile_recipe
import shop_catalog
from music_generator import MUSIC_PATH, generate as generate_music, revision as music_revision, validate_music

ROOT = Path(__file__).resolve().parents[1]
LIBRARY_PATH = compile_recipe.LIBRARY_PATH


class Handler(SimpleHTTPRequestHandler):
    extensions_map = {**SimpleHTTPRequestHandler.extensions_map, '.mjs': 'text/javascript', '.wasm': 'application/wasm'}

    def _json(self, status, value):
        data = json.dumps(value).encode('utf8')
        self.send_response(status)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        request_path = urlsplit(self.path).path
        if request_path == '/api/library':
            try:
                data = LIBRARY_PATH.read_bytes()
                self._json(200, {'revision': compile_recipe.library_revision(data), 'library': json.loads(data)})
            except Exception as error:
                self._json(500, {'error': str(error)})
            return
        if request_path == '/api/music':
            try:
                data = MUSIC_PATH.read_bytes()
                self._json(200, {'revision': music_revision(data), 'music': json.loads(data),
                                 'preview': '/generated/music-preview.wav'})
            except Exception as error:
                self._json(500, {'error': str(error)})
            return
        super().do_GET()

    def do_PUT(self):
        request_path = urlsplit(self.path).path
        if request_path == '/api/music':
            try:
                length = int(self.headers.get('Content-Length', '0'))
                if length <= 0 or length > 256 * 1024:
                    raise ValueError('Music request must be between 1 byte and 256 KiB')
                request = json.loads(self.rfile.read(length))
                current = MUSIC_PATH.read_bytes()
                if request.get('revision') != music_revision(current):
                    self._json(409, {'error': 'The music source changed on disk. Reload before saving.'})
                    return
                music = validate_music(request.get('music'))
                encoded = (json.dumps(music, indent=2) + '\n').encode('utf8')
                temporary = MUSIC_PATH.with_suffix('.json.tmp')
                temporary.write_bytes(encoded)
                os.replace(temporary, MUSIC_PATH)
                generate_music()
                self._json(200, {'revision': music_revision(encoded), 'music': music,
                                 'preview': '/generated/music-preview.wav'})
            except (ValueError, TypeError, json.JSONDecodeError) as error:
                self._json(400, {'error': str(error)})
            except Exception as error:
                self._json(500, {'error': str(error)})
            return
        if request_path != '/api/library':
            self._json(404, {'error': 'Unknown endpoint'})
            return
        try:
            length = int(self.headers.get('Content-Length', '0'))
            if length <= 0 or length > 1024 * 1024:
                raise ValueError('Map library request must be between 1 byte and 1 MiB')
            request = json.loads(self.rfile.read(length))
            # The workshop is commonly left running while its compiler changes.
            # Reload it for each save so client and server recipe versions cannot drift.
            compiler = importlib.reload(compile_recipe)
            current = LIBRARY_PATH.read_bytes()
            if request.get('revision') != compiler.library_revision(current):
                self._json(409, {'error': 'The map library changed on disk. Reload before saving.'})
                return
            library = request.get('library')
            compiler.validate_library(library, release=False)
            encoded = (json.dumps(library, indent=2) + '\n').encode('utf8')
            temporary = LIBRARY_PATH.with_suffix('.json.tmp')
            temporary.write_bytes(encoded)
            os.replace(temporary, LIBRARY_PATH)
            self._json(200, {'revision': compiler.library_revision(encoded), 'library': library})
        except (ValueError, TypeError, json.JSONDecodeError) as error:
            self._json(400, {'error': str(error)})
        except Exception as error:
            self._json(500, {'error': str(error)})

    def do_POST(self):
        if urlsplit(self.path).path != '/api/shop-preview':
            self._json(404, {'error': 'Unknown endpoint'})
            return
        try:
            length = int(self.headers.get('Content-Length', '0'))
            if length <= 0 or length > 64 * 1024:
                raise ValueError('Shop preview request must be between 1 byte and 64 KiB')
            request = json.loads(self.rfile.read(length))
            library = json.loads(LIBRARY_PATH.read_bytes())
            map_ids = {entry['id'] for entry in library['maps']}
            map_id = request.get('mapId')
            if map_id not in map_ids:
                raise ValueError('Shop preview references an unknown map')
            profile = shop_catalog.normalize_profile(request.get('shopProfile'))
            catalog = shop_catalog.load_catalog(map_ids=map_ids)
            by_save_id = {item['saveId']: item for item in catalog['items']}
            towns = []
            for town in range(shop_catalog.TOWN_COUNT):
                resolved = shop_catalog.resolve_inventory(catalog, map_id, request.get('seed'), town, profile)
                towns.append({'index': town, 'name': shop_catalog.town_name(map_id, town),
                              'tierCap': resolved['tierCap'],
                              'items': [{'saveId': save_id, 'id': by_save_id[save_id]['id'],
                                         'name': by_save_id[save_id]['name'],
                                         'family': by_save_id[save_id]['family'],
                                         'tier': by_save_id[save_id]['tier']}
                                        for save_id in resolved['saveIds']]})
            self._json(200, {'profile': profile, 'towns': towns})
        except (ValueError, TypeError, json.JSONDecodeError) as error:
            self._json(400, {'error': str(error)})
        except Exception as error:
            self._json(500, {'error': str(error)})

    def translate_path(self, path):
        path = unquote(urlsplit(path).path)
        base = ROOT / ('maps/recipes' if path.startswith('/recipes/') else 'tools/map_editor')
        relative = path[len('/recipes/'):] if path.startswith('/recipes/') else path.lstrip('/')
        candidate = (base / relative).resolve()
        if not candidate.is_relative_to(base.resolve()):
            return str(base / '__not_found__')
        return str(candidate)

    def end_headers(self):
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=8765)
    args = parser.parse_args()
    print(f'Dustline Workshop: http://127.0.0.1:{args.port} (maps) /music.html (music)', flush=True)
    ThreadingHTTPServer(('127.0.0.1', args.port), partial(Handler)).serve_forever()
