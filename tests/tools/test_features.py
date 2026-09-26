import importlib.util
import itertools
import configparser
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('features_under_test', ROOT/'tools/build-config/features.py')
features = importlib.util.module_from_spec(spec)
spec.loader.exec_module(features)

class FeatureConfigurationTest(unittest.TestCase):
    def parse(self, text, sleep=None):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'features.ini'; path.write_text(text)
            return features.resolve(path, sleep)

    def test_all_combinations_and_dependencies(self):
        valid = 0
        for bits in itertools.product((0, 1), repeat=8):
            values = dict(zip(features.NAMES,bits))
            text = '[features]\n'+''.join(f'{k}={v}\n' for k,v in values.items())
            if (values['epaper'] or values['user_files']) and not values['storage']:
                with self.assertRaisesRegex(ValueError, 'requires STORAGE'): self.parse(text)
            else:
                self.assertEqual(self.parse(text)['features'], {k:bool(v) for k,v in values.items()})
                valid += 1
        self.assertEqual(valid,160)

    def test_omitted_features_are_off_and_storage_must_be_selected(self):
        self.assertFalse(self.parse('[features]\nauth=1')['features']['sleep'])
        for dependent in ('epaper', 'user_files'):
            with self.subTest(dependent=dependent), self.assertRaisesRegex(ValueError, 'requires STORAGE'):
                self.parse(f'[features]\n{dependent}=1')
            selected = self.parse(f'[features]\n{dependent}=1\nstorage=1')['features']
            self.assertTrue(selected[dependent] and selected['storage'])

    def test_invalid_and_duplicate_settings_fail(self):
        for text in ('[features]\nunknown=1','[features]\nauth=true','[features]\nauth=2',
                     '[features]\nauth=0\nauth=1','[bad]\nauth=0','[DEFAULT]\nauth=0',
                     '[sleep]\nidle_timeout_seconds=0','[sleep]\nignore_usb_host=2'):
            with self.subTest(text=text), self.assertRaises((ValueError, configparser.Error)):
                self.parse(text)

    def test_explicit_sleep_override_and_stable_identity(self):
        profile = '[features]\nsleep=0\n'+''.join(f'{name}=1\n' for name in features.NAMES if name != 'sleep')
        off = self.parse(profile)
        on = self.parse(profile,1)
        self.assertFalse(off['features']['sleep'])
        self.assertTrue(on['features']['sleep'])
        self.assertNotEqual(features.config_hash(off),features.config_hash(on))
        self.assertEqual(features.config_hash(on),features.config_hash(features.resolve()))

    def test_storage_off_reclaims_only_userdata(self):
        config = self.parse('[features]\nepaper=0\nuser_files=0\nstorage=0')
        with tempfile.TemporaryDirectory() as directory:
            output = features.materialize(config, directory)
            csv = (output/'partitions.csv').read_text()
            self.assertNotIn('userdata,',csv)
            self.assertIn('0x3D8000',csv)
            self.assertIn('0x3E8000',csv)
            self.assertIn('0x3F0000',csv)
            header = (output/'ProjectFeatures.generated.h').read_text()
            self.assertIn('#define IOT_FEATURE_STORAGE 0',header)
            filters = features.source_filter(config)
            self.assertIn('-<modules/storage/UserDataStorage.cpp>',filters)
            self.assertNotIn('-<modules/storage/>',filters)
