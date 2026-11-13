/**
 * @file mock_ws2812_rmt.hpp
 * @brief A model of the RMT transmit path and of a chain of pixels, written
 * from the datasheets on its own, and the mock ports.
 *
 * The model has two parts and neither includes the driver's profile table
 * (handbook KB-TST-030):
 *
 * - `FakeRmtCore` (the port behaviour): expands every `Transmit()` into the
 * recorded list of (level, duration in ticks) half-symbols exactly as the ESP32
 * RMT bytes encoder does, with the reset as one or more low symbols of at most
 * 15 bits. Knobs: delivered tick rate and skew, per-symbol jitter, failing
 * hooks, a timeout, an underrun gap after a byte.
 * - `PixelChainModel`: N pixels that decode the half-symbols against the timing
 * windows of one datasheet (one `Document` per datasheet, not per profile),
 * latch on a low above the reset minimum, keep bits
 *   `[k*bits, (k+1)*bits)` in pixel k, apply the document's colour order and
 * expose the latched colours.
 *
 * | Port | Hooks |
 * | --- | --- |
 * | `MinimalPort` | the four required ones (`DelayUs()` is in `FakeRmtCore`) |
 * | `FullPort` | also `GpioSet()` and `Log()` |
 */
#ifndef WS2812_RMT_MOCK_WS2812_RMT_HPP
#define WS2812_RMT_MOCK_WS2812_RMT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

#include "ws2812_rmt_gpio_interface.hpp"
#include "ws2812_rmt_types.hpp"

namespace ws2812_rmt_test {

/// The channel a datasheet's colour-order letter names ('R', 'G', 'B', anything else W), found by
/// position in a table rather than by the letter's numerical value (MISRA C++:2023 Rule 7.0.3).
inline ws2812_rmt::Channel ChannelOfLetter(char letter) noexcept {
  constexpr std::string_view kLetters = "RGB";
  constexpr std::array<ws2812_rmt::Channel, 3> kChannels = {
      ws2812_rmt::Channel::R, ws2812_rmt::Channel::G, ws2812_rmt::Channel::B};
  const size_t at = kLetters.find(letter);
  return (at < kChannels.size()) ? kChannels[at] : ws2812_rmt::Channel::W;
}

/// An enumeration value that is not an enumerator, built from raw storage (a
/// test of the driver's rejection of corrupt input; a plain cast of a literal
/// is flagged by the analyzer).
template <typename E>
[[nodiscard]] E RawEnum(unsigned v) noexcept {
  using U = std::underlying_type_t<E>;
  const U raw = static_cast<U>(v);
  E e{};
  std::memcpy(&e, &raw, sizeof e);
  return e;
}

/// A window in nanoseconds; `max_ns` 0 means the datasheet gives no maximum.
struct Win {
  uint32_t min_ns;
  uint32_t max_ns;
};

/// One datasheet's wire timing: the model's own data, one row per document of
/// docs/datasheets.yml.
struct Document {
  const char* id;              ///< The id used in docs/datasheets.yml.
  ws2812_rmt::Variant variant; ///< The variant whose profile must sit inside this document.
  Win t0h;
  Win t0l;
  Win t1h;
  Win t1l;
  uint32_t period_min_ns; ///< 0 none.
  uint32_t period_max_ns; ///< 0 none.
  uint32_t reset_min_ns;  ///< A low strictly above this latches.
  uint32_t max_low_ns;    ///< 0: below the reset is allowed; else a low inside a
                          ///< frame must stay below it.
  const char* order;      ///< Channels on the wire, e.g. "GRB" or "RGBW".
};

using ws2812_rmt::Variant;

/// Every document of docs/datasheets.yml that gives wire timing (units: ns).
/// Printed pages are in docs/.
inline constexpr std::array<Document, 22> kDocuments{{
    {"ws2812",
     Variant::Ws2812,
     {200, 500},
     {650, 950},
     {550, 850},
     {450, 750},
     650,
     1850,
     50000,
     0,
     "GRB"},
    {"ws2812s",
     Variant::Ws2812bPreV5,
     {250, 550},
     {700, 1000},
     {700, 1000},
     {250, 550},
     950,
     1550,
     50000,
     0,
     "GRB"},
    {"b-af",
     Variant::Ws2812bPreV5,
     {250, 550},
     {700, 1000},
     {650, 950},
     {300, 600},
     650,
     1850,
     50000,
     0,
     "GRB"},
    {"b-cn",
     Variant::Ws2812bPreV5,
     {250, 550},
     {700, 1000},
     {700, 1000},
     {250, 550},
     650,
     1850,
     50000,
     0,
     "GRB"},
    {"b-mar17",
     Variant::Ws2812bPreV5,
     {220, 380},
     {750, 1600},
     {750, 1600},
     {220, 420},
     0,
     0,
     300000,
     0,
     "GRB"},
    {"b-v5",
     Variant::Ws2812bV5,
     {220, 380},
     {580, 1000},
     {580, 1000},
     {580, 1000},
     0,
     0,
     280000,
     0,
     "GRB"},
    {"b-v5-w",
     Variant::Ws2812bV5,
     {220, 380},
     {580, 1000},
     {580, 1000},
     {580, 1000},
     0,
     0,
     280000,
     0,
     "GRB"},
    {"b-v6-h",
     Variant::Ws2812bV5,
     {220, 380},
     {580, 1000},
     {580, 1000},
     {580, 1000},
     0,
     0,
     280000,
     0,
     "GRB"},
    {"b-v6",
     Variant::Ws2812bV5,
     {220, 380},
     {580, 1000},
     {580, 1000},
     {580, 1000},
     1250,
     0,
     280000,
     0,
     "GRB"},
    {"b-2020",
     Variant::Ws2812bV5,
     {220, 380},
     {580, 1000},
     {580, 1000},
     {580, 1000},
     0,
     0,
     280000,
     0,
     "GRB"},
    {"b-1313",
     Variant::Ws2812bV5,
     {220, 380},
     {580, 1000},
     {580, 1000},
     {580, 1000},
     1250,
     0,
     280000,
     0,
     "GRB"},
    {"c-2020",
     Variant::Ws2812bV5,
     {220, 380},
     {580, 1000},
     {580, 1000},
     {580, 1000},
     0,
     0,
     280000,
     0,
     "GRB"},
    {"c-a",
     Variant::Ws2812c,
     {220, 380},
     {580, 1600},
     {580, 1600},
     {220, 420},
     0,
     0,
     280000,
     0,
     "GRB"},
    {"c-b",
     Variant::Ws2812c,
     {220, 380},
     {580, 1000},
     {580, 1000},
     {220, 420},
     0,
     0,
     280000,
     0,
     "GRB"},
    {"ws-2020",
     Variant::Ws2812c,
     {220, 380},
     {580, 1000},
     {580, 1000},
     {220, 420},
     0,
     0,
     280000,
     0,
     "GRB"},
    {"d-f8",
     Variant::Ws2812d,
     {220, 380},
     {750, 1000},
     {750, 1000},
     {220, 380},
     0,
     0,
     280000,
     0,
     "GRB"},
    {"sk-01",
     Variant::Sk6812,
     {150, 450},
     {750, 1050},
     {450, 750},
     {450, 750},
     650,
     1850,
     80000,
     50000, // Trst is 80 us in the table, but the sheet's own waveform figure says ">=50us"
     "GRB"},
    {"skw-01",
     Variant::Sk6812Rgbw,
     {150, 450},
     {750, 1050},
     {450, 750},
     {450, 750},
     650,
     1850,
     80000,
     0,
     "RGBW"},
    {"skw-03",
     Variant::Sk6812Rgbw,
     {150, 450},
     {750, 1050},
     {450, 750},
     {450, 750},
     650,
     1850,
     80000,
     0,
     "RGBW"},
    {"skw-07",
     Variant::Sk6812Rgbw,
     {200, 400},
     {800, 0},
     {580, 1000},
     {200, 0},
     1200,
     0,
     80000,
     20000,
     "GRBW"},
    {"skm-e",
     Variant::Sk6812Mini,
     {200, 400},
     {800, 0},
     {580, 1000},
     {200, 0},
     1200,
     0,
     80000,
     20000,
     "GRB"},
    {"skm",
     Variant::Sk6812Mini,
     {200, 400},
     {800, 0},
     {580, 1000},
     {200, 0},
     1200,
     0,
     80000,
     20000,
     "GRB"},
}};

/// Starts a strip the way an application does: `EnsureInitialized()`, then `PowerOn()` when the
/// port has the supply hook (a port with the hook starts unpowered, `Unsupported` means there is no
/// hook).
template <typename Strip>
[[nodiscard]] inline bool Start(Strip& strip) noexcept {
  if (!strip.EnsureInitialized().ok()) {
    return false;
  }
  const auto power = strip.PowerOn();
  return power.ok() || (power.error == ws2812_rmt::Error::Unsupported);
}

/// The document with this id (docs/datasheets.yml); the first row when there is none.
inline const Document& DocumentById(const char* id) noexcept {
  for (const Document& d : kDocuments) {
    if (std::strcmp(d.id, id) == 0) {
      return d;
    }
  }
  return kDocuments[0];
}

/// One half of an RMT symbol as the line sees it.
struct HalfSymbol {
  bool level;
  uint64_t ticks;
};

/// `ticks` at `hz` against a window, by cross multiplication (no rounding):
/// true when inside.
inline bool TicksInWin(uint64_t ticks, uint64_t hz, const Win& w, uint64_t cap_ns) noexcept {
  const uint64_t scaled = ticks * 1'000'000'000ULL;
  const bool above = scaled >= (static_cast<uint64_t>(w.min_ns) * hz);
  if (w.max_ns != 0U) {
    return above && (scaled <= (static_cast<uint64_t>(w.max_ns) * hz));
  }
  return above && ((cap_ns == 0U) || (scaled < (cap_ns * hz)));
}

/// Whether a waveform lies inside one document: windows, period rules, reset
/// strictly above the minimum.
inline bool WaveInDocument(const Document& d, const ws2812_rmt::Waveform& w) noexcept {
  const uint64_t hz = w.resolution_hz;
  const uint64_t cap = (d.max_low_ns != 0U) ? d.max_low_ns : d.reset_min_ns;
  const bool windows =
      TicksInWin(w.t0h_ticks, hz, d.t0h, 0) && TicksInWin(w.t0l_ticks, hz, d.t0l, cap) &&
      TicksInWin(w.t1h_ticks, hz, d.t1h, 0) && TicksInWin(w.t1l_ticks, hz, d.t1l, cap);
  const uint64_t p0 = (static_cast<uint64_t>(w.t0h_ticks) + w.t0l_ticks) * 1'000'000'000ULL;
  const uint64_t p1 = (static_cast<uint64_t>(w.t1h_ticks) + w.t1l_ticks) * 1'000'000'000ULL;
  const bool pmin = (p0 >= (static_cast<uint64_t>(d.period_min_ns) * hz)) &&
                    (p1 >= (static_cast<uint64_t>(d.period_min_ns) * hz));
  const bool pmax =
      (d.period_max_ns == 0U) || ((p0 <= (static_cast<uint64_t>(d.period_max_ns) * hz)) &&
                                  (p1 <= (static_cast<uint64_t>(d.period_max_ns) * hz)));
  const bool reset = (static_cast<uint64_t>(w.reset_ticks) * 1'000'000'000ULL) >
                     (static_cast<uint64_t>(d.reset_min_ns) * hz);
  return windows && pmin && pmax && reset;
}

/// A chain of pixels reading the symbols of one document. Strict: every
/// deviation is counted.
class PixelChainModel {
public:
  PixelChainModel(const Document& doc, size_t pixels) : doc_(doc), latched_(pixels) {}

  /// Feed the half-symbols of a transmission at `hz` ticks per second.
  void Consume(const std::vector<HalfSymbol>& symbols, uint64_t hz) {
    std::vector<HalfSymbol> merged;
    for (const HalfSymbol& s : symbols) {
      if (!merged.empty() && (merged.back().level == s.level)) {
        merged.back().ticks += s.ticks;
      } else {
        merged.push_back(s);
      }
    }
    if (!merged.empty() && !merged.front().level) {
      ++idle_violations; // a frame must start with a high
    }
    for (size_t i = 0; i < merged.size(); ++i) {
      if (!merged[i].level) {
        continue;
      }
      const bool has_low = (i + 1) < merged.size();
      handleBit(merged[i].ticks, has_low ? merged[i + 1].ticks : 0U, has_low, hz);
    }
    unlatched_bits = static_cast<int32_t>(bits_.size());
  }

  /// The latched colours after the last reset.
  [[nodiscard]] const std::vector<ws2812_rmt::Colour>& Latched() const noexcept {
    return latched_;
  }

  int32_t violations = 0;      ///< A high or low outside its window, or a period outside its rules.
  int32_t gaps_too_long = 0;   ///< A low above its window maximum inside a frame (an underrun).
  int32_t idle_violations = 0; ///< A frame that does not start with a high.
  int32_t latches = 0;         ///< Resets seen.
  int32_t wrong_size_frames = 0; ///< Latches with a bit count other than `pixels x bits`.
  int32_t unlatched_bits = 0;    ///< Bits received after the last reset.

private:
  void handleBit(uint64_t high, uint64_t low, bool has_low, uint64_t hz) {
    const uint64_t cap = (doc_.max_low_ns != 0U) ? doc_.max_low_ns : doc_.reset_min_ns;
    const bool in0 = TicksInWin(high, hz, doc_.t0h, 0);
    const bool in1 = TicksInWin(high, hz, doc_.t1h, 0);
    const bool latch =
        has_low && ((low * 1'000'000'000ULL) > (static_cast<uint64_t>(doc_.reset_min_ns) * hz));
    bool bit = false;
    if (in0 && in1) {
      bit = has_low && !latch && TicksInWin(low, hz, doc_.t1l, cap) &&
            !TicksInWin(low, hz, doc_.t0l, cap);
    } else if (in1) {
      bit = true;
    } else if (in0) {
      bit = false;
    } else {
      ++violations;
      bit = (high * 1'000'000'000ULL) >=
            (static_cast<uint64_t>(doc_.t0h.max_ns + doc_.t1h.min_ns) / 2U * hz);
    }
    bits_.push_back(bit);
    if (!latch && has_low) {
      checkLow(low, bit, high, hz, cap);
    }
    if (latch) {
      latchFrame();
    }
  }

  void checkLow(uint64_t low, bool bit, uint64_t high, uint64_t hz, uint64_t cap) {
    const Win& w = bit ? doc_.t1l : doc_.t0l;
    const uint64_t scaled = low * 1'000'000'000ULL;
    if (scaled < (static_cast<uint64_t>(w.min_ns) * hz)) {
      ++violations;
    } else if (!TicksInWin(low, hz, w, cap)) {
      ++gaps_too_long;
    } else {
      // inside the window
    }
    const uint64_t period = (high + low) * 1'000'000'000ULL;
    const bool short_period = period < (static_cast<uint64_t>(doc_.period_min_ns) * hz);
    const bool long_period =
        (doc_.period_max_ns != 0U) && (period > (static_cast<uint64_t>(doc_.period_max_ns) * hz));
    if (short_period || long_period) {
      ++violations;
    }
  }

  void latchFrame() {
    ++latches;
    const size_t width = std::strlen(doc_.order);
    const size_t per_pixel = width * 8U;
    if (bits_.size() != (latched_.size() * per_pixel)) {
      ++wrong_size_frames;
    }
    for (size_t k = 0; (k < latched_.size()) && (((k + 1U) * per_pixel) <= bits_.size()); ++k) {
      ws2812_rmt::Colour c{};
      for (size_t ch = 0; ch < width; ++ch) {
        uint8_t v = 0;
        for (size_t b = 0; b < 8U; ++b) {
          v = static_cast<uint8_t>((static_cast<unsigned>(v) << 1U) |
                                   (bits_[(k * per_pixel) + (ch * 8U) + b] ? 1U : 0U));
        }
        switch (ChannelOfLetter(doc_.order[ch])) {
        case ws2812_rmt::Channel::R:
          c.r = v;
          break;
        case ws2812_rmt::Channel::G:
          c.g = v;
          break;
        case ws2812_rmt::Channel::B:
          c.b = v;
          break;
        case ws2812_rmt::Channel::W:
        default:
          c.w = v;
          break;
        }
      }
      latched_[k] = c;
    }
    bits_.clear();
  }

  const Document& doc_;
  std::vector<ws2812_rmt::Colour> latched_;
  std::vector<bool> bits_;
};

/// The port behaviour shared by the mock ports: the RMT bytes encoder in
/// software, with fault knobs.
template <typename Self>
class FakeRmtCore : public ws2812_rmt::GpioInterface<Self> {
public:
  // Knobs
  // ---------------------------------------------------------------------------------------
  uint32_t resolution_hz = 20'000'000; ///< What `ResolutionHz()` reports.
  int32_t skew_ppm = 0;         ///< The real tick rate is `resolution_hz` x (1 + skew_ppm / 1e6).
  uint32_t jitter_ticks = 0;    ///< Each half-symbol except the reset moves by up to +-this.
  bool fail_init = false;       ///< `EnsureInitialized()` returns false.
  bool fail_resolution = false; ///< `ResolutionHz()` returns false.
  int32_t fail_transmit_at = 0; ///< The n-th `Transmit()` (1-based) returns false; 0 never.
  bool timeout_on_transmit = false; ///< `Transmit()` returns false, nothing sent (a timeout).
  int32_t cut_transmit_at = 0; ///< The n-th `Transmit()` (1-based) sends `cut_after_bits` bits,
                               ///< then returns false with no reset (a frame cut mid-way); 0 never.
  int32_t cut_after_bits = 0;  ///< Bits a cut frame delivers before it stops.
  int32_t underrun_after_byte = -1; ///< After this byte the low is stretched by
                                    ///< `underrun_gap_ticks`; -1 never.
  uint32_t underrun_gap_ticks = 0;  ///< The stretch.
  // Records
  // -------------------------------------------------------------------------------------
  int32_t init_calls = 0;
  int32_t resolution_calls = 0;
  int32_t transmit_calls = 0;
  uint64_t delayed_us = 0; ///< Sum of `DelayUs()` arguments.
  uint32_t last_timeout_ms = 0;
  ws2812_rmt::Waveform last_wave{};
  std::vector<uint8_t> last_wire;
  std::vector<HalfSymbol> symbols; ///< Everything sent, in order, across calls.

  bool EnsureInitialized() noexcept {
    ++init_calls;
    return !fail_init;
  }
  bool ResolutionHz(uint32_t& hz) noexcept {
    ++resolution_calls;
    if (fail_resolution) {
      return false;
    }
    hz = resolution_hz;
    return true;
  }
  bool Transmit(std::span<const uint8_t> wire, const ws2812_rmt::Waveform& wave,
                uint32_t timeout_ms) noexcept {
    ++transmit_calls;
    last_timeout_ms = timeout_ms;
    last_wave = wave;
    last_wire.assign(wire.begin(), wire.end());
    if (timeout_on_transmit || (transmit_calls == fail_transmit_at)) {
      return false;
    }
    int32_t byte_index = 0;
    const bool cut = transmit_calls == cut_transmit_at;
    int32_t bits_left = cut ? cut_after_bits : -1;
    for (const uint8_t byte : wire) {
      for (uint32_t bit = 0; bit < 8U; ++bit) {
        if (bits_left == 0) {
          return false; // cut: the line stays at the low of the last bit, no reset
        }
        if (bits_left > 0) {
          --bits_left;
        }
        const bool one = ((static_cast<uint32_t>(byte) >> (7U - bit)) & 1U) != 0U;
        uint64_t low = jittered(one ? wave.t1l_ticks : wave.t0l_ticks);
        if ((bit == 7U) && (byte_index == underrun_after_byte)) {
          low += underrun_gap_ticks;
        }
        symbols.push_back({true, jittered(one ? wave.t1h_ticks : wave.t0h_ticks)});
        symbols.push_back({false, low});
      }
      ++byte_index;
    }
    uint64_t left = wave.reset_ticks;
    while (left > 0U) {
      const uint64_t chunk =
          left > ws2812_rmt::kRmtHalfSymbolMaxTicks ? ws2812_rmt::kRmtHalfSymbolMaxTicks : left;
      symbols.push_back({false, chunk});
      left -= chunk;
    }
    return true;
  }
  /// A wait with the line low: it shows in the symbol list as a low of that length.
  void DelayUs(uint32_t us) noexcept {
    delayed_us += us;
    if (!symbols.empty()) {
      symbols.push_back({false, (static_cast<uint64_t>(us) * EffectiveHz()) / 1'000'000U});
    }
  }
  /// The tick rate the line really runs at, for the chain model.
  [[nodiscard]] uint64_t EffectiveHz() const noexcept {
    return static_cast<uint64_t>(static_cast<int64_t>(resolution_hz) +
                                 ((static_cast<int64_t>(resolution_hz) * skew_ppm) / 1'000'000));
  }
  /// Forget what was sent.
  void ClearSymbols() {
    symbols.clear();
  }

private:
  uint64_t jittered(uint32_t ticks) noexcept {
    if (jitter_ticks == 0U) {
      return ticks;
    }
    seed_ = (seed_ * 1103515245U) + 12345U;
    const int64_t delta =
        static_cast<int64_t>((seed_ >> 16U) % ((2U * jitter_ticks) + 1U)) - jitter_ticks;
    const int64_t v = static_cast<int64_t>(ticks) + delta;
    return v < 1 ? 1U : static_cast<uint64_t>(v);
  }
  uint32_t seed_ = 12345U;
};

/// Only the three required hooks.
class MinimalPort : public FakeRmtCore<MinimalPort> {};

/// Every optional hook, with counters.
class FullPort : public FakeRmtCore<FullPort> {
public:
  bool power_ok = true;    ///< `GpioSet()` result.
  int32_t power_calls = 0; ///< `GpioSet()` calls.
  bool powered = false;    ///< The last level set.
  int32_t log_lines = 0;   ///< `Log()` calls.

  bool GpioSet(ws2812_rmt::CtrlPin pin, ws2812_rmt::GpioSignal signal) noexcept {
    ++power_calls;
    if (!power_ok || (pin != ws2812_rmt::CtrlPin::Power)) {
      return false;
    }
    powered = signal == ws2812_rmt::GpioSignal::ACTIVE;
    return true;
  }
  void Log(ws2812_rmt::LogLevel level, const char* message) noexcept {
    static_cast<void>(level);
    static_cast<void>(message);
    ++log_lines;
  }
};

} // namespace ws2812_rmt_test

#endif // WS2812_RMT_MOCK_WS2812_RMT_HPP
