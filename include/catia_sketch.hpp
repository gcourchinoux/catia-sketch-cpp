#pragma once

#include <cstdint>
#include <filesystem>
#include <list>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace catia {

using Bytes = std::vector<std::uint8_t>;

struct Vec2 {
    double x{};
    double y{};
};

struct Vec3 {
    double x{};
    double y{};
    double z{};
};

struct Point2D {
    Vec2 position{};
};

struct Line2D {
    Vec2 start{};
    Vec2 end{};
};

struct Circle2D {
    Vec2 center{};
    double radius{};
};

struct Arc2D {
    Vec2 center{};
    double radius{};
    double start_angle{};
    double end_angle{};
    bool clockwise{};
};

struct UnknownGeometry {
    std::string native_kind;
    Bytes payload;
};

using SketchGeometry = std::variant<Point2D, Line2D, Circle2D, Arc2D, UnknownGeometry>;

enum class ConstraintKind {
    Coincident,
    Horizontal,
    Vertical,
    Parallel,
    Perpendicular,
    Tangent,
    Equal,
    Concentric,
    Symmetric,
    Distance,
    DistanceX,
    DistanceY,
    Radius,
    Diameter,
    Angle,
    Fix,
    Unknown
};

struct ConstraintReference {
    std::uint32_t entity_index{};
    std::optional<std::uint32_t> point_index;
    std::string role;
};

struct TypedConstraint {
    ConstraintKind kind{ConstraintKind::Unknown};
    std::list<ConstraintReference> references;
    std::optional<double> parameter;
    bool driving{};
    bool active{true};
};

struct NativeConstraint {
    std::string native_class;
    std::string native_entry;
    std::string native_id;
    Bytes payload;
    std::list<std::string> referenced_native_objects;
    std::uint64_t byte_offset{};
};

using SketchConstraint = std::variant<TypedConstraint, NativeConstraint>;

struct SketchEntity {
    std::uint32_t index{};
    std::string native_id;
    std::string native_kind;
    SketchGeometry geometry;
    bool construction{};
    bool visible{true};
};

struct SketchPlacement {
    Vec3 origin{};
    Vec3 axis_x{1.0, 0.0, 0.0};
    Vec3 axis_y{0.0, 1.0, 0.0};
    Vec3 normal{0.0, 0.0, 1.0};
    bool resolved{};
};

struct SketchProfile {
    std::uint32_t index{};
    std::list<std::uint32_t> entity_indices;
    bool closed{};
    bool outer{};
};

struct Sketch {
    std::string id;
    std::string name;
    std::string native_id;
    SketchPlacement placement;
    std::list<SketchEntity> entities;
    std::list<SketchConstraint> constraints;
    std::list<SketchProfile> profiles;
    std::list<std::string> native_members;
    Bytes native_payload;
};

struct ParseOptions {
    bool preserve_native_payloads{true};
    bool emit_unknown_geometry{true};
    bool emit_native_constraints{true};
};

struct ParseReport {
    std::size_t sketches{};
    std::size_t entities{};
    std::size_t native_constraints{};
    std::size_t unknown_entities{};
    std::size_t unresolved_relations{};
    std::list<std::string> warnings;
};

struct ParseResult {
    std::list<Sketch> sketches;
    ParseReport report;
};

class ParseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class SketchParser {
public:
    explicit SketchParser(std::filesystem::path filename);
    [[nodiscard]] ParseResult parse(const ParseOptions& options = {}) const;

private:
    std::filesystem::path filename_;
};

[[nodiscard]] const char* to_string(ConstraintKind kind) noexcept;

} // namespace catia
