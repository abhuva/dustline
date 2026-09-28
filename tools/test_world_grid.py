#!/usr/bin/env python3
"""Regression tests for cardinal world graph validation."""

import copy
import world_grid


def graph(connections):
    return {'version': 3, 'start': {'map': 'a', 'spawn': 'start'},
            'nodes': [{'map': key, 'x': index * 100, 'y': 0} for index, key in enumerate('abcd')],
            'connections': connections}


def link(identity, a, side, b):
    return {'id': identity, 'a': {'map': a, 'side': side},
            'b': {'map': b, 'side': world_grid.OPPOSITE[side]}, 'requirement': None}


def rejects(world, text, release=True):
    try:
        world_grid.validate_world(world, set('abcd'), {key: {'start': {}} for key in 'abcd'}, release)
    except ValueError as error:
        assert text in str(error), error
    else:
        raise AssertionError(f'Expected validation error containing {text!r}')


links = [link('a_east_b', 'a', 'east', 'b'), link('b_south_c', 'b', 'south', 'c'),
         link('c_west_d', 'c', 'west', 'd'), link('d_north_a', 'd', 'north', 'a')]
derived = world_grid.validate_world(graph(links), set('abcd'), {key: {'start': {}} for key in 'abcd'})
assert derived['coordinates'] == {'a': (0, 0), 'b': (1, 0), 'c': (1, 1), 'd': (0, 1)}

bad = graph(copy.deepcopy(links))
bad['connections'][-1]['b']['side'] = 'east'
rejects(bad, 'complementary')
rejects(graph([link('one', 'a', 'east', 'b'), link('two', 'a', 'east', 'c')]), 'occupied')
rejects(graph([link('ab', 'a', 'east', 'b'), link('bc', 'b', 'south', 'c'),
               link('cd', 'c', 'west', 'd'), link('da', 'd', 'south', 'a')]), 'inconsistent')
overlap = {'version': 3, 'start': {'map': 'a', 'spawn': 'start'},
           'nodes': [{'map': key, 'x': index * 100, 'y': 0} for index, key in enumerate('abcde')],
           'connections': [link('ab', 'a', 'east', 'b'), link('ac', 'a', 'south', 'c'),
                           link('bd', 'b', 'south', 'd'), link('ce', 'c', 'east', 'e')]}
try:
    world_grid.validate_world(overlap, set('abcde'), {key: {'start': {}} for key in 'abcde'})
except ValueError as error:
    assert 'overlaps' in str(error), error
else:
    raise AssertionError('Expected coordinate overlap rejection')
disconnected = graph([link('ab', 'a', 'east', 'b'), link('cd', 'c', 'east', 'd')])
rejects(disconnected, 'connect every')
world_grid.validate_world(disconnected, set('abcd'), {key: {'start': {}} for key in 'abcd'}, release=False)
print('Cardinal world graph validation passed')
