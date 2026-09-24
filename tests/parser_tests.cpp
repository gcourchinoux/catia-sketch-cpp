#include "catia_sketch.hpp"

#include <cassert>
#include <fstream>
#include <string>

int main() {
    const std::string path = "catia-sketch-test.CATPart";
    std::ofstream file(path, std::ios::binary);
    file.write("V5_CFV2\0", 8);
    file.write("\x7c\x09Sketch\x00", 12);
    file.write("\x7c\x09ConstraintDYS\x00", 18);
    file.write("\x7c\x09\x32\x2DPoint\x00", 16);
    file.close();

    catia::SketchParser parser(path);
    const auto result = parser.parse();
    assert(!result.sketches.empty());
    assert(result.report.sketches >= 1);
    std::remove(path.c_str());
}
