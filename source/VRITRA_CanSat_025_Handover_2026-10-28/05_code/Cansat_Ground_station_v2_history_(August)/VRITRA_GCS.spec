# -*- mode: python ; coding: utf-8 -*-
# =====================================================================
#  VRITRA CAN-7USAT Ground Control Station — PyInstaller spec
#  Team 2026-INSPACe-CAN-7USAT-025, Lovely Professional University
#
#  Build with:   pyinstaller VRITRA_GCS.spec
#  Produces:     dist/VRITRA_GCS.exe   (single-file, no Python needed)
#
#  NOTE: pyqtgraph.opengl and PyOpenGL are deliberately EXCLUDED.
#        The 3-D CanSat view is rendered with pure QPainter, so no GPU
#        or OpenGL driver is required — this avoids the GLError 1282
#        "invalid operation / glClearColor" crash on machines without
#        a dedicated GPU.
# =====================================================================
from PyInstaller.utils.hooks import collect_data_files
import os

datas = []
datas += collect_data_files('pyqtgraph')

# Embed the recovery-map background so it ships INSIDE the exe and never
# needs to be manually copied into dist/ after a rebuild. At runtime it is
# extracted to sys._MEIPASS — GroundTrack._load_bg() checks there first.
if os.path.exists('2dmap.jpg'):
    datas += [('2dmap.jpg', '.')]

# map_tiles/ IS DELIBERATELY NOT BUNDLED. The offline XYZ cache that fetch_map.py
# builds runs to tens of MB and changes every time you fly somewhere new;
# embedding it would mean a full 4-minute PyInstaller rebuild to change launch
# site, and a 90 MB exe to email. GroundTrack._tile_root() looks next to the
# executable as well as in sys._MEIPASS, so the right workflow is:
#
#     python fetch_map.py "Phagwara, Punjab" --radius 10
#     copy the map_tiles folder next to VRITRA_GCS.exe
#
# and the map follows the vehicle to any launch site with no rebuild at all.

hiddenimports = [
    'serial.tools.list_ports',
    'numpy',
]

# pyqtgraph's Qt Designer templates are imported dynamically, so PyInstaller's
# static analysis cannot see them — they have to be named here. The catch is
# that pyqtgraph RENAMED them: up to 0.12 they were per-binding
# (`axisCtrlTemplate_pyqt5`), from 0.13 they are binding-agnostic
# (`axisCtrlTemplate_generic`). Naming the old spelling on pyqtgraph 0.14
# produced three `ERROR: Hidden import ... not found` lines in every build.
# Harmless — the real modules were picked up by the pyqtgraph hook — but an
# error in a build log trains you to ignore build logs.
#
# Probe for whichever spelling this install actually has instead of hard-coding
# either, so the spec keeps working across a pyqtgraph upgrade in both
# directions and the log stays clean.
import importlib.util

for _stem in ('pyqtgraph.graphicsItems.ViewBox.axisCtrlTemplate',
              'pyqtgraph.graphicsItems.PlotItem.plotConfigTemplate',
              'pyqtgraph.imageview.ImageViewTemplate'):
    for _suffix in ('_generic', '_pyqt5'):
        try:
            if importlib.util.find_spec(_stem + _suffix) is not None:
                hiddenimports.append(_stem + _suffix)
                break
        except (ImportError, ValueError):
            continue

excludes = [
    'tkinter', 'matplotlib', 'PyQt6',
    'OpenGL', 'pyqtgraph.opengl',          # no GPU dependency
]

a = Analysis(
    ['vritra_gcs.py'],
    pathex=[],
    binaries=[],
    datas=datas,
    hiddenimports=hiddenimports,
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=excludes,
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
    name='VRITRA_GCS',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,            # windowed app (no console window)
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon='vritra.ico' if __import__('os').path.exists('vritra.ico') else None,
)
