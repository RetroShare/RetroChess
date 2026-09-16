"""Exercise the production configuration hook through the plugin base interface."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
header = (root / 'RetroChessPlugin.h').read_text()
source = (root / 'RetroChessPlugin.cpp').read_text()
declaration = re.search(r'p3Config\s*\*p3_config\(\) const override;', header)
if not declaration:
    raise RuntimeError('Plugin does not override the configuration discovery hook')
start = source.index('p3Config *RetroChessPlugin::p3_config() const')
end = source.index('\n}', start) + 2
method = source[start:end]
fixture = r"""
#include <cassert>
#include <iostream>
struct p3Service { virtual ~p3Service() = default; };
struct p3Config { virtual ~p3Config() = default; };
struct ChessService : p3Service, p3Config {};
struct RsPlugin {
    virtual ~RsPlugin() = default;
    virtual p3Config *p3_config() const { return nullptr; }
};
struct RetroChessPlugin : RsPlugin {
    mutable ChessService *mRetroChess = nullptr;
    mutable unsigned int creations = 0;
    ~RetroChessPlugin() { delete mRetroChess; }
    p3Service *p3_service() const {
        if (!mRetroChess) { mRetroChess = new ChessService; ++creations; }
        return mRetroChess;
    }
    // DECLARATION
};
// METHOD
int main() {
    RetroChessPlugin plugin;
    RsPlugin *base = &plugin;
    p3Config *config = base->p3_config(); // Config discovery before service discovery.
    assert(config != nullptr);
    assert(config == static_cast<p3Config *>(plugin.mRetroChess));
    assert(plugin.p3_service() == static_cast<p3Service *>(plugin.mRetroChess));
    assert(base->p3_config() == config && plugin.creations == 1);
    RetroChessPlugin serviceFirst;
    serviceFirst.p3_service();
    assert(serviceFirst.p3_config() == static_cast<p3Config *>(serviceFirst.mRetroChess));
    assert(serviceFirst.creations == 1);
    std::cout << "Plugin configuration registration checks passed\n";
}
""".replace('// DECLARATION', declaration.group()).replace('// METHOD', method)
with tempfile.TemporaryDirectory(prefix='config-test-', dir=root / 'temp') as directory:
    directory = Path(directory)
    cpp = directory / 'test.cpp'
    exe = directory / ('test.exe' if os.name == 'nt' else 'test')
    cpp.write_text(fixture)
    environment = dict(os.environ, TMPDIR=str(directory))
    subprocess.run([shutil.which('g++'), '-std=c++11', str(cpp), '-o', str(exe)], check=True, env=environment)
    subprocess.run([str(exe)], check=True, env=environment)
