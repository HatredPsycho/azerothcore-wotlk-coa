#!/usr/bin/env python3

import argparse
import collections
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parent.parent
SCENARIOS = REPOSITORY / 'apps' / 'coa-gameplay-test' / 'scenarios'
DEFAULT_SETTINGS = REPOSITORY / 'conf' / 'verify-all.json'
CLAMPED = REPOSITORY / '.cache' / 'verify-coa' / 'clamped'
WINDOWS_COMMAND_LIMIT = 32767
COMMAND_MARGIN = 2048
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


def clamped_copies(level_cap):
    if CLAMPED.exists():
        shutil.rmtree(CLAMPED)
    CLAMPED.mkdir(parents=True)

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
            continue

        for player in scenario['players']:
            if int(player.get('level') or 0) > level_cap:
                player['level'] = level_cap

        copy = CLAMPED / path.name
        copy.write_text(json.dumps(scenario), encoding='utf-8')
        selected.append(str(copy))

    return selected


def batches(command_prefix, selected):
    budget = WINDOWS_COMMAND_LIMIT - COMMAND_MARGIN - len(subprocess.list2cmdline(command_prefix))

    batch = []
    length = 0
    for entry in selected:
        size = len(entry) + 3
        if batch and length + size > budget:
            yield batch
            batch, length = [], 0
        batch.append(entry)
        length += size

    if batch:
        yield batch


def tally(stdout):
    for line in reversed(stdout.splitlines()):
        if line.startswith('VERIFY ALL:'):
            report = Path(line.split(None, 3)[-1])
            if report.is_file():
                stage = json.loads(report.read_text(encoding='utf-8'))['stages']['gameplay']
                return report, stage.get('summary', {}).get('counts', {})
    return None, {}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--settings', type=Path, default=DEFAULT_SETTINGS)
    parser.add_argument('--level-cap', type=int)
    parser.add_argument('--clamp', action='store_true',
                        help='lower every player above the cap to it and run the whole catalogue')
    parser.add_argument('--list', action='store_true',
                        help='print the selected scenario ids and exit')
    parser.add_argument('rest', nargs=argparse.REMAINDER,
                        help='further arguments for verify_all.py, after --')
    arguments = parser.parse_args()

    level_cap = arguments.level_cap or configured_level_cap(arguments.settings)
    if not level_cap:
        print('ERROR: no MaxPlayerLevel found; pass --level-cap', file=sys.stderr)
        return 2

    selected = clamped_copies(level_cap) if arguments.clamp else scenarios_within(level_cap)
    if not selected:
        print('ERROR: no scenario fits a level cap of {}'.format(level_cap), file=sys.stderr)
        return 2

    if arguments.list:
        print('\n'.join(selected))
        return 0

    total = sum(1 for path in SCENARIOS.glob('*.json'))
    lowered = sum(1 for entry in selected if entry.endswith('.json'))
    print('Level cap {}: {} of {} scenarios, {} with players lowered to it'.format(
        level_cap, len(selected), total, lowered))

    passthrough = [argument for argument in arguments.rest if argument != '--']
    if arguments.settings != DEFAULT_SETTINGS:
        passthrough = ['--settings', str(arguments.settings)] + passthrough

    prefix = [sys.executable, '-B', str(REPOSITORY / 'tools' / 'verify_all.py'),
              '--stages', 'gameplay'] + passthrough + ['--scenario']

    groups = list(batches(prefix, selected))
    counts = collections.Counter()
    reports = []
    failed = 0

    for number, group in enumerate(groups, 1):
        print('\n--- batch {} of {}: {} scenarios'.format(number, len(groups), len(group)), flush=True)
        run = subprocess.run(prefix + group, cwd=REPOSITORY, text=True,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        print(run.stdout, end='', flush=True)

        report, batch_counts = tally(run.stdout)
        if report:
            reports.append(report)
        counts.update(batch_counts)
        failed = failed or run.returncode

    print('\n=== level cap {} over {} batches ==='.format(level_cap, len(groups)))
    for name in ('passed', 'failed', 'not_run', 'passed_on_isolated_rerun'):
        if name in counts:
            print('  {:<26} {}'.format(name, counts[name]))
    for report in reports:
        print('  {}'.format(report))

    return failed


if __name__ == '__main__':
    sys.exit(main())
