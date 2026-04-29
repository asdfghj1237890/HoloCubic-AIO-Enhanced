# -*- mode: python ; coding: utf-8 -*-

# esptool ships JSON stub-flasher files under
# esptool/targets/stub_flasher/<v>/esp*.json that it loads at runtime
# via package data. PyInstaller's static analysis doesn't pick these
# up automatically, so the windowed build crashed at flash time with
# "Stub flasher JSON file for ESP32 not found". collect_data_files
# pulls in everything the package ships under its install root.
from PyInstaller.utils.hooks import collect_data_files

esptool_datas = collect_data_files('esptool')


a = Analysis(
    ['CubicAIO_Tool.py'],
    pathex=[],
    binaries=[],
    datas=[
        ('cubictool.json', '.'),
        ('image', 'image'),
        ('i18n', 'i18n'),
        *esptool_datas,
    ],
    hiddenimports=[
        'esptool',
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name='CubicAIO_Tool',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=['image\\holo_256.ico'],
)
