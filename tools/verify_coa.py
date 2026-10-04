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
PROFILE_ROOT = REPOSITORY / '.cache' / 'verify-coa' / 'profiles'
WINDOWS_COMMAND_LIMIT = 32767
COMMAND_MARGIN = 2048
STOCK_LEVEL_CAP = 80

MAX_PLAYER_LEVEL = re.compile(r'^\s*MaxPlayerLevel\s*=\s*(\d+)', re.MULTILINE)

PROFILES = {
    'wildcard': {
        'realm': 'Darkmoon - Season 10 Wildcard',
        'overrides': {
            'mod-coa-challenges.conf': {'CoAChallenges.GameModes.Realm': 'WildCard'},
        },
        'scenarios': [
            'hero-call-board-s10-quests',
            'hero-class-baseline',
            'wildcard-character-power-native-rage-control',
            'wildcard-first-login-state',
            'wildcard-hero-tames-a-beast',
            'wildcard-hero-victorious-needs-victory-rush',
            'wildcard-obliterating-aether-spends-runes',
            'wildcard-scroll-rewards',
            'wildcard-season-event',
        ],
    },
}


def profile_scenarios():
    return {name for profile in PROFILES.values() for name in profile['scenarios']}


def setting_line(key, value):
    return re.compile(r'^\s*' + re.escape(key) + r'\s*=.*$', re.MULTILINE), '{} = {}'.format(key, value)


def profile_settings(name, settings_path):
    profile = PROFILES[name]
    settings = json.loads(settings_path.read_text(encoding='utf-8'))

    source = Path(settings.get('modules_config_dir') or 'conf/verify-modules')
    if not source.is_absolute():
        source = REPOSITORY / source

    root = PROFILE_ROOT / name
    if root.exists():
        shutil.rmtree(root)
    modules = root / 'modules'
    shutil.copytree(source, modules)

    for filename, values in profile['overrides'].items():
        target = modules / filename
        if not target.is_file():
            raise SystemExit('ERROR: profile {} expects {} in {}'.format(name, filename, source))
        text = target.read_text(encoding='utf-8')
        for key, value in values.items():
            pattern, replacement = setting_line(key, value)
            text, replaced = pattern.subn(replacement, text, count=1)
            if not replaced:
                raise SystemExit('ERROR: {} has no {} to override'.format(target, key))
        target.write_text(text, encoding='utf-8')

    settings['modules_config_dir'] = str(modules)
    generated = root / 'verify-all.json'
    generated.write_text(json.dumps(settings, indent=2), encoding='utf-8')
    return generated


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
    parser.add_argument('--profile', choices=sorted(PROFILES),
                        help='run only the scenarios that need this realm, with its module settings applied')
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

    reserved = profile_scenarios()
    if arguments.profile:
        wanted = set(PROFILES[arguments.profile]['scenarios'])
        selected = [entry for entry in selected if Path(entry).stem in wanted]
    else:
        selected = [entry for entry in selected if Path(entry).stem not in reserved]

    if not selected:
        print('ERROR: no scenario fits a level cap of {}'.format(level_cap), file=sys.stderr)
        return 2

    if arguments.list:
        print('\n'.join(selected))
        return 0

    settings_path = arguments.settings
    if arguments.profile:
        settings_path = profile_settings(arguments.profile, settings_path)
        print('Profile {} ({}): {} scenarios'.format(
            arguments.profile, PROFILES[arguments.profile]['realm'], len(selected)))

    total = sum(1 for path in SCENARIOS.glob('*.json'))
    lowered = sum(1 for entry in selected if entry.endswith('.json'))
    print('Level cap {}: {} of {} scenarios, {} with players lowered to it'.format(
        level_cap, len(selected), total, lowered))

    passthrough = [argument for argument in arguments.rest if argument != '--']
    if settings_path != DEFAULT_SETTINGS:
        passthrough = ['--settings', str(settings_path)] + passthrough

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
