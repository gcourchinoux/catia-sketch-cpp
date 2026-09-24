#include "catia_binary_reader.hpp"
#include "catia_sketch.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string_view>

namespace catia {
namespace {

constexpr std::array<std::uint8_t, 8> kCfv2Magic = {'V', '5', '_', 'C', 'F', 'V', '2', '\0'};

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw ParseError("cannot open CATPart file: " + path.string());
    }
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool is_valid_header(std::span<const std::uint8_t> bytes) {
    return bytes.size() >= kCfv2Magic.size() &&
           std::equal(kCfv2Magic.begin(), kCfv2Magic.end(), bytes.begin());
}

std::string ascii_string(std::span<const std::uint8_t> bytes, std::size_t offset, std::size_t length) {
    if (offset + length > bytes.size()) {
        return {};
    }
    return std::string(reinterpret_cast<const char*>(bytes.data() + offset), length);
}

std::optional<std::string> read_utf8_token(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset >= bytes.size()) {
        return std::nullopt;
    }
    std::size_t end = offset;
    while (end < bytes.size() && bytes[end] != 0) {
        ++end;
    }
    if (end == offset) {
        return std::nullopt;
    }
    return ascii_string(bytes, offset, end - offset);
}

std::optional<Vec2> maybe_point_payload(std::span<const std::uint8_t> payload) {
    if (payload.size() < 16) {
        return std::nullopt;
    }
    BinaryReader reader(payload);
    const auto x = reader.f64_le(0);
    const auto y = reader.f64_le(8);
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return std::nullopt;
    }
    return Vec2{x, y};
}

std::optional<NativeConstraint> make_native_constraint(
    std::size_t offset,
    std::span<const std::uint8_t> payload,
    std::string native_id,
    std::string native_class,
    std::string native_entry,
    std::list<std::string> refs) {
    if (native_class != "ConstraintDYS") {
        return std::nullopt;
    }
    NativeConstraint constraint;
    constraint.native_class = native_class;
    constraint.native_entry = native_entry;
    constraint.native_id = std::move(native_id);
    constraint.payload.assign(payload.begin(), payload.end());
    constraint.byte_offset = static_cast<std::uint64_t>(offset);
    constraint.referenced_native_objects = std::move(refs);
    return constraint;
}

std::list<std::string> find_referenced_record_ids(std::span<const std::uint8_t> bytes, std::size_t start)
{
    std::list<std::string> refs;
    std::size_t i = start;
    while (i + 8 < bytes.size()) {
        if (bytes[i] == 0x7c && bytes[i + 1] == 0x09) {
            const auto token = ascii_string(bytes, i + 2, 8);
            if (!token.empty()) {
                refs.push_back(token);
            }
        }
        ++i;
    }
    return refs;
}

std::string class_name_from_tokens(std::span<const std::uint8_t> bytes, std::size_t token_offset) {
    // Heuristic: in an object-graph stream, class names are ASCII markers near
    // the record start. This is intentionally conservative and matches the real
    // CATIA layout's expectation that class names are stored as symbolic tokens.
    auto maybe = read_utf8_token(bytes, token_offset);
    if (maybe && !maybe->empty() && *maybe != "\x00") {
        return *maybe;
    }
    return {};
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
    for (unsigned i = 0; i < 8; ++i) {
        result |= static_cast<std::uint64_t>(bytes_[offset + i]) << (8U * i);
    }
    return result;
}

std::span<const std::uint8_t> BinaryReader::slice(std::size_t offset, std::size_t length) const {
    require(offset, length);
    return bytes_.subspan(offset, length);
}

void BinaryReader::require(std::size_t offset, std::size_t length) const {
    if (!contains(offset, length)) {
        throw ParseError("CATIA sketch record exceeds input boundary");
    }
}

SketchParser::SketchParser(std::filesystem::path filename) : filename_(std::move(filename)) {}

ParseResult SketchParser::parse(const ParseOptions& options) const {
    const auto bytes = read_file(filename_);
    std::span<const std::uint8_t> data(bytes);

    ParseResult result;
    if (!is_valid_header(data)) {
        throw ParseError("input is not a CATIA V5 CFV2 file");
    }

    std::list<std::string> warnings;
    std::list<Sketch> sketches;
    std::list<SketchEntity> entities;
    std::list<SketchConstraint> constraints;

    // We deliberately index the entities and constraints in a conservative way:
    // by finding symbolic class names in the native object graph and by reading a
    // few numeric bytes in recognized payloads.
    std::size_t i = 0;
    std::size_t sketch_index = 0;
    while (i + 4 < data.size()) {
        if (data[i] == 0x7c && data[i + 1] == 0x08) {
            // A 7C08 object-graph root is a native graph boundary.
            const auto graph_tag = data[i];
            (void)graph_tag;
        }

        if (data[i] == 0x7c && data[i + 1] == 0x09) {
            const auto start = i;
            const auto class_name = class_name_from_tokens(data, i + 2);
            if (!class_name.empty()) {
                if (class_name == "Sketch") {
                    Sketch sketch;
                    sketch.id = "catia:sketch#" + std::to_string(sketch_index++);
                    sketch.name = "Sketch";
                    sketch.native_id = "graph:7c09@" + std::to_string(start);
                    sketch.native_payload.assign(data.begin() + start, data.begin() + std::min(start + 64, data.size()));
                    sketches.push_back(sketch);
                } else if (class_name == "2DPoint") {
                    SketchEntity entity;
                    entity.index = static_cast<std::uint32_t>(entities.size());
                    entity.native_id = "catia:entity#" + std::to_string(entity.index);
                    entity.native_kind = class_name;

                    const auto payload_start = std::min(i + 16, data.size());
                    auto payload = std::vector<std::uint8_t>(data.begin() + i + 2, data.begin() + payload_start);
                    if (auto point = maybe_point_payload(payload)) {
                        entity.geometry = Point2D{{point->x, point->y}};
                    } else if (options.emit_unknown_geometry) {
                        entity.geometry = UnknownGeometry{class_name, payload};
                    }
                    entities.push_back(entity);
                } else if (class_name == "ConstraintDYS") {
                    const auto payload_start = std::min(i + 16, data.size());
                    std::vector<std::uint8_t> payload(data.begin() + i + 2, data.begin() + payload_start);
                    const auto refs = find_referenced_record_ids(data, i + 2);
                    auto native = make_native_constraint(
                        i,
                        payload,
                        "catia:constraint#" + std::to_string(constraints.size()),
                        class_name,
                        "ConstraintDYS",
                        refs);
                    if (native.has_value()) {
                        constraints.push_back(NativeConstraint{native->native_class, native->native_entry, native->native_id, native->payload, native->referenced_native_objects, native->byte_offset});
                    }
                }
            }
        }

        ++i;
    }

    // Link the found entities to a sketch when the record set contains a Sketch.
    for (auto& sketch : sketches) {
        sketch.entities = entities;
        sketch.native_members.reserve(entities.size());
        for (const auto& entity : entities) {
            sketch.native_members.push_back(entity.native_id);
        }
        for (const auto& constraint : constraints) {
            sketch.constraints.push_back(constraint);
        }
    }

    if (sketches.empty()) {
        Sketch fallback;
        fallback.id = "catia:sketch#0";
        fallback.name = "Sketch";
        fallback.native_id = "unknown";
        fallback.native_payload = bytes;
        if (!entities.empty()) {
            fallback.entities = entities;
        }
        if (!constraints.empty()) {
            for (const auto& constraint : constraints) {
                fallback.constraints.push_back(constraint);
            }
        }
        sketches.push_back(fallback);
        warnings.push_back("No concrete CATIA Sketch owner record was identified; a fallback native sketch was emitted.");
    }

    result.sketches = std::move(sketches);
    result.report.sketches = result.sketches.size();
    result.report.entities = 0;
    result.report.native_constraints = 0;
    for (const auto& sketch : result.sketches) {
        result.report.entities += sketch.entities.size();
        for (const auto& entry : sketch.constraints) {
            std::visit([&](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, NativeConstraint>) {
                    ++result.report.native_constraints;
                }
            }, entry);
        }
    }
    result.report.warnings = std::move(warnings);

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
