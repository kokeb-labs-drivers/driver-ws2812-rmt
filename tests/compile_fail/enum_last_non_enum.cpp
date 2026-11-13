/**
 * @file enum_last_non_enum.cpp
 * @brief Must NOT compile: `detail::EnumLast` names the last enumerator of an enumeration, so an
 * integer type does not satisfy its constraint (handbook KB-CPP20-005, -026).
 *
 * EXPECT: Enumeration
 */
#include "ws2812_rmt_detail.hpp"

int main() {
  const ws2812_rmt::detail::EnumLast<int>* last = nullptr;
  return last == nullptr ? 0 : 1;
}
