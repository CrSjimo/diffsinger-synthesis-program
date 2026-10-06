#ifndef DSSP_ERROR_ERRORSET_H
#define DSSP_ERROR_ERRORSET_H

#include <expected>
#include <variant>

#include <dssperror/Error.h>

namespace dssp::error {

    namespace detail {
        template <class...>
        inline constexpr bool uniqueTypes = true;
        template <class First, class... Rest>
        inline constexpr bool uniqueTypes<First, Rest...> = (!std::same_as<First, Rest> && ...) && uniqueTypes<Rest...>;
    }

    template <class... Errors>
    class ErrorSet {
        static_assert(sizeof...(Errors) > 0 && detail::uniqueTypes<Errors...>);
        static_assert((std::derived_from<Errors, Error> && ...));
        static_assert((std::is_final_v<Errors> && ...));
        static_assert((std::is_nothrow_move_constructible_v<Errors> && ...));
        static_assert((std::is_nothrow_move_assignable_v<Errors> && ...));

    public:
        template <class E>
        static constexpr bool contains = (std::same_as<E, Errors> || ...);

        template <class E>
            requires contains<std::remove_cvref_t<E>>
        explicit ErrorSet(E &&error) : value_(std::in_place_type<std::remove_cvref_t<E>>, std::forward<E>(error)) {}

        template <class Visitor>
        decltype(auto) visit(Visitor &&visitor) const & { return std::visit(std::forward<Visitor>(visitor), value_); }
        template <class Visitor>
        decltype(auto) visit(Visitor &&visitor) && { return std::visit(std::forward<Visitor>(visitor), std::move(value_)); }
        template <class E>
            requires contains<E>
        const E *get_if() const noexcept {
            return std::get_if<E>(&value_);
        }
        const Error &asError() const noexcept {
            return visit([](const auto &error) -> const Error & { return error; });
        }

    private:
        std::variant<Errors...> value_;
    };

    template <class To, class... From>
        requires(To::template contains<From> && ...)
    To widen_errors(ErrorSet<From...> errors) {
        return std::move(errors).visit([](auto &&error) -> To { return To{std::move(error)}; });
    }

    template <class Set, class E>
        requires(Set::template contains<std::remove_cvref_t<E>>)
    std::unexpected<Set> fail(E &&error) {
        return std::unexpected{Set{std::forward<E>(error)}};
    }

    template <class... F>
    struct Overload : F... {
        using F::operator()...;
    };
    template <class... F>
    Overload(F...) -> Overload<F...>;

}

namespace dssp {
    using error::ErrorSet;
    using error::fail;
    using error::Overload;
    using error::widen_errors;
}

#endif // DSSP_ERROR_ERRORSET_H
