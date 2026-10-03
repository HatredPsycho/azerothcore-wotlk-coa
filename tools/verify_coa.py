#!/usr/bin/env python3

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parent.parent
SCENARIOS = REPOSITORY / 'apps' / 'coa-gameplay-test' / 'scenarios'
DEFAULT_SETTINGS = REPOSITORY / 'conf' / 'verify-all.json'
STOCK_LEVEL_CAP = 80

MAX_PLAYER_LEVEL = re.compile(r'^\s*MaxPlayerLevel\s*=\s*(\d+)', re.MULTILINE)


def configured_level_cap(settings_path):
    if not settings_path.is_file():
        return None

    settings = json.loads(settings_path.read_text(encoding='utf-8'))
    configured = settings.get('worldserver_config')
    if not configured:
        return None

    config = Path(configured)
    if not config.is_absolute():
        config = REPOSITORY / config
    if not config.is_file():
        return None

    match = MAX_PLAYER_LEVEL.search(config.read_text(encoding='utf-8', errors='replace'))
    return int(match.group(1)) if match else None


def highest_player_level(scenario):
    players = scenario.get('players') or []
    return max((int(player.get('level') or 0) for player in players), default=0)


def scenarios_within(level_cap):
    selected = []
    for path in sorted(SCENARIOS.glob('*.json')):
        try:
            scenario = json.loads(path.read_text(encoding='utf-8'))
        except (OSError, ValueError):
            continue

        if not scenario.get('players'):
            continue

        if highest_player_level(scenario) <= level_cap:
            selected.append(path.stem)

    return selected


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--settings', type=Path, default=DEFAULT_SETTINGS)
    parser.add_argument('--level-cap', type=int)
    parser.add_argument('--list', action='store_true',
                        help='print the selected scenario ids and exit')
    parser.add_argument('rest', nargs=argparse.REMAINDER,
                        help='further arguments for verify_all.py, after --')
    arguments = parser.parse_args()

    level_cap = arguments.level_cap or configured_level_cap(arguments.settings)
    if not level_cap:
        print('ERROR: no MaxPlayerLevel found; pass --level-cap', file=sys.stderr)
        return 2

    selected = scenarios_within(level_cap)
    if not selected:
        print('ERROR: no scenario fits a level cap of {}'.format(level_cap), file=sys.stderr)
        return 2

    if arguments.list:
        print('\n'.join(selected))
        return 0

    total = sum(1 for path in SCENARIOS.glob('*.json'))
    print('Level cap {}: {} of {} scenarios selected{}'.format(
        level_cap, len(selected), total,
        '' if level_cap >= STOCK_LEVEL_CAP else '; the rest ask for characters this realm cannot make'))

    passthrough = [argument for argument in arguments.rest if argument != '--']
    if arguments.settings != DEFAULT_SETTINGS:
        passthrough = ['--settings', str(arguments.settings)] + passthrough

    command = [sys.executable, '-B', str(REPOSITORY / 'tools' / 'verify_all.py'),
               '--stages', 'gameplay'] + passthrough + ['--scenario'] + selected

    return subprocess.run(command, cwd=REPOSITORY).returncode


if __name__ == '__main__':
    sys.exit(main())
