// test_embedded <unused> <tests/data>: the game must start and render from the compiled-in files alone (no disk image).
// If TANGRAM_ADF is set, the embedded files must also be byte-identical to the originals.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include "../src/embedded_assets.hpp"
#include "../src/game.hpp"
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)
using namespace tg;
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    Adf emb; loadEmbeddedAssets(emb);
    const char* names[] = {"TANGRAM.DAT", "TANGRAM.MLU", "TITLEPIC.LO5", "THALION.LO5", "MUSIC1.IMG", "MUSIC2.IMG", "HIGHSCOREPIC.LO5", "SCORE.DTA"};
    for (const char* n : names) CHECK(emb.file(n) && !emb.file(n)->empty());
    if (const char* p = std::getenv("TANGRAM_ADF")) if (*p) {
        Adf adf; std::string err; CHECK(adf.loadFile(p, err));
        for (const char* n : names) CHECK(emb.file(n) && adf.file(n) && *emb.file(n) == *adf.file(n));
    }
    Game g; std::string err;
    CHECK(g.init(emb, err, false));
    CHECK(g.mode() == Game::Mode::Menu);
    Event k; k.type = Event::Key; k.key = kKeyEnter; g.event(k);
    for (int i = 0; i < 30; i++) g.step();   // the menu fades out first
    CHECK(g.mode() == Game::Mode::Play && g.level() == 1);
    for (int i = 0; i < 10; i++) g.step();
    int16_t buf[2 * 1000]; g.audio(buf, 1000); CHECK(g.song() == 1);
    std::printf(failures ? "FAILED\n" : "OK\n");
    return failures ? 1 : 0;
}
