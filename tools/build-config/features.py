"""Resolve the project's compile-time features; shared by release and PlatformIO."""
import configparser
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NAMES = ('sleep', 'epaper', 'storage', 'auth', 'user_files', 'mdns', 'battery', 'console', 'status_led')
DEFAULTS = {name: True for name in NAMES}


def resolve(path=None, sleep=None):
    values = dict(DEFAULTS)
    settings = {'idle_timeout_seconds': 600, 'ignore_usb_host': 0}
    if path:
        values = {name: False for name in NAMES}
        parser = configparser.ConfigParser(interpolation=None)
        if not parser.read(path, encoding='utf-8'):
            raise ValueError(f'feature configuration is missing: {path}')
        if parser.defaults() or set(parser.sections()) - {'features', 'sleep'}:
            raise ValueError('unknown feature configuration section')
        for key, raw in parser.items('features') if parser.has_section('features') else ():
            if key not in NAMES or raw not in ('0', '1'):
                raise ValueError(f'invalid feature: {key}={raw}')
            values[key] = raw == '1'
        for key, raw in parser.items('sleep') if parser.has_section('sleep') else ():
            if key not in settings or not raw.isdecimal():
                raise ValueError(f'invalid sleep setting: {key}={raw}')
            settings[key] = int(raw)
    if sleep is not None:
        if sleep not in (0, 1):
            raise ValueError('sleep must be 0 or 1')
        values['sleep'] = bool(sleep)
    for dependent in ('epaper', 'user_files'):
        if values[dependent] and not values['storage']:
            raise ValueError(f'{dependent.upper()} requires STORAGE=1; enable it explicitly')
    if not 1 <= settings['idle_timeout_seconds'] <= 86400 or settings['ignore_usb_host'] not in (0, 1):
        raise ValueError('invalid sleep build settings')
    return {'features': values, 'sleep': settings}


def config_hash(config):
    return hashlib.sha256(json.dumps(config, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def materialize(config, output):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    header = '#pragma once\n' + ''.join(f'#define IOT_FEATURE_{key.upper()} {int(value)}\n' for key, value in config['features'].items())
    header += f"#define SLEEP_IDLE_TIMEOUT_SECONDS {config['sleep']['idle_timeout_seconds']}\n"
    header += f"#define SLEEP_IGNORE_USB_HOST {config['sleep']['ignore_usb_host']}\n"
    snapshot = '[features]\n'+''.join(f'{key} = {int(value)}\n' for key,value in config['features'].items())
    snapshot += '\n[sleep]\n'+''.join(f'{key} = {value}\n' for key,value in config['sleep'].items())
    files = {'effective-features.ini': snapshot, 'ProjectFeatures.generated.h': header,
             'effective-features.json': json.dumps(config, indent=2, sort_keys=True)+'\n'}
    csv = (ROOT/'partitions.csv').read_text()
    if not config['features']['storage']:
        csv = '\n'.join('app0, app, ota_0, 0x10000, 0x3D8000,' if line.startswith('app0,') else line
                        for line in csv.splitlines() if not line.startswith('userdata,'))+'\n'
    files['partitions.csv'] = csv
    for name, content in files.items():
        target = output/name
        if not target.exists() or target.read_text() != content:
            target.write_text(content, encoding='utf-8')
    return output


def source_filter(config):
    f = config['features']
    excluded = []
    if not f['sleep']: excluded += ['modules/sleep/*.cpp', 'api/sleep/']
    if not f['epaper']: excluded += ['modules/epaper/', 'modules/hardware/SpiBus.cpp', 'api/epaper/']
    if not f['storage']: excluded += ['modules/storage/UserDataStorage.cpp', 'modules/storage/UserDataPath.cpp']
    if not f['storage']: excluded += ['api/storage/UserFileEndpoints.cpp']
    if not f['auth']: excluded += ['modules/auth/', 'api/auth/']
    if not f['mdns']: excluded += ['modules/mdns/']
    if not f['battery']: excluded += ['modules/power/']
    if not f['console']: excluded += ['modules/console/']
    if not f['status_led']: excluded += ['modules/status_led/']
    return '+<*> ' + ' '.join(f'-<{path}>' for path in excluded)
