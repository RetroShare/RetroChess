"""Build/run deterministic tests against the production tunnel lifecycle methods.

Optional: set RETROCHESS_TEST_QT_INCLUDE to a directory with Qt-compatible
headers to build without qmake (the default uses qmake + Qt5Core like
chess-presence.py).
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
source = (root / 'services/p3RetroChess.cc').read_text()
header = (root / 'services/p3RetroChess.h').read_text()


def extract(text, signature):
    start = text.index(signature)
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


def constants(text):
    start = text.index('static const unsigned int kPresenceMaxFailures')
    return text[start:text.index('static time_t chessPresenceRetryDelay(')] + \
        extract(text, 'static time_t chessPresenceRetryDelay(')


fixture = (root / 'tests/tunnel-lifecycle.cpp').read_text()
fixture = fixture.replace('// CONTACT_STRUCT', extract(header, 'struct ChessContact') + ';')
fixture = fixture.replace('// PRODUCTION_METHODS', '\n\n'.join([
    constants(source),
    extract(source, 'bool p3RetroChess::closeGxsTunnel('),
    extract(source, 'bool p3RetroChess::closeGxsTunnelNow('),
    extract(source, 'void p3RetroChess::processDeferredCloses()'),
    extract(source, 'bool p3RetroChess::openGxsTunnel('),
    extract(source, 'bool p3RetroChess::sendGxsData('),
    extract(source, 'void p3RetroChess::dumpTunnelState()'),
    extract(source, 'void p3RetroChess::closeQueuedGxsTunnels()'),
    extract(source, 'void p3RetroChess::sweepOrphanGxsTunnels()'),
    extract(source, 'void p3RetroChess::tickChessPresence()'),
    extract(source, 'bool p3RetroChess::handleChessPresence('),
    extract(source, 'void p3RetroChess::notifyTunnelStatus('),
    extract(source, 'void p3RetroChess::handleGxsTick()'),
]))
temporary = root / 'temp'
temporary.mkdir(exist_ok=True)
test_source = temporary / 'tunnel-lifecycle-test.cpp'
test_exe = temporary / ('tunnel-lifecycle-test.exe' if os.name == 'nt' else 'tunnel-lifecycle-test')
test_source.write_text(fixture)
compiler = Path(shutil.which('g++'))
environment = dict(os.environ, TMPDIR=str(temporary))
flags = ['-fPIC', '-Wall', str(test_source), '-I' + str(root)]
shim = os.environ.get('RETROCHESS_TEST_QT_INCLUDE')
if shim:
    flags += ['-std=c++17', '-I' + shim]
else:
    flags += qt_core_flags(compiler)
try:
    subprocess.run([str(compiler)] + flags + ['-o', str(test_exe)], check=True, env=environment)
    subprocess.run([str(test_exe)], check=True, env=environment)
finally:
    test_source.unlink(missing_ok=True)
    test_exe.unlink(missing_ok=True)
