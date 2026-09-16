"""Build/run deterministic tests against the production presence methods."""
from pathlib import Path
import os
import subprocess
import shutil

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

fixture = (root / 'tests/chess-presence.cpp').read_text()
fixture = fixture.replace('// CONTACT_STRUCT', extract(header, 'struct ChessContact') + ';')
fixture = fixture.replace('// PRODUCTION_METHODS', '\n\n'.join([
    extract(source, 'void p3RetroChess::tickChessPresence()'),
    extract(source, 'bool p3RetroChess::handleChessPresence('),
    extract(source, 'bool p3RetroChess::saveList('),
    extract(source, 'bool p3RetroChess::loadList('),
    extract(source, 'bool p3RetroChess::addChessContact('),
    extract(source, 'void p3RetroChess::removeChessContact('),
    extract(source, 'bool p3RetroChess::acceptDataFromPeer('),
    extract(source, 'void p3RetroChess::receiveData('),
]))
temporary = root / 'temp'
temporary.mkdir(exist_ok=True)
test_source = temporary / 'chess-presence-test.cpp'
test_exe = temporary / ('chess-presence-test.exe' if os.name == 'nt' else 'chess-presence-test')
test_source.write_text(fixture)
compiler = Path(shutil.which('g++'))
qmake = compiler.with_name('qmake.exe' if os.name == 'nt' else 'qmake')
qt_headers = subprocess.check_output([str(qmake), '-query', 'QT_INSTALL_HEADERS'], text=True).strip()
qt_libs = subprocess.check_output([str(qmake), '-query', 'QT_INSTALL_LIBS'], text=True).strip()
environment = dict(os.environ, TMPDIR=str(temporary))
try:
    subprocess.run([str(compiler), '-std=c++11', '-fPIC', str(test_source), '-I' + qt_headers,
                    '-I' + qt_headers + '/QtCore', '-L' + qt_libs, '-lQt5Core',
                    '-o', str(test_exe)], check=True, env=environment)
    subprocess.run([str(test_exe)], check=True, env=environment)
finally:
    test_source.unlink(missing_ok=True)
    test_exe.unlink(missing_ok=True)
