#include "veex/Application.h"

int main(int argc, char** argv) {
    veex::Application app("Game/gameinfo.txt", "");
    return app.Run();
}