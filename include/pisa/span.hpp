// Copyright 2024 PISA developers
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <span>
#include <stdexcept>

namespace pisa {

/**
 * Element access with bound checking.
 *
 * The method `at()` is not available in C++20, therefore we use this helper function whenever we
 * want checked access.
 */
template <typename T>
[[nodiscard]] constexpr auto at(std::span<T> const& span, typename std::span<T>::size_type pos) ->
    typename std::span<T>::reference {
    if (pos >= span.size()) {
        throw std::out_of_range("out of range access to span");
    }
    return span[pos];
}

template <typename T>
[[nodiscard]] constexpr auto subspan_or_throw(
    std::span<T> const& span,
    typename std::span<T>::size_type offset,
    typename std::span<T>::size_type count,
    std::string const& error_msg
) -> std::span<T> {
    if (offset + count > span.size()) {
        throw std::out_of_range(error_msg);
    }
    return span.subspan(offset, count);
}

template <typename T>
[[nodiscard]] constexpr auto subspan_or_throw(
    std::span<T> const& span,
    typename std::span<T>::size_type offset,
    typename std::span<T>::size_type count
) -> std::span<T> {
    return subspan_or_throw(span, offset, count, "out of range subspan");
}

template <typename T>
[[nodiscard]] auto lex_lt(std::span<T> const& lhs, std::span<T> const& rhs) -> bool {
    auto lit = lhs.begin();
    auto rit = rhs.begin();
    while (lit != lhs.end() && rit != rhs.end()) {
        if (*lit < *rit) {
            return true;
        }
        if (*lit > *rit) {
            return false;
        }
        ++lit;
        ++rit;
    }
    return lit == lhs.end() && rit != rhs.end();
}

template <typename T>
[[nodiscard]] auto lex_eq(std::span<T> const& lhs, std::span<T> const& rhs) -> bool {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    auto lit = lhs.begin();
    auto rit = rhs.begin();
    while (lit != lhs.end()) {
        if (*lit++ != *rit++) {
            return false;
        }
    }
    return true;
}

}  // namespace pisa

namespace std {

template <typename T>
[[nodiscard]] auto operator==(std::span<T> const& lhs, std::span<T> const& rhs) -> bool {
    return ::pisa::lex_eq(lhs, rhs);
}

}  // namespace std
