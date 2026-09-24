#include "catia_sketch.hpp"
#include <iostream>
#include <type_traits>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: catia-sketch-read FILE.CATPart\n";
        return 2;
    }
    try {
        catia::SketchParser parser{argv[1]};
        const auto result = parser.parse();
        std::cout << "sketches: " << result.sketches.size() << "\n";
        for (const auto& sketch : result.sketches) {
            std::cout << "id: " << sketch.id << "\n"
                      << "entities: " << sketch.entities.size() << "\n"
                      << "constraints: " << sketch.constraints.size() << "\n";
        }
        for (const auto& warning : result.report.warnings) std::cerr << "warning: " << warning << '\n';
    } catch (const catia::ParseError& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
