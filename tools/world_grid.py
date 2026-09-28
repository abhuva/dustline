"""Validation and derivation for the reciprocal cardinal world graph."""

import re
from collections import deque

SIDES = ('north', 'east', 'south', 'west')
SIDE_INDEX = {side: index for index, side in enumerate(SIDES)}
OPPOSITE = {'north': 'south', 'east': 'west', 'south': 'north', 'west': 'east'}
DELTA = {'north': (0, -1), 'east': (1, 0), 'south': (0, 1), 'west': (-1, 0)}
CONNECTION_ID = re.compile(r'^[a-z0-9]+(?:[-_][a-z0-9]+)*$')


def reconcile_world(library, enabled, player_spawns):
    """Prune disabled maps while preserving the authored v3 graph and canvas layout."""
    enabled_ids = {entry['id'] for entry in enabled}
    existing = library.get('world')
    if not isinstance(existing, dict) or existing.get('version') != 3:
        existing = {}
    positioned = {}
    raw_nodes = existing.get('nodes')
    if isinstance(raw_nodes, list):
        for node in raw_nodes:
            if isinstance(node, dict) and node.get('map') in enabled_ids and node['map'] not in positioned:
                positioned[node['map']] = node
    nodes = []
    for index, entry in enumerate(enabled):
        previous = positioned.get(entry['id'], {})
        x = previous.get('x', 80 + (index % 4) * 250)
        y = previous.get('y', 70 + (index // 4) * 210)
        nodes.append({'map': entry['id'], 'x': x, 'y': y})

    connections = []
    raw_connections = existing.get('connections')
    if isinstance(raw_connections, list):
        for connection in raw_connections[:128]:
            if not isinstance(connection, dict):
                continue
            a, b = connection.get('a'), connection.get('b')
            if isinstance(a, dict) and isinstance(b, dict) and a.get('map') in enabled_ids and b.get('map') in enabled_ids:
                connections.append(connection)

    start = existing.get('start') if isinstance(existing.get('start'), dict) else {}
    start_map = start.get('map') if start.get('map') in enabled_ids else enabled[0]['id']
    spawn_ids = player_spawns.get(start_map, {})
    start_spawn = start.get('spawn') if start.get('spawn') in spawn_ids else next(iter(spawn_ids), None)
    library['world'] = {'version': 3, 'start': {'map': start_map, 'spawn': start_spawn},
                        'nodes': nodes, 'connections': connections}


def validate_world(world, enabled_ids, player_spawns, release=True):
    """Validate a v3 graph and return derived grid coordinates and side neighbours."""
    if not isinstance(world, dict) or world.get('version') != 3:
        raise ValueError('World must use cardinal grid version 3')
    if not isinstance(enabled_ids, (set, frozenset)):
        enabled_ids = set(enabled_ids)
    start = world.get('start')
    if not isinstance(start, dict) or start.get('map') not in enabled_ids:
        raise ValueError('World start must name an enabled map')
    if start.get('spawn') not in player_spawns.get(start['map'], {}):
        raise ValueError('World start must name a player spawn on its map')

    nodes = world.get('nodes')
    if not isinstance(nodes, list) or len(nodes) != len(enabled_ids):
        raise ValueError('World nodes must contain every enabled map exactly once')
    node_ids = set()
    for node in nodes:
        if not isinstance(node, dict) or node.get('map') not in enabled_ids or node['map'] in node_ids:
            raise ValueError('World nodes must contain every enabled map exactly once')
        if type(node.get('x')) is not int or type(node.get('y')) is not int:
            raise ValueError('World canvas coordinates must be integers')
        node_ids.add(node['map'])
    if node_ids != enabled_ids:
        raise ValueError('World nodes must contain every enabled map exactly once')

    raw_connections = world.get('connections')
    if not isinstance(raw_connections, list) or len(raw_connections) > 128:
        raise ValueError('World must contain at most 128 connections')
    occupied, ids, adjacency = set(), set(), {map_id: [] for map_id in enabled_ids}
    neighbours = {map_id: [-1, -1, -1, -1] for map_id in enabled_ids}
    ordered_ids = {map_id: index for index, map_id in enumerate(sorted(enabled_ids))}
    for connection in raw_connections:
        if not isinstance(connection, dict):
            raise ValueError('World connections must be objects')
        identity = connection.get('id')
        if not isinstance(identity, str) or len(identity) > 96 or not CONNECTION_ID.fullmatch(identity) or identity in ids:
            raise ValueError('World connection IDs must be unique and path-safe')
        ids.add(identity)
        if connection.get('requirement') is not None:
            raise ValueError(f'Connection {identity} has an unsupported requirement')
        a, b = connection.get('a'), connection.get('b')
        if not isinstance(a, dict) or not isinstance(b, dict):
            raise ValueError(f'Connection {identity} needs two endpoints')
        a_map, b_map, a_side, b_side = a.get('map'), b.get('map'), a.get('side'), b.get('side')
        if a_map not in enabled_ids or b_map not in enabled_ids or a_map == b_map:
            raise ValueError(f'Connection {identity} must join two enabled maps')
        if a_side not in SIDES or b_side != OPPOSITE.get(a_side):
            raise ValueError(f'Connection {identity} must use complementary cardinal sides')
        if (a_map, a_side) in occupied or (b_map, b_side) in occupied:
            raise ValueError(f'Connection {identity} reuses an occupied map side')
        occupied.update(((a_map, a_side), (b_map, b_side)))
        dx, dy = DELTA[a_side]
        adjacency[a_map].append((b_map, dx, dy, identity))
        adjacency[b_map].append((a_map, -dx, -dy, identity))
        neighbours[a_map][SIDE_INDEX[a_side]] = ordered_ids[b_map]
        neighbours[b_map][SIDE_INDEX[b_side]] = ordered_ids[a_map]

    coordinates, components = {}, {}
    roots = [start['map'], *(map_id for map_id in sorted(enabled_ids) if map_id != start['map'])]
    component = 0
    for root in roots:
        if root in coordinates:
            continue
        coordinates[root] = (0, 0)
        components[root] = component
        queue = deque([root])
        occupied_coordinates = {(0, 0): root}
        while queue:
            source = queue.popleft()
            sx, sy = coordinates[source]
            for target, dx, dy, identity in adjacency[source]:
                expected = (sx + dx, sy + dy)
                if target in coordinates:
                    if components[target] == component and coordinates[target] != expected:
                        raise ValueError(f'Connection {identity} creates an inconsistent grid cycle')
                    continue
                other = occupied_coordinates.get(expected)
                if other is not None and other != target:
                    raise ValueError(f'Connection {identity} overlaps maps {other} and {target}')
                coordinates[target] = expected
                components[target] = component
                occupied_coordinates[expected] = target
                queue.append(target)
        component += 1
    if release and any(value != components[start['map']] for value in components.values()):
        raise ValueError('Release world must connect every enabled map to the start map')
    return {'coordinates': coordinates, 'components': components, 'neighbours': neighbours,
            'orderedMaps': sorted(enabled_ids)}
