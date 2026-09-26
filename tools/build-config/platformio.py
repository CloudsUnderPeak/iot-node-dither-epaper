"""PlatformIO PRE hook: one feature configuration for direct and release builds."""
Import('env')
import sys
from pathlib import Path
sys.path.insert(0, str(Path(env.subst('$PROJECT_DIR'))/'tools/build-config'))
from features import resolve, materialize, config_hash, source_filter

path = env.GetProjectOption('custom_features', 'config/features.ini')
sleep = env.GetProjectOption('custom_sleep', '')
config = resolve(path, int(sleep) if sleep else None)
flags = env.GetProjectOption('build_flags', '')
if isinstance(flags, list): flags = ' '.join(flags)
if 'IOT_FEATURE_' in flags or 'SLEEP_IDLE_TIMEOUT_SECONDS' in flags or 'SLEEP_IGNORE_USB_HOST' in flags:
    raise ValueError('feature and sleep macros are controlled by the feature configuration')
out = materialize(config, Path(env.subst('$PROJECT_DIR'))/'.pio/feature-config'/config_hash(config))
env.Append(CCFLAGS=['-include', str(out/'ProjectFeatures.generated.h')])
env.Replace(SRC_FILTER=source_filter(config))
env.BoardConfig().update('build.partitions', str(out/'partitions.csv'))
