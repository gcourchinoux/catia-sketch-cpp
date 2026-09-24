#include "catia_sketch.hpp"
#include <cassert>
#include <fstream>
#include <string>

int main() {
    const std::string path = "catia-sketch-test.CATPart";
    std::ofstream file(path, std::ios::binary);
    file.write("V5_CFV2\0", 8);
    file.close();

    catia::SketchParser parser(path);
    const auto result = parser.parse();
    assert(result.sketches.size() == 1);
    assert(result.sketches.front().native_payload.size() == 8);
    std::remove(path.c_str());
}
