#ifndef CODECUTIL_CODECUTIL_H
#define CODECUTIL_CODECUTIL_H

#include <cstddef>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace dssp::codecutil {

    enum class Base64Mode {
        Normal,
        URL,
        OmitEquals,
    };

    class HexEncodingView {
    public:
        using value_type = char;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;

        class iterator {
        public:
            using iterator_category = std::random_access_iterator_tag;
            using iterator_concept = std::random_access_iterator_tag;
            using value_type = char;
            using difference_type = std::ptrdiff_t;
            using reference = char;
            using pointer = void;

            constexpr iterator() noexcept = default;

            constexpr char operator*() const noexcept {
                const auto byte = std::to_integer<unsigned char>(m_data[m_pos / 2]);
                const auto nibble = static_cast<unsigned char>((m_pos & 1) ? (byte & 0x0f) : (byte >> 4));
                return nibble < 10 ? static_cast<char>('0' + nibble) : static_cast<char>('a' + nibble - 10);
            }

            constexpr char operator[](difference_type n) const noexcept { return *(*this + n); }

            constexpr iterator &operator++() noexcept {
                ++m_pos;
                return *this;
            }

            constexpr iterator operator++(int) noexcept {
                auto old = *this;
                ++*this;
                return old;
            }

            constexpr iterator &operator--() noexcept {
                --m_pos;
                return *this;
            }

            constexpr iterator operator--(int) noexcept {
                auto old = *this;
                --*this;
                return old;
            }

            constexpr iterator &operator+=(difference_type n) noexcept {
                m_pos += n;
                return *this;
            }

            constexpr iterator &operator-=(difference_type n) noexcept {
                m_pos -= n;
                return *this;
            }

            friend constexpr iterator operator+(iterator it, difference_type n) noexcept {
                it += n;
                return it;
            }

            friend constexpr iterator operator+(difference_type n, iterator it) noexcept {
                it += n;
                return it;
            }

            friend constexpr iterator operator-(iterator it, difference_type n) noexcept {
                it -= n;
                return it;
            }

            friend constexpr difference_type operator-(iterator lhs, iterator rhs) noexcept { return lhs.m_pos - rhs.m_pos; }

            friend constexpr bool operator==(iterator lhs, iterator rhs) noexcept { return lhs.m_pos == rhs.m_pos; }

            friend constexpr auto operator<=>(iterator lhs, iterator rhs) noexcept { return lhs.m_pos <=> rhs.m_pos; }

        private:
            friend class HexEncodingView;

            constexpr iterator(const std::byte *data, difference_type pos) noexcept : m_data(data), m_pos(pos) {}

            const std::byte *m_data = nullptr;
            difference_type m_pos = 0;
        };

        constexpr HexEncodingView() noexcept = default;

        template <class T, std::size_t Extent>
        constexpr explicit HexEncodingView(std::span<T, Extent> span) noexcept : m_bytes(std::as_bytes(span)) {}

        template <class Container>
            requires requires(Container &c) { std::span{c}; }
        constexpr explicit HexEncodingView(Container &container) noexcept : HexEncodingView(std::span{container}) {}

        [[nodiscard]]
        constexpr iterator begin() const noexcept {
            return iterator{m_bytes.data(), 0};
        }

        [[nodiscard]]
        constexpr iterator end() const noexcept {
            return iterator{m_bytes.data(), static_cast<difference_type>(size())};
        }

        [[nodiscard]]
        constexpr size_type size() const noexcept {
            return m_bytes.size() * 2;
        }

        [[nodiscard]]
        constexpr bool empty() const noexcept {
            return m_bytes.empty();
        }

        [[nodiscard]]
        constexpr char operator[](size_type index) const noexcept {
            return begin()[static_cast<difference_type>(index)];
        }

    private:
        std::span<const std::byte> m_bytes;
    };

    class HexDecodingView {
    public:
        using value_type = unsigned char;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;

        class invalid_length : public std::length_error {
        public:
            explicit invalid_length(size_type size) : std::length_error{"hex string length must be even, got " + std::to_string(size)} {}
        };

        class invalid_digit : public std::invalid_argument {
        public:
            invalid_digit(difference_type position, char digit) : std::invalid_argument{"invalid hex digit '" + std::string(1, digit) + "' at position " + std::to_string(position)} {}
        };

        class iterator {
        public:
            using iterator_category = std::random_access_iterator_tag;
            using iterator_concept = std::random_access_iterator_tag;
            using value_type = unsigned char;
            using difference_type = std::ptrdiff_t;
            using reference = unsigned char;
            using pointer = void;

            constexpr iterator() noexcept = default;

            constexpr unsigned char operator*() const { return decode_pair(m_pos); }

            constexpr unsigned char operator[](difference_type n) const { return *(*this + n); }

            constexpr iterator &operator++() noexcept {
                ++m_pos;
                return *this;
            }

            constexpr iterator operator++(int) noexcept {
                auto old = *this;
                ++*this;
                return old;
            }

            constexpr iterator &operator--() noexcept {
                --m_pos;
                return *this;
            }

            constexpr iterator operator--(int) noexcept {
                auto old = *this;
                --*this;
                return old;
            }

            constexpr iterator &operator+=(difference_type n) noexcept {
                m_pos += n;
                return *this;
            }

            constexpr iterator &operator-=(difference_type n) noexcept {
                m_pos -= n;
                return *this;
            }

            friend constexpr iterator operator+(iterator it, difference_type n) noexcept {
                it += n;
                return it;
            }

            friend constexpr iterator operator+(difference_type n, iterator it) noexcept {
                it += n;
                return it;
            }

            friend constexpr iterator operator-(iterator it, difference_type n) noexcept {
                it -= n;
                return it;
            }

            friend constexpr difference_type operator-(iterator lhs, iterator rhs) noexcept { return lhs.m_pos - rhs.m_pos; }

            friend constexpr bool operator==(iterator lhs, iterator rhs) noexcept { return lhs.m_pos == rhs.m_pos; }

            friend constexpr auto operator<=>(iterator lhs, iterator rhs) noexcept { return lhs.m_pos <=> rhs.m_pos; }

        private:
            friend class HexDecodingView;

            constexpr iterator(const char *data, difference_type pos) noexcept : m_data(data), m_pos(pos) {}

            [[nodiscard]]
            static constexpr unsigned char decode_nibble(char c, difference_type position) {
                if (c >= '0' && c <= '9') {
                    return static_cast<unsigned char>(c - '0');
                }
                if (c >= 'a' && c <= 'f') {
                    return static_cast<unsigned char>(c - 'a' + 10);
                }
                if (c >= 'A' && c <= 'F') {
                    return static_cast<unsigned char>(c - 'A' + 10);
                }
                throw invalid_digit{position, c};
            }

            [[nodiscard]]
            constexpr unsigned char decode_pair(difference_type pos) const {
                const auto index = pos * 2;
                const auto high = decode_nibble(m_data[index], index);
                const auto low = decode_nibble(m_data[index + 1], index + 1);
                return static_cast<unsigned char>((high << 4) | low);
            }

            const char *m_data = nullptr;
            difference_type m_pos = 0;
        };

        constexpr HexDecodingView() noexcept = default;

        template <class T, std::size_t Extent>
        constexpr explicit HexDecodingView(std::span<T, Extent> span) : m_chars(span) {
            if (span.size() % 2 != 0) {
                throw invalid_length{span.size()};
            }
        }

        template <class Container>
            requires requires(Container &c) { std::span{c}; }
        constexpr explicit HexDecodingView(Container &container) : HexDecodingView(std::span{container}) {}

        [[nodiscard]]
        constexpr iterator begin() const noexcept {
            return iterator{m_chars.data(), 0};
        }

        [[nodiscard]]
        constexpr iterator end() const noexcept {
            return iterator{m_chars.data(), static_cast<difference_type>(size())};
        }

        [[nodiscard]]
        constexpr size_type size() const noexcept {
            return m_chars.size() / 2;
        }

        [[nodiscard]]
        constexpr bool empty() const noexcept {
            return m_chars.empty();
        }

        [[nodiscard]]
        constexpr unsigned char operator[](size_type index) const {
            return begin()[static_cast<difference_type>(index)];
        }

    private:
        std::span<const char> m_chars;
    };

    template <Base64Mode Mode = Base64Mode::Normal>
    class Base64EncodingView {
    public:
        using value_type = char;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;

        class iterator {
        public:
            using iterator_category = std::random_access_iterator_tag;
            using iterator_concept = std::random_access_iterator_tag;
            using value_type = char;
            using difference_type = std::ptrdiff_t;
            using reference = char;
            using pointer = void;

            constexpr iterator() noexcept = default;

            constexpr char operator*() const noexcept {
                const auto groups = m_total / 3;
                const auto rem = m_total % 3;
                const auto full_chars = groups * 4;

                if (m_pos < full_chars) {
                    const auto cpos = static_cast<size_type>(m_pos % 4);
                    const auto b = (m_pos / 4) * 3;
                    switch (cpos) {
                        case 0: return alphabet(byte(b) >> 2);
                        case 1: return alphabet(((byte(b) & 0x03) << 4) | (byte(b + 1) >> 4));
                        case 2: return alphabet(((byte(b + 1) & 0x0f) << 2) | (byte(b + 2) >> 6));
                        default: return alphabet(byte(b + 2) & 0x3f);
                    }
                }

                const auto rpos = static_cast<size_type>(m_pos - full_chars);
                const auto b = groups * 3;
                if (rpos == 0) {
                    return alphabet(byte(b) >> 2);
                }
                if (rpos == 1) {
                    return alphabet(((byte(b) & 0x03) << 4) | (rem == 2 ? byte(b + 1) >> 4 : 0));
                }
                if (rem == 1) {
                    return '=';
                }
                if (rpos == 2) {
                    return alphabet((byte(b + 1) & 0x0f) << 2);
                }
                return '=';
            }

            constexpr char operator[](difference_type n) const noexcept { return *(*this + n); }

            constexpr iterator &operator++() noexcept {
                ++m_pos;
                return *this;
            }

            constexpr iterator operator++(int) noexcept {
                auto old = *this;
                ++*this;
                return old;
            }

            constexpr iterator &operator--() noexcept {
                --m_pos;
                return *this;
            }

            constexpr iterator operator--(int) noexcept {
                auto old = *this;
                --*this;
                return old;
            }

            constexpr iterator &operator+=(difference_type n) noexcept {
                m_pos += n;
                return *this;
            }

            constexpr iterator &operator-=(difference_type n) noexcept {
                m_pos -= n;
                return *this;
            }

            friend constexpr iterator operator+(iterator it, difference_type n) noexcept {
                it += n;
                return it;
            }

            friend constexpr iterator operator+(difference_type n, iterator it) noexcept {
                it += n;
                return it;
            }

            friend constexpr iterator operator-(iterator it, difference_type n) noexcept {
                it -= n;
                return it;
            }

            friend constexpr difference_type operator-(iterator lhs, iterator rhs) noexcept { return lhs.m_pos - rhs.m_pos; }

            friend constexpr bool operator==(iterator lhs, iterator rhs) noexcept { return lhs.m_pos == rhs.m_pos; }

            friend constexpr auto operator<=>(iterator lhs, iterator rhs) noexcept { return lhs.m_pos <=> rhs.m_pos; }

        private:
            friend class Base64EncodingView;

            constexpr iterator(const std::byte *data, difference_type pos, difference_type total) noexcept : m_data(data), m_pos(pos), m_total(total) {}

            [[nodiscard]]
            static constexpr char alphabet(size_type index) noexcept {
                constexpr const char *normal = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                constexpr const char *url = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
                return (Mode == Base64Mode::URL ? url : normal)[index];
            }

            [[nodiscard]]
            constexpr unsigned char byte(difference_type index) const noexcept {
                return std::to_integer<unsigned char>(m_data[index]);
            }

            const std::byte *m_data = nullptr;
            difference_type m_pos = 0;
            difference_type m_total = 0;
        };

        constexpr Base64EncodingView() noexcept = default;

        template <class T, std::size_t Extent>
        constexpr explicit Base64EncodingView(std::span<T, Extent> span) noexcept : m_bytes(std::as_bytes(span)) {}

        template <class Container>
            requires requires(Container &c) { std::span{c}; }
        constexpr explicit Base64EncodingView(Container &container) noexcept : Base64EncodingView(std::span{container}) {}

        [[nodiscard]]
        constexpr iterator begin() const noexcept {
            return iterator{m_bytes.data(), 0, static_cast<difference_type>(m_bytes.size())};
        }

        [[nodiscard]]
        constexpr iterator end() const noexcept {
            return iterator{m_bytes.data(), static_cast<difference_type>(size()), static_cast<difference_type>(m_bytes.size())};
        }

        [[nodiscard]]
        constexpr size_type size() const noexcept {
            const auto n = m_bytes.size();
            const auto r = n % 3;
            size_type extra = 0;
            if (r == 1) {
                extra = m_omitEquals ? 2 : 4;
            } else if (r == 2) {
                extra = m_omitEquals ? 3 : 4;
            }
            return n / 3 * 4 + extra;
        }

        [[nodiscard]]
        constexpr bool empty() const noexcept {
            return m_bytes.empty();
        }

        [[nodiscard]]
        constexpr char operator[](size_type index) const noexcept {
            return begin()[static_cast<difference_type>(index)];
        }

    private:
        static constexpr bool m_omitEquals = (Mode == Base64Mode::OmitEquals);
        std::span<const std::byte> m_bytes;
    };

    template <Base64Mode Mode = Base64Mode::Normal>
    class Base64DecodingView {
    public:
        using value_type = unsigned char;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;

        class invalid_length : public std::length_error {
        public:
            explicit invalid_length(size_type size) : std::length_error{"base64 input length must be a valid multiple, got " + std::to_string(size)} {}
        };

        class invalid_character : public std::invalid_argument {
        public:
            invalid_character(difference_type position, char character) : std::invalid_argument{"invalid base64 character '" + std::string(1, character) + "' at position " + std::to_string(position)} {}
        };

        class invalid_padding : public std::invalid_argument {
        public:
            explicit invalid_padding(difference_type position) : std::invalid_argument{"unexpected padding '=' at position " + std::to_string(position)} {}
        };

        class iterator {
        public:
            using iterator_category = std::random_access_iterator_tag;
            using iterator_concept = std::random_access_iterator_tag;
            using value_type = unsigned char;
            using difference_type = std::ptrdiff_t;
            using reference = unsigned char;
            using pointer = void;

            constexpr iterator() noexcept = default;

            constexpr unsigned char operator*() const noexcept {
                const auto bindex = (m_pos / 3) * 4;
                const auto a = value_of(m_data[bindex]);
                const auto b = value_of(m_data[bindex + 1]);
                const auto cpos = static_cast<size_type>(m_pos % 3);
                if (cpos == 0) {
                    return static_cast<unsigned char>((a << 2) | (b >> 4));
                }
                const auto c = value_of(m_data[bindex + 2]);
                if (cpos == 1) {
                    return static_cast<unsigned char>((b << 4) | (c >> 2));
                }
                return static_cast<unsigned char>((c << 6) | value_of(m_data[bindex + 3]));
            }

            constexpr unsigned char operator[](difference_type n) const noexcept { return *(*this + n); }

            constexpr iterator &operator++() noexcept {
                ++m_pos;
                return *this;
            }

            constexpr iterator operator++(int) noexcept {
                auto old = *this;
                ++*this;
                return old;
            }

            constexpr iterator &operator--() noexcept {
                --m_pos;
                return *this;
            }

            constexpr iterator operator--(int) noexcept {
                auto old = *this;
                --*this;
                return old;
            }

            constexpr iterator &operator+=(difference_type n) noexcept {
                m_pos += n;
                return *this;
            }

            constexpr iterator &operator-=(difference_type n) noexcept {
                m_pos -= n;
                return *this;
            }

            friend constexpr iterator operator+(iterator it, difference_type n) noexcept {
                it += n;
                return it;
            }

            friend constexpr iterator operator+(difference_type n, iterator it) noexcept {
                it += n;
                return it;
            }

            friend constexpr iterator operator-(iterator it, difference_type n) noexcept {
                it -= n;
                return it;
            }

            friend constexpr difference_type operator-(iterator lhs, iterator rhs) noexcept { return lhs.m_pos - rhs.m_pos; }

            friend constexpr bool operator==(iterator lhs, iterator rhs) noexcept { return lhs.m_pos == rhs.m_pos; }

            friend constexpr auto operator<=>(iterator lhs, iterator rhs) noexcept { return lhs.m_pos <=> rhs.m_pos; }

        private:
            friend class Base64DecodingView;

            constexpr iterator(const char *data, difference_type pos) noexcept : m_data(data), m_pos(pos) {}

            [[nodiscard]]
            static constexpr unsigned char value_of(char c) noexcept {
                if (c >= 'A' && c <= 'Z') {
                    return static_cast<unsigned char>(c - 'A');
                }
                if (c >= 'a' && c <= 'z') {
                    return static_cast<unsigned char>(c - 'a' + 26);
                }
                if (c >= '0' && c <= '9') {
                    return static_cast<unsigned char>(c - '0' + 52);
                }
                if (c == (Mode == Base64Mode::URL ? '-' : '+')) {
                    return 62;
                }
                if (c == (Mode == Base64Mode::URL ? '_' : '/')) {
                    return 63;
                }
                return 0;
            }

            const char *m_data = nullptr;
            difference_type m_pos = 0;
        };

        constexpr Base64DecodingView() noexcept = default;

        template <class T, std::size_t Extent>
        constexpr explicit Base64DecodingView(std::span<T, Extent> span) : m_chars(span) { validate(); }

        template <class Container>
            requires requires(Container &c) { std::span{c}; }
        constexpr explicit Base64DecodingView(Container &container) : Base64DecodingView(std::span{container}) {}

        [[nodiscard]]
        constexpr iterator begin() const noexcept {
            return iterator{m_chars.data(), 0};
        }

        [[nodiscard]]
        constexpr iterator end() const noexcept {
            return iterator{m_chars.data(), static_cast<difference_type>(m_size)};
        }

        [[nodiscard]]
        constexpr size_type size() const noexcept {
            return m_size;
        }

        [[nodiscard]]
        constexpr bool empty() const noexcept {
            return m_chars.empty();
        }

        [[nodiscard]]
        constexpr unsigned char operator[](size_type index) const noexcept {
            return begin()[static_cast<difference_type>(index)];
        }

    private:
        static constexpr bool m_omitEquals = (Mode == Base64Mode::OmitEquals);

        [[nodiscard]]
        static constexpr bool is_valid_char(char c) noexcept {
            if (c >= 'A' && c <= 'Z') {
                return true;
            }
            if (c >= 'a' && c <= 'z') {
                return true;
            }
            if (c >= '0' && c <= '9') {
                return true;
            }
            if (c == (Mode == Base64Mode::URL ? '-' : '+')) {
                return true;
            }
            if (c == (Mode == Base64Mode::URL ? '_' : '/')) {
                return true;
            }
            return false;
        }

        [[nodiscard]]
        constexpr size_type decoded_size(size_type n) const noexcept {
            if (m_omitEquals) {
                const auto r = n % 4;
                return n / 4 * 3 + (r == 2 ? 1 : r == 3 ? 2
                                                        : 0);
            }
            auto eq = size_type{0};
            if (n >= 2 && m_chars[n - 1] == '=') {
                ++eq;
            }
            if (n >= 2 && m_chars[n - 2] == '=') {
                ++eq;
            }
            return n / 4 * 3 - eq;
        }

        constexpr void validate() {
            const auto n = m_chars.size();

            if (m_omitEquals) {
                if (n % 4 == 1) {
                    throw invalid_length{n};
                }
            } else {
                if (n % 4 != 0) {
                    throw invalid_length{n};
                }
            }

            m_size = decoded_size(n);

            for (difference_type i = 0; i < static_cast<difference_type>(n); ++i) {
                const auto c = m_chars[static_cast<size_type>(i)];
                if (c == '=') {
                    if (m_omitEquals || static_cast<size_type>(i) + 4 < n || static_cast<size_type>(i) < n - 2) {
                        throw invalid_padding{i};
                    }
                    continue;
                }
                if (!is_valid_char(c)) {
                    throw invalid_character{i, c};
                }
            }
        }

        std::span<const char> m_chars;
        size_type m_size = 0;
    };

}

#endif // CODECUTIL_CODECUTIL_H
