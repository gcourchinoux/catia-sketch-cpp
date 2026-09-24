#include "catia_sketch.hpp"
#include "catia_binary_reader.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <span>

namespace catia {
namespace {

constexpr std::uint8_t kV5Magic[] = {'V','5','_','C','F','V','2',0};

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw ParseError("cannot open CATPart file: " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

bool finite(double value) { return std::isfinite(value); }
bool finite(Vec2 value) { return finite(value.x) && finite(value.y); }

SketchGeometry decode_known_geometry(const std::string& kind,
                                     std::span<const std::uint8_t> bytes,
                                     bool emit_unknown) {
    BinaryReader reader(bytes);
    if (kind == "2DPoint" && reader.size() >= 16) {
        Point2D point{{reader.f64_le(0), reader.f64_le(8)}};
        if (finite(point.position)) return point;
    } else if (kind == "Line2D" && reader.size() >= 32) {
        Line2D line{{reader.f64_le(0), reader.f64_le(8)}, {reader.f64_le(16), reader.f64_le(24)}};
        if (finite(line.start) && finite(line.end)) return line;
    } else if (kind == "Circle2D" && reader.size() >= 24) {
        Circle2D circle{{reader.f64_le(0), reader.f64_le(8)}, reader.f64_le(16)};
        if (finite(circle.center) && finite(circle.radius) && circle.radius > 0.0) return circle;
    } else if (kind == "Arc2D" && reader.size() >= 41) {
        Arc2D arc{{reader.f64_le(0), reader.f64_le(8)}, reader.f64_le(16), reader.f64_le(24), reader.f64_le(32), reader.u8(40) != 0};
        if (finite(arc.center) && finite(arc.radius) && finite(arc.start_angle) && finite(arc.end_angle) && arc.radius > 0.0) return arc;
    }
    if (emit_unknown) return UnknownGeometry{kind, Bytes(bytes.begin(), bytes.end())};
    return UnknownGeometry{};
}

} // namespace

std::uint32_t BinaryReader::u32_le(std::size_t offset) const {
    require(offset, 4);
    return static_cast<std::uint32_t>(bytes_[offset]) |
           (static_cast<std::uint32_t>(bytes_[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes_[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes_[offset + 3]) << 24);
}

std::uint64_t BinaryReader::u64_le(std::size_t offset) const {
    require(offset, 8);
    std::uint64_t result = 0;
    for (unsigned i = 0; i < 8; ++i) result |= static_cast<std::uint64_t>(bytes_[offset + i]) << (8 * i);
    return result;
}

std::span<const std::uint8_t> BinaryReader::slice(std::size_t offset, std::size_t length) const {
    require(offset, length);
    return bytes_.subspan(offset, length);
}

void BinaryReader::require(std::size_t offset, std::size_t length) const {
    if (!contains(offset, length)) throw ParseError("CATIA sketch record exceeds input boundary");
}

SketchParser::SketchParser(std::filesystem::path filename) : filename_(std::move(filename)) {}

ParseResult SketchParser::parse(const ParseOptions& options) const {
    const auto bytes = read_file(filename_);
    ParseResult result;
    if (bytes.size() < sizeof(kV5Magic) || !std::equal(std::begin(kV5Magic), std::end(kV5Magic), bytes.begin())) {
        throw ParseError("input is not a CATIA V5 CFV2 file");
    }

    // The CATIA specification identifies Sketch and its relations in the
    // outer 7C08/7C09 object graph. This conservative first implementation
    // preserves the source bytes and does not invent a sketch when ownership
    // and field identity cannot be proven.
    result.report.warnings.push_back(
        "Sketch object-graph ownership is not exposed by this build; native bytes were not guessed.");
    if (options.preserve_native_payloads) {
        Sketch native;
        native.id = "catia:native:document";
        native.native_id = native.id;
        native.native_payload = bytes;
        result.sketches.push_back(std::move(native));
    }
    return result;
}

const char* to_string(ConstraintKind kind) noexcept {
    switch (kind) {
    case ConstraintKind::Coincident: return "coincident";
    case ConstraintKind::Horizontal: return "horizontal";
    case ConstraintKind::Vertical: return "vertical";
    case ConstraintKind::Parallel: return "parallel";
    case ConstraintKind::Perpendicular: return "perpendicular";
    case ConstraintKind::Tangent: return "tangent";
    case ConstraintKind::Equal: return "equal";
    case ConstraintKind::Concentric: return "concentric";
    case ConstraintKind::Symmetric: return "symmetric";
    case ConstraintKind::Distance: return "distance";
    case ConstraintKind::DistanceX: return "distance-x";
    case ConstraintKind::DistanceY: return "distance-y";
    case ConstraintKind::Radius: return "radius";
    case ConstraintKind::Diameter: return "diameter";
    case ConstraintKind::Angle: return "angle";
    case ConstraintKind::Fix: return "fix";
    case ConstraintKind::Unknown: return "unknown";
    }
    return "unknown";
}

} // namespace catia
