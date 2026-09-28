#include <catch2/catch_session.hpp>
#include <cstdlib>

int main(int argc, char* argv[]) {
    Catch::Session session;

    bool update_fixtures = false;
    auto cli = session.cli() | Catch::Clara::Opt(update_fixtures)["--update-fixtures"]("Regenerate expected golden snapshot files with current output");
    session.cli(cli);

    int return_code = session.applyCommandLine(argc, argv);
    if (return_code != 0) {
        return return_code;
    }

    if (update_fixtures) {
#if defined(_WIN32)
        _putenv("SNIP_UPDATE_FIXTURES=1");
#else
        setenv("SNIP_UPDATE_FIXTURES", "1", 1);
#endif
    }

    return session.run();
}