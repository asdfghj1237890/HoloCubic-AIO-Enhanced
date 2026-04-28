# -*- mode: python ; coding: utf-8 -*-


a = Analysis(
    ['CubicAIO_Tool.py'],
    pathex=[],
    binaries=[],
    datas=[
        ('cubictool.json', '.'),
        ('image', 'image'),
        ('i18n', 'i18n'),
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
