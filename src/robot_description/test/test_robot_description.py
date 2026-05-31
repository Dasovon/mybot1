"""Smoke test: xacro expands to valid URDF with required frames."""
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET


def test_robot_xacro_expands_to_valid_urdf():
    package_dir = Path(__file__).resolve().parents[1]
    xacro_file = package_dir / 'urdf' / 'robot.urdf.xacro'

    assert xacro_file.exists(), f'Missing xacro file: {xacro_file}'

    result = subprocess.run(
        ['xacro', str(xacro_file)],
        capture_output=True,
        text=True,
        check=True,
    )

    root = ET.fromstring(result.stdout)

    assert root.tag == 'robot'
    assert root.attrib.get('name'), 'URDF robot element must have a name attribute'

    links = {link.attrib['name'] for link in root.findall('link')}
    required = {'base_footprint', 'base_link', 'laser', 'imu_link', 'camera_link'}
    missing = required - links
    assert not missing, f'URDF missing required links: {missing}'
