#pragma once

#include "catia_sketch.hpp"
#include <bit>
#include <span>

namespace catia {

class BinaryReader {
public:
    explicit BinaryReader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}
    [[nodiscard]] bool contains(std::size_t offset, std::size_t length) const noexcept {
        return offset <= bytes_.size() && length <= bytes_.size() - offset;
    }
    [[nodiscard]] std::size_t size() const noexcept { return bytes_.size(); }
    [[nodiscard]] std::uint8_t u8(std::size_t offset) const { require(offset, 1); return bytes_[offset]; }
    [[nodiscard]] std::uint32_t u32_le(std::size_t offset) const;
    [[nodiscard]] std::uint64_t u64_le(std::size_t offset) const;
    [[nodiscard]] float f32_le(std::size_t offset) const { return std::bit_cast<float>(u32_le(offset)); }
    [[nodiscard]] double f64_le(std::size_t offset) const { return std::bit_cast<double>(u64_le(offset)); }
    [[nodiscard]] std::span<const std::uint8_t> slice(std::size_t offset, std::size_t length) const;
private:
    void require(std::size_t offset, std::size_t length) const;
    std::span<const std::uint8_t> bytes_;
};

} // namespace catia
