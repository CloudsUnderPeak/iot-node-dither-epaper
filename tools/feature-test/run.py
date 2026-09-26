#!/usr/bin/env python3
"""Validate all feature configurations and build representative isolated profiles."""
import argparse
import configparser
import importlib.util
import itertools
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools/build-config'))
from features import NAMES, resolve


def profiles(all_combinations=False):
    if all_combinations:
        for bits in itertools.product((False, True), repeat=len(NAMES)):
            f = dict(zip(NAMES, bits))
            if (f['epaper'] or f['user_files']) and not f['storage']: continue
            yield ''.join(str(int(b)) for b in bits), f
        return
    full = {name: True for name in NAMES}
    yield 'full', full
    for name in NAMES:
        f = dict(full); f[name] = False
        if name == 'storage': f.update(epaper=False, user_files=False)
        yield name+'-off', f
    yield 'minimal', {name: False for name in NAMES}
    f = {name: False for name in NAMES}; f['sleep'] = True
    yield 'timer-only', f
    f = {name: False for name in NAMES}; f.update(storage=True, user_files=True, sleep=True, console=True)
    yield 'file-server', f
    f = dict(full); f.update(auth=False, user_files=False, mdns=False, battery=False, console=False)
    yield 'panel-only', f


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', action='store_true')
    parser.add_argument('--all', action='store_true', help='Compile every valid combination (160).')
    parser.add_argument('--profile', action='append')
    parser.add_argument('--output', type=Path, default=ROOT/'tmp/verification/features-matrix')
    args = parser.parse_args()
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)
    # Validate all 256 inputs including hard dependency errors before any build.
    accepted = 0
    with tempfile.TemporaryDirectory(prefix='iot-features-') as temporary:
        work = Path(temporary)
        for bits in itertools.product((0, 1), repeat=len(NAMES)):
            values = dict(zip(NAMES, bits))
            config = work/'check.ini'
            config.write_text('[features]\n'+''.join(f'{k}={v}\n' for k,v in values.items()))
            valid = not ((values['epaper'] or values['user_files']) and not values['storage'])
            try: resolve(config)
            except ValueError:
                if valid: raise
            else:
                if not valid: raise RuntimeError('dependency validation accepted an invalid profile')
                accepted += 1
        print(f'Resolver: {accepted} valid / 256 combinations', flush=True)
        if not args.build: return
        for directory in ('src', 'config'):
            shutil.copytree(ROOT/directory, work/directory)
        (work/'tools/build-config').mkdir(parents=True)
        for file in ('features.py', 'platformio.py'):
            shutil.copy2(ROOT/'tools/build-config'/file, work/'tools/build-config'/file)
        for file in ('platformio.ini', 'partitions.csv'):
            shutil.copy2(ROOT/file, work/file)
        shutil.copytree(ROOT/'.pio/libdeps', work/'.pio/libdeps')
        # Use the production header generator with a verified empty frontend.
        spec = importlib.util.spec_from_file_location('release', ROOT/'tools/release-build/build_release.py')
        release = importlib.util.module_from_spec(spec); spec.loader.exec_module(release)
        release.WEB_OUTPUT = work/'web'; release.WEB_OUTPUT.mkdir()
        release.EMBEDDED_WEB_HEADER = work/'build/.work/esp-generated/EmbeddedWebAssets.generated.h'
        release.GENERATED_OUTPUT = release.EMBEDDED_WEB_HEADER.parent
        release.generate_embedded_web_header({'web':'none', 'web_sha256': release.tree_sha256(release.WEB_OUTPUT)})
        selected = list(profiles(args.all))
        if args.profile:
            selected = [(n,f) for n,f in selected if n in args.profile]
            if set(args.profile) != {n for n,f in selected}: raise ValueError('unknown profile')
        records = []
        symbols = {'sleep':'SleepCoordinator::', 'epaper':'EpaperService::',
                   'storage':'UserDataStorage::', 'auth':'AuthService::',
                   'user_files':'UserFileEndpoints::list(', 'mdns':'MdnsService::',
                   'battery':'BatteryMonitor::', 'console':'ConsoleShell::'}
        for name, f in selected:
            config = work/'config/matrix.ini'
            config.write_text('[features]\n'+''.join(f'{k}={int(v)}\n' for k,v in f.items()))
            ini = configparser.ConfigParser(interpolation=None); ini.read(work/'platformio.ini')
            ini.set('env:firebeetle2_esp32c6', 'custom_features', 'config/matrix.ini')
            project = work/'matrix.ini'
            with project.open('w') as stream: ini.write(stream)
            with (output/(name+'.log')).open('w') as log:
                result = subprocess.run(['pio','run','-d',str(work),'-c',str(project),'-e','firebeetle2_esp32c6'],stdout=log,stderr=subprocess.STDOUT)
            build = work/'.pio/build/firebeetle2_esp32c6'
            record = {'profile':name,'features':f,'success':result.returncode == 0}
            if result.returncode == 0:
                nm = Path.home()/'.platformio/packages/toolchain-riscv32-esp/bin/riscv32-esp-elf-nm'
                text = subprocess.check_output([str(nm), '-C', str(build/'firmware.elf')],text=True)
                for key, symbol in symbols.items():
                    if not f[key] and symbol in text: raise RuntimeError(f'{name}: disabled {key} remains in ELF')
                record['firmware_bytes'] = (build/'firmware.bin').stat().st_size
                record['symbol_exclusion'] = True
                shutil.copy2(build/'partitions.bin',output/(name+'-partitions.bin'))
            records.append(record)
            (output/'matrix.json').write_text(json.dumps(records,indent=2)+'\n')
            print(json.dumps(record),flush=True)
            if result.returncode: raise SystemExit(f'{name} failed; see {output/(name+".log")}')

if __name__ == '__main__': main()
