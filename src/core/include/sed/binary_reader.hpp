#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace sed
{

class ByteReader
{
public:
    explicit ByteReader(const std::span<const std::byte> data) noexcept : data_(data)
    {
    }

    std::size_t position() const noexcept
    {
        return pos_;
    }

    std::size_t remaining() const noexcept
    {
        return data_.size() - pos_;
    }

    void seek(const std::size_t newPos)
    {
        if (newPos > data_.size())
        {
            throw std::runtime_error("ByteReader: seek past end");
        }
        pos_ = newPos;
    }

    std::span<const std::byte> readBytes(const std::size_t count)
    {
        if (count > remaining())
        {
            throw std::runtime_error("ByteReader: read past end");
        }
        const auto out = data_.subspan(pos_, count);
        pos_ += count;
        return out;
    }

    std::string_view readString(const std::size_t count)
    {
        const auto bytes = readBytes(count);
        return std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    template <typename IntT>
    IntT readLE()
    {
        static_assert(std::is_integral_v<IntT>, "readLE only supports integral types");
        static_assert(sizeof(IntT) == 1 || sizeof(IntT) == 2 || sizeof(IntT) == 4, "readLE supports 1/2/4 byte ints");

        using UIntT = std::make_unsigned_t<IntT>;
        const auto bytes = readBytes(sizeof(IntT));

        UIntT value = 0;
        for (std::size_t i = 0; i < bytes.size(); ++i)
        {
            const UIntT b = static_cast<UIntT>(std::to_integer<unsigned char>(bytes[i]));
            value |= (b << (8u * static_cast<unsigned>(i)));
        }
        if constexpr (std::is_signed_v<IntT>)
        {
            return std::bit_cast<IntT>(value);
        }
        else
        {
            return value;
        }
    }

private:
    std::span<const std::byte> data_{};
    std::size_t pos_{};
};

} // namespace sed
