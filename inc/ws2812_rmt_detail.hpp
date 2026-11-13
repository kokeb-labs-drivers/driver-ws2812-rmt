/**
 * @file ws2812_rmt_detail.hpp
 * @brief Compile-time helpers of the WS2812_RMT driver: exact hook matching and enumeration checks.
 * @copyright Copyright (c) 2026 The driver-ws2812-rmt authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Everything here is `constexpr` or used only in unevaluated operands, so it adds no code to the
 * run-time path (handbook C++20 profile, KB-CPP20-006, -008, -020, -021, -025, -026).
 */
#ifndef WS2812_RMT_DETAIL_HPP
#define WS2812_RMT_DETAIL_HPP

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace ws2812_rmt::detail {

/**
 * @brief Matches a port hook against exactly one signature, `R(Args...) noexcept`, as a member
 * function of `Port` (`const` or not) (KB-CPP20-006).
 *
 * `ExactHook<Port, Sig>::Take(&Port::Hook)` is well-formed only when `&Port::Hook` names a member
 * function of exactly that type: the address of an overloaded or template hook resolves to the
 * one that matches, and a hook with a narrower or different parameter, another result type or no
 * `noexcept` has no match. No value is converted. Declared only: it appears in
 * requires-expressions, never in evaluated code.
 *
 * @tparam Port The port class.
 * @tparam Sig The hook's function type, `noexcept`; a `Sig` without `noexcept` has no definition.
 */
template <typename Port, typename Sig>
  requires std::is_class_v<Port> && std::is_function_v<Sig>
struct ExactHook;

/// The `noexcept` signatures: the only ones a hook may have.
template <typename Port, typename R, typename... Args>
  requires std::is_class_v<Port>
struct ExactHook<Port, R(Args...) noexcept> {
  /// Takes the address of a non-`const` hook of exactly this signature.
  static void Take(R (Port::*hook)(Args...) noexcept) noexcept;
  /// Takes the address of a `const` hook of exactly this signature.
  static void Take(R (Port::*hook)(Args...) const noexcept) noexcept;
};

/// A class type: what `kAlwaysFalse` depends on, the port or another class of the branch.
template <typename T>
concept ClassType = std::is_class_v<T>;

/// `false` for every class `T`: the condition of a `static_assert` in a branch that must not be
/// instantiated (KB-CPP20-008).
template <ClassType>
inline constexpr bool kAlwaysFalse = false;

/// A scoped or unscoped enumeration: what the enumeration helpers take (KB-CPP20-005, -026).
template <typename E>
concept Enumeration = std::is_enum_v<E>;

/**
 * @brief The value of an enumerator in its underlying type, our stand-in for C++23
 * `std::to_underlying` (KB-CPP20-002, -026).
 */
template <Enumeration E>
[[nodiscard]] constexpr std::underlying_type_t<E> ToUnderlying(E e) noexcept {
  return static_cast<std::underlying_type_t<E>>(e);
}

/**
 * @brief The last enumerator of a contiguous enumeration that starts at 0; specialised next to each
 * enumeration that `IsEnumerator()` checks (KB-CPP20-026).
 */
template <Enumeration E>
struct EnumLast;

/**
 * @brief Whether `e` holds an enumerator of a contiguous enumeration that starts at 0. A value made
 * by `static_cast` from an integer can be anything its underlying type holds (KB-CPP20-025).
 */
template <Enumeration E>
[[nodiscard]] constexpr bool IsEnumerator(E e) noexcept {
  return static_cast<uint32_t>(ToUnderlying(e)) <=
         static_cast<uint32_t>(ToUnderlying(EnumLast<E>::kValue));
}

} // namespace ws2812_rmt::detail

#endif // WS2812_RMT_DETAIL_HPP
