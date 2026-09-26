"""Build/run the leaderboard sync test against the production methods.

Set RETROCHESS_TEST_QT_INCLUDE to a directory with Qt-compatible headers to
build without qmake (default: qmake + Qt5Core, like chess-presence.py).
"""
from pathlib import Path
import os
import subprocess
import shutil


def qt_core_flags(compiler):
    """Include/link flags for QtCore (Qt 5 or 6). qmake comes from
    RETROCHESS_QT_QMAKE, else next to g++, else qmake6/qmake on PATH."""
    qmake = os.environ.get('RETROCHESS_QT_QMAKE')
    if not qmake:
        qmake = str(compiler.with_name('qmake.exe' if os.name == 'nt' else 'qmake'))
        if not Path(qmake).exists():
            qmake = shutil.which('qmake6') or shutil.which('qmake') or qmake
    query = lambda key: subprocess.check_output([qmake, '-query', key], text=True).strip()
    headers, libs = query('QT_INSTALL_HEADERS'), query('QT_INSTALL_LIBS')
    major = query('QT_VERSION').split('.')[0]
    return ['-std=c++17' if major == '6' else '-std=c++14', '-I' + headers, '-I' + headers + '/QtCore',
            '-L' + libs, '-Wl,-rpath,' + libs, '-lQt%sCore' % major]

root = Path(__file__).resolve().parents[1]
source = (root / 'gui/RetroChessLeaderboard.cpp').read_text()


def extract(text, signature):
    start = text.index(signature)
    start = text.rfind('\n', 0, start) + 1  # include the return type
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        if text[end] == '{':
            depth += 1
        elif text[end] == '}':
            depth -= 1
        end += 1
    return text[start:end]


namespace = extract(source, 'namespace {\n')
constants = '\n'.join(line for line in namespace.splitlines() if line.startswith('constexpr'))
fixture = (root / 'tests/leaderboard-sync.cpp').read_text()
fixture = fixture.replace('// CONSTANTS', constants)
fixture = fixture.replace('// PRODUCTION_METHODS', '\n\n'.join(
    extract(source, 'RetroChessLeaderboard::' + name) for name in [
        'validResult(', 'canonicalKey(', 'consumeReceipt(', 'storeReceipt(',
        'broadcastReceipt(', 'sendSyncToPeer(', 'sendSyncRequest(',
        'handleTunnelData(', 'synchronizeTunnels()']))
temporary = root / 'temp'
temporary.mkdir(exist_ok=True)
test_source = temporary / 'leaderboard-sync-test.cpp'
test_exe = temporary / ('leaderboard-sync-test.exe' if os.name == 'nt' else 'leaderboard-sync-test')
test_source.write_text(fixture)
compiler = Path(shutil.which('g++'))
flags = ['-fPIC', str(test_source), '-I' + str(root)]
shim = os.environ.get('RETROCHESS_TEST_QT_INCLUDE')
if shim:
    flags += ['-std=c++17', '-I' + shim]
else:
    flags += qt_core_flags(compiler)
try:
    subprocess.run([str(compiler)] + flags + ['-o', str(test_exe)], check=True)
    subprocess.run([str(test_exe)], check=True)
finally:
    test_source.unlink(missing_ok=True)
    test_exe.unlink(missing_ok=True)
