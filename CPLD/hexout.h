#pragma once
#include <iostream>
#include <iomanip>
#include <concepts>
#include <type_traits>
#include <utility>

template <typename T>
concept HexFormattable = std::integral<T> || std::is_enum_v<T>;

template <HexFormattable T>
struct HexOut {
  T val;
  int width;

  constexpr HexOut(T v, int w = 8) : val(v), width(w) {}

  friend std::ostream& operator<<(std::ostream& os, const HexOut& h) {
    auto printVal = []<typename U>(U v) {
      if constexpr (std::is_enum_v<U>) {
        return static_cast<std::underlying_type_t<U>>(v);
      } else {
        return v;
      }
    };

    os << "0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(h.width) << printVal(h.val);
    return os;
  }
};

template <HexFormattable T>
HexOut(T) -> HexOut<T>;

template <HexFormattable T>
HexOut(T, int) -> HexOut<T>;
