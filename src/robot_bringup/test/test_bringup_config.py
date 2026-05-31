"""Smoke tests: bringup config YAML files exist and have required parameters."""
from pathlib import Path

import yaml


PACKAGE_DIR = Path(__file__).resolve().parents[1]
CONFIG_DIR = PACKAGE_DIR / 'config'


def _load_yaml(name: str) -> dict:
    path = CONFIG_DIR / name
    assert path.exists(), f'Missing config file: {path}'
    with path.open('r', encoding='utf-8') as f:
        return yaml.safe_load(f)


def test_lidar_config_has_required_parameters():
    data = _load_yaml('lidar.yaml')
    params = data['rplidar_composition']['ros__parameters']

    assert params['serial_port'] == '/dev/rplidar'
    assert params['serial_baudrate'] == 115200
    assert params['frame_id'] == 'laser'
    assert params['angle_compensate'] is True
    assert params['scan_mode'] == 'Standard'


def test_ekf_config_preserves_fusion_contract():
    data = _load_yaml('ekf.yaml')
    params = data['ekf_filter_node']['ros__parameters']

    assert params['frequency'] == 20.0
    assert params['two_d_mode'] is True
    assert params['odom0'] == '/diff_cont/odom'
    assert params['imu0'] == '/imu/imu'
    assert params['odom_frame'] == 'odom'
    assert params['base_link_frame'] == 'base_footprint'


def test_realsense_config_exists_and_has_parameters():
    data = _load_yaml('realsense.yaml')
    assert data, 'realsense.yaml must not be empty'
