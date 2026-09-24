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
        std::cout << "sketches=" << result.report.sketches << '\n';
        std::cout << "entities=" << result.report.entities << '\n';
        std::cout << "native_constraints=" << result.report.native_constraints << '\n';

        for (const auto& sketch : result.sketches) {
            std::cout << "Sketch: " << sketch.id << "\n";
            for (const auto& entity : sketch.entities) {
                std::cout << "  entity: " << entity.native_id << " kind=" << entity.native_kind << '\n';
                std::visit([](const auto& value) {
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<T, catia::Point2D>) {
                        std::cout << "    Point2D " << value.position.x << ", " << value.position.y << '\n';
                    } else if constexpr (std::is_same_v<T, catia::UnknownGeometry>) {
                        std::cout << "    Unknown geometry: " << value.native_kind << '\n';
                    }
                }, entity.geometry);
            }
            for (const auto& entry : sketch.constraints) {
                std::visit([](const auto& constraint) {
                    using T = std::decay_t<decltype(constraint)>;
                    if constexpr (std::is_same_v<T, catia::NativeConstraint>) {
                        std::cout << "  native_constraint: " << constraint.native_class << " @ " << constraint.byte_offset << '\n';
                    }
                }, entry);
            }
            for (const auto& warning : result.report.warnings) {
                std::cout << "warning: " << warning << '\n';
            }
        }
    } catch (const catia::ParseError& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
