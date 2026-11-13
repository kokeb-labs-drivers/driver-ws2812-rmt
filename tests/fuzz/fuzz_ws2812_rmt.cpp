/**
 * @file fuzz_ws2812_rmt.cpp
 * @brief Fuzz target: the driver over a transmit path whose rate, failures and
 * optional hooks the fuzzer controls.
 *
 * The fuzzer's bytes choose a `Config` (any variant, `Custom` with arbitrary
 * timing, order, brightness, gamma, pixel count, reset, settle, slack), a port
 * (any tick rate including 0 and 2^32-1, failing hooks, a timeout, with or
 * without the optional hooks) and a sequence of calls
 * (`SetPixel` with any index, `Fill`, `Clear`, `SetBrightness`, `SetGamma`,
 * `Show`, `TraceWire` with any length, `PowerOn`, `PowerOff`, `CurrentWaveform`,
 * `FrameTimeUs`), with a supply pin whose writes can fail.
 *
 * After every call the target checks the driver's promises (docs/contract.md)
 * and aborts, naming the line, when one breaks:
 *
 * - a failed call returns an `Error` the enum defines and this driver returns
 * (never `Timeout`, `DeviceIdMismatch`, `Crc`, `Busy` or `DeviceFault`), and
 * `T{}`;
 * - before `EnsureInitialized()` succeeds the port saw no `Transmit()`;
 * - `InvalidArgument` and `TimingUnreachable` mean nothing was transmitted, and
 * `InvalidArgument` left the pixels unchanged;
 * - every transmitted waveform lies inside the windows of the declared profile
 * (a named variant: every datasheet of the model; `Custom`: the declared
 * window) and its reset is above the minimum: a hostile tick rate can never
 * produce an illegal waveform;
 * - the wire has exactly `Length() x channels` bytes at every `Transmit()`,
 * equal to a second, independent encoding, and `DecodeWire()` returns the
 * scaled pixels when no gamma is set;
 * - after a failed `Transmit()` the pixels are intact, the port waits (the
 * line low) strictly longer than the reset before the next `Transmit()`, and
 * the next `Show()`, with the fault removed, succeeds;
 * - `Show()` and `TraceWire()` send nothing before `PowerOn()` succeeds and
 * after `PowerOff()`, even when the pin write of `PowerOff()` failed; a failed
 * pin write is `Bus`;
 * - `TraceWire()` sends exactly the bytes it was given, and refuses an empty
 * span or one above four bytes per pixel of capacity with `InvalidArgument`;
 * - the port sees no out-of-range access (ASan/UBSan builds), the driver uses
 * no heap, every log line is non-empty plain text.
 *
 * Built two ways (tests/fuzz/CMakeLists.txt): with libFuzzer (Clang,
 * `-DWS2812_RMT_BUILD_FUZZERS=ON`), and as a plain executable that replays the
 * corpus in tests/fuzz/corpus/ as a CTest on any compiler.
 */
#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <span>
#include <string_view>

#include "mock_ws2812_rmt.hpp"
#include "ws2812_rmt.hpp"

namespace {

using namespace ws2812_rmt;

constexpr size_t kCapacity = 16;

/// The fuzzer's bytes, read front to back; zeros once they run out.
class Input {
public:
  Input(const uint8_t* data, std::size_t size) noexcept : data_(data), size_(size) {}

  [[nodiscard]] bool Empty() const noexcept {
    return pos_ >= size_;
  }
  uint8_t Byte() noexcept {
    return pos_ < size_ ? data_[pos_++] : uint8_t{0};
  }
  uint16_t Word() noexcept {
    const auto high = Byte();
    return static_cast<uint16_t>((static_cast<uint32_t>(high) << 8U) | Byte());
  }
  uint32_t Dword() noexcept {
    const uint32_t high = Word();
    return (high << 16U) | Word();
  }

private:
  const uint8_t* data_;
  std::size_t size_;
  std::size_t pos_ = 0;
};

/// A broken promise: name the line and abort, so libFuzzer saves the input as a
/// crash.
void Check(bool promise, int32_t line = __builtin_LINE()) noexcept {
  if (!promise) {
    static_cast<void>(
        std::fprintf(stderr, "fuzz_ws2812_rmt: promise broken at line %" PRId32 "\n", line));
    std::abort();
  }
}

/// The model's transmit path, with the order of the line's events checked: after a failed
/// `Transmit()` (the pixels may hold part of a frame) the waits before the next `Transmit()` must
/// add up to strictly more than that next frame's reset, so the half frame latches first.
template <typename Self>
class FuzzLine : public ws2812_rmt_test::FakeRmtCore<Self> {
  using Core = ws2812_rmt_test::FakeRmtCore<Self>;

public:
  bool Transmit(std::span<const uint8_t> wire, const Waveform& wave, uint32_t timeout_ms) noexcept {
    if (owes_reset_) {
      // waited_us_ x hz > reset_ticks x 1e6: the low lasted longer than the reset.
      Check((waited_us_ * static_cast<uint64_t>(wave.resolution_hz)) >
            (static_cast<uint64_t>(wave.reset_ticks) * 1'000'000ULL));
    }
    const bool ok = Core::Transmit(wire, wave, timeout_ms);
    owes_reset_ = !ok;
    waited_us_ = 0U;
    return ok;
  }
  void DelayUs(uint32_t us) noexcept {
    Check(us <= kMaxSettleUs); // the settle (the reset after a failed frame is shorter)
    Core::DelayUs(us);
    waited_us_ = std::min(waited_us_ + us, kWaitCapUs);
  }

private:
  static constexpr uint64_t kWaitCapUs = 1'000'000'000ULL; // no overflow in the check above
  bool owes_reset_ = false;
  uint64_t waited_us_ = 0U;
};

/// The ports: with and without the optional hooks.
class FuzzMinimal : public FuzzLine<FuzzMinimal> {};

class FuzzFull : public FuzzLine<FuzzFull> {
public:
  bool power_ok = true; ///< The result of the supply pin writes (a fault knob).

  [[nodiscard]] bool GpioSet(CtrlPin pin, GpioSignal signal) const noexcept {
    Check(pin == CtrlPin::Power);
    static_cast<void>(signal);
    return power_ok;
  }
  void Log(LogLevel level, const char* message) noexcept {
    static_cast<void>(level);
    Check((message != nullptr) && !std::string_view(message).empty());
  }
};

/// The errors this driver returns; `Timeout`, `DeviceIdMismatch`, `Crc`, `Busy`
/// and `DeviceFault` never.
bool KnownError(Error e) noexcept {
  switch (e) {
  case Error::Ok:
  case Error::NotInitialized:
  case Error::InvalidArgument:
  case Error::Bus:
  case Error::Unsupported:
  case Error::TimingUnreachable:
  case Error::NotPowered:
    return true;
  case Error::Timeout:
  case Error::DeviceIdMismatch:
  case Error::Crc:
  case Error::Busy:
  case Error::DeviceFault:
    return false;
  default:
    break;
  }
  return false;
}

/// A timing from the fuzzer's bytes; often invalid on purpose.
Timing FuzzTiming(Input& in) noexcept {
  const auto range = [&in]() noexcept {
    const uint32_t min = static_cast<uint32_t>(in.Byte()) * 5U;
    return Range{min, (in.Byte() & 1U) != 0U ? min + (static_cast<uint32_t>(in.Byte()) * 8U) : 0U,
                 min + (static_cast<uint32_t>(in.Byte() & 0x3FU) * 4U)};
  };
  Timing t{};
  t.t0h = range();
  t.t0l = range();
  t.t1h = range();
  t.t1l = range();
  t.period_min_ns = static_cast<uint32_t>(in.Byte()) * 8U;
  t.period_max_ns =
      (in.Byte() & 1U) != 0U ? t.period_min_ns + (static_cast<uint32_t>(in.Byte()) * 16U) : 0U;
  t.reset_min_ns = static_cast<uint32_t>(in.Word()) * 4U;
  t.max_low_ns = (in.Byte() & 1U) != 0U ? static_cast<uint32_t>(in.Byte()) * 100U : 0U;
  return t;
}

Channel FuzzChannel(Input& in) noexcept {
  return static_cast<Channel>(in.Byte() % 5U); // 4 is not an enumerator: Validate must refuse it
}

/// A configuration from the fuzzer's bytes.
Config FuzzConfig(Input& in) noexcept {
  Config c = Config::Default();
  const uint8_t flags = in.Byte();
  c.variant = static_cast<Variant>(in.Byte() % 10U); // 9 is not an enumerator
  if ((flags & 0x01U) != 0U) {
    c.use_default_order = false;
    c.order.count = static_cast<uint8_t>(in.Byte() % 6U);
    for (auto& ch : c.order.ch) {
      ch = FuzzChannel(in);
    }
  }
  if (c.variant == Variant::Custom) {
    c.use_default_order = false;
    c.custom = FuzzTiming(in);
    if ((flags & 0x08U) != 0U) {
      c.custom = ProfileTiming(static_cast<Variant>(in.Byte() % 8U)); // a valid one, often
    }
    if ((flags & 0x10U) != 0U) {
      c.order = (in.Byte() & 1U) != 0U ? kOrderGrbw : kOrderGrb;
    }
  } else if ((flags & 0x20U) != 0U) {
    c.custom = FuzzTiming(in); // a custom timing on a named variant: refused
  } else {
    // a named variant keeps an all-zero custom timing
  }
  c.brightness = in.Byte();
  c.gamma = static_cast<Gamma>(in.Byte() % 4U); // 3 is not an enumerator
  if ((flags & 0x02U) != 0U) {
    c.pixel_count = in.Byte() % (kCapacity + 4U);
  }
  if ((flags & 0x04U) != 0U) {
    c.reset_us = static_cast<uint32_t>(in.Word()) % (kMaxResetUs + 200U);
  }
  if ((flags & 0x40U) != 0U) {
    c.power_on_settle_us = in.Dword() % (kMaxSettleUs + 100U);
    c.transmit_slack_ms = in.Dword() % (kMaxSlackMs + 100U);
  }
  return c;
}

/// The tick rate the port reports: a few likely values, the extremes, and
/// anything.
uint32_t FuzzRate(Input& in) noexcept {
  switch (in.Byte() % 8U) {
  case 0:
    return 20'000'000U;
  case 1:
    return 10'000'000U;
  case 2:
    return 0U;
  case 3:
    return 0xFFFF'FFFFU;
  case 4:
    return kMaxResolutionHz;
  case 5:
    return in.Dword();
  case 6:
    return static_cast<uint32_t>(in.Word()) * 1000U;
  default:
    break;
  }
  return 40'000'000U;
}

/// The expected wire byte of a channel value, written again here from the
/// formula (not the driver's code).
uint8_t Expected(uint8_t v, uint8_t brightness, Gamma gamma) noexcept {
  const uint32_t g = (gamma == Gamma::Gamma22) ? kGamma22[v] : v;
  return static_cast<uint8_t>(((2U * g * brightness) + 255U) / 510U);
}

/// Whether a waveform is legal for the declared timing, by the model's own
/// cross multiplication.
bool LegalFor(const Config& c, const Waveform& w) noexcept {
  if (c.variant == Variant::Custom) {
    const uint64_t hz = w.resolution_hz;
    const uint64_t cap = (c.custom.max_low_ns != 0U) ? c.custom.max_low_ns : c.custom.reset_min_ns;
    const auto win = [](const Range& r) { return ws2812_rmt_test::Win{r.min_ns, r.max_ns}; };
    const bool windows = ws2812_rmt_test::TicksInWin(w.t0h_ticks, hz, win(c.custom.t0h), 0) &&
                         ws2812_rmt_test::TicksInWin(w.t0l_ticks, hz, win(c.custom.t0l), cap) &&
                         ws2812_rmt_test::TicksInWin(w.t1h_ticks, hz, win(c.custom.t1h), 0) &&
                         ws2812_rmt_test::TicksInWin(w.t1l_ticks, hz, win(c.custom.t1l), cap);
    const uint64_t p0 = (static_cast<uint64_t>(w.t0h_ticks) + w.t0l_ticks) * 1'000'000'000ULL;
    const uint64_t p1 = (static_cast<uint64_t>(w.t1h_ticks) + w.t1l_ticks) * 1'000'000'000ULL;
    const bool periods = (p0 >= (static_cast<uint64_t>(c.custom.period_min_ns) * hz)) &&
                         (p1 >= (static_cast<uint64_t>(c.custom.period_min_ns) * hz)) &&
                         ((c.custom.period_max_ns == 0U) ||
                          ((p0 <= (static_cast<uint64_t>(c.custom.period_max_ns) * hz)) &&
                           (p1 <= (static_cast<uint64_t>(c.custom.period_max_ns) * hz))));
    const bool reset = (static_cast<uint64_t>(w.reset_ticks) * 1'000'000'000ULL) >
                       (static_cast<uint64_t>(c.custom.reset_min_ns) * hz);
    return windows && periods && reset;
  }
  bool any = false;
  for (const auto& d : ws2812_rmt_test::kDocuments) {
    if (d.variant == c.variant) {
      any = true;
      if (!ws2812_rmt_test::WaveInDocument(d, w)) {
        return false;
      }
    }
  }
  return any;
}

template <typename PortT>
class Session {
public:
  Session(Input& in, const Config& config) noexcept
      : in_(in), config_(config), gamma_(config.gamma), driver_(port_, config) {}

  void Run() noexcept {
    for (uint32_t calls = 0U; (calls < 48U) && !in_.Empty(); ++calls) {
      step();
    }
  }

private:
  using Driver = Ws2812Rmt<PortT, kCapacity>;

  [[nodiscard]] std::array<Colour, kCapacity> snapshot() const noexcept {
    std::array<Colour, kCapacity> s{};
    for (size_t i = 0; i < driver_.Length(); ++i) {
      s[i] = driver_.GetPixel(i).value;
    }
    return s;
  }

  void checkShown(bool ok) noexcept {
    if (!ok) {
      return;
    }
    const auto wave = driver_.CurrentWaveform();
    Check(wave.ok() && (port_.last_wave == wave.value));
    Check(LegalFor(config_, port_.last_wave));
    const size_t channels = driver_.ChannelsPerPixel();
    Check((channels == 3U) || (channels == 4U));
    Check(port_.last_wire.size() == (driver_.Length() * channels));
    const ColourOrder order = OrderOf(config_);
    for (size_t p = 0; p < driver_.Length(); ++p) {
      const Colour logical = driver_.GetPixel(p).value;
      for (size_t k = 0; k < channels; ++k) {
        const uint8_t expected =
            Expected(ChannelOf(logical, order.ch[k]), driver_.Brightness(), gamma_);
        Check(port_.last_wire[(p * channels) + k] == expected);
      }
    }
    if (gamma_ == Gamma::None) {
      std::array<Colour, kCapacity> back{};
      const size_t n = DecodeWire(port_.last_wire, order, back);
      Check(n == driver_.Length());
      if (driver_.Brightness() == 255U) {
        for (size_t p = 0; p < n; ++p) {
          const Colour logical = driver_.GetPixel(p).value;
          const bool carries_red = (order.count >= 3U) && hasChannel(order, Channel::R);
          Check(back[p].r == (carries_red ? logical.r : 0U));
        }
      }
    }
  }

  static bool hasChannel(const ColourOrder& order, Channel c) noexcept {
    for (size_t i = 0; i < static_cast<size_t>(order.count); ++i) {
      if (order.ch[i] == c) {
        return true;
      }
    }
    return false;
  }

  template <typename R>
  void checkCommon(const R& r, bool was_up, int32_t transmits_before) noexcept {
    Check(KnownError(r.error));
    if (!was_up && !driver_.IsInitialized()) {
      Check(port_.transmit_calls == transmits_before);
    }
    if ((r.error == Error::InvalidArgument) || (r.error == Error::TimingUnreachable) ||
        (r.error == Error::NotInitialized) || (r.error == Error::NotPowered)) {
      Check(port_.transmit_calls == transmits_before);
    }
  }

  void step() noexcept {
    const uint8_t op = in_.Byte() % 15U;
    const bool was_up = driver_.IsInitialized();
    const int32_t before = port_.transmit_calls;
    switch (op) {
    case 0:
      applyFaults();
      doInit(was_up, before);
      break;
    case 1:
    case 2:
      doSetPixel();
      break;
    case 3:
      driver_.Fill(Colour{in_.Byte(), in_.Byte(), in_.Byte(), in_.Byte()});
      break;
    case 4:
      driver_.Clear();
      break;
    case 5:
      driver_.SetBrightness(in_.Byte());
      break;
    case 6: {
      const auto g = static_cast<Gamma>(in_.Byte() % 4U);
      driver_.SetGamma(g);
      gamma_ = detail::IsEnumerator(g) ? g : Gamma::None;
      break;
    }
    case 7:
    case 8:
    case 9:
      doShow(was_up, before);
      break;
    case 10:
      doPower(true, was_up, before);
      break;
    case 11:
      doPower(false, was_up, before);
      break;
    case 12: {
      const auto w = driver_.CurrentWaveform();
      checkCommon(w, was_up, before);
      Check(w.ok() == was_up);
      Check(w.ok() || (w.value == Waveform{}));
      break;
    }
    case 13:
      doTraceWire(was_up, before);
      break;
    default: {
      const auto t = driver_.FrameTimeUs();
      checkCommon(t, was_up, before);
      Check(t.ok() == was_up);
      break;
    }
    }
  }

  /// The port's behaviour for the next call: sometimes a fault.
  void applyFaults() noexcept {
    const uint8_t b = in_.Byte();
    port_.resolution_hz = FuzzRate(in_);
    port_.fail_init = (b & 0x01U) != 0U;
    port_.fail_resolution = (b & 0x02U) != 0U;
    if constexpr (detail::HasGpioSet<PortT>) {
      port_.power_ok = (b & 0x04U) == 0U;
    }
  }

  void doInit(bool was_up, int32_t before) noexcept {
    const auto r = driver_.EnsureInitialized();
    checkCommon(r, was_up, before);
    Check(r.ok() == driver_.IsInitialized());
    if (was_up) {
      Check(r.ok());
    } else if (!Validate(config_, kCapacity).ok()) {
      Check(r.error == Error::InvalidArgument);
    } else {
      // a valid configuration: Ok, Bus or TimingUnreachable
    }
  }

  void doSetPixel() noexcept {
    const size_t index = in_.Word() % (kCapacity + 8U);
    const Colour c{in_.Byte(), in_.Byte(), in_.Byte(), in_.Byte()};
    const auto before = snapshot();
    const auto r = driver_.SetPixel(index, c);
    Check(KnownError(r.error));
    if (index >= driver_.Length()) {
      Check(r.error == Error::InvalidArgument);
      Check(before == snapshot());
    } else {
      Check(r.ok() && (driver_.GetPixel(index).value == c));
    }
  }

  void doShow(bool was_up, int32_t before) noexcept {
    const uint8_t b = in_.Byte();
    port_.fail_transmit_at = ((b & 0x01U) != 0U) ? (port_.transmit_calls + 1) : 0;
    port_.timeout_on_transmit = (b & 0x02U) != 0U;
    port_.ClearSymbols();
    const auto pixels = snapshot();
    const auto r = driver_.Show();
    checkCommon(r, was_up, before);
    Check(pixels == snapshot()); // Show never changes the stored colours
    if (!was_up) {
      Check(r.error == Error::NotInitialized);
    } else if (!powered_) {
      Check(r.error == Error::NotPowered); // refused after PowerOff(), nothing sent
    } else {
      Check(r.error != Error::NotPowered);
    }
    if (r.error == Error::Bus) {
      failed_show_ = true;
    }
    checkShown(r.ok());
    if (r.ok()) {
      Check(port_.transmit_calls == (before + 1));
    }
    // the fault removed, a good Show must succeed on an initialised driver
    port_.fail_transmit_at = 0;
    port_.timeout_on_transmit = false;
    if (failed_show_ && driver_.IsInitialized() && powered_) {
      const auto again = driver_.Show();
      Check(again.ok());
      checkShown(true);
      failed_show_ = false;
    }
    port_.ClearSymbols();
  }

  /// Raw bytes of any length, some above the limit.
  void doTraceWire(bool was_up, int32_t before) noexcept {
    const size_t n = in_.Byte() % ((kCapacity * 4U) + 3U);
    std::array<uint8_t, (kCapacity * 4U) + 2U> bytes{};
    for (size_t i = 0; i < n; ++i) {
      bytes[i] = in_.Byte();
    }
    port_.fail_transmit_at = ((in_.Byte() & 0x01U) != 0U) ? (port_.transmit_calls + 1) : 0;
    const auto pixels = snapshot();
    const auto r = driver_.TraceWire(std::span<const uint8_t>(bytes).first(n));
    port_.fail_transmit_at = 0;
    checkCommon(r, was_up, before);
    Check(pixels == snapshot()); // the raw bytes never touch the stored colours
    if (!was_up) {
      Check(r.error == Error::NotInitialized);
    } else if (!powered_) {
      Check(r.error == Error::NotPowered);
    } else if ((n == 0U) || (n > (kCapacity * 4U))) {
      Check(r.error == Error::InvalidArgument);
    } else {
      Check(r.ok() || (r.error == Error::Bus));
      Check(port_.transmit_calls == (before + 1));
      Check(std::equal(port_.last_wire.begin(), port_.last_wire.end(), bytes.begin(),
                       bytes.begin() + static_cast<std::ptrdiff_t>(n)));
      Check(LegalFor(config_, port_.last_wave));
    }
    if (r.error == Error::Bus) {
      failed_show_ = true;
    }
    port_.ClearSymbols();
  }

  void doPower(bool on, bool was_up, int32_t before) noexcept {
    const auto r = on ? driver_.PowerOn() : driver_.PowerOff();
    checkCommon(r, was_up, before);
    if constexpr (detail::HasGpioSet<PortT>) {
      if (was_up) {
        // PowerOff() refuses sending even when the pin write failed; PowerOn() only when it worked.
        powered_ = on ? (r.ok() || powered_) : false;
      }
    }
    Check(port_.transmit_calls == before);
    if constexpr (!detail::HasGpioSet<PortT>) {
      Check(r.error == Error::Unsupported);
    } else if (!was_up) {
      Check(r.error == Error::NotInitialized);
    } else {
      Check(r.ok() == port_.power_ok); // a failed pin write is Bus, a good one Ok
      Check(r.ok() || (r.error == Error::Bus));
    }
  }

  Input& in_;
  Config config_;
  Gamma gamma_;
  bool failed_show_{false};
  bool powered_{!detail::HasGpioSet<PortT>}; // a port with a supply hook starts unpowered
  PortT port_{};
  Driver driver_;
};

template <typename PortT>
void RunWith(Input& in, const Config& config) noexcept {
  auto session = std::make_unique<Session<PortT>>(in, config);
  session->Run();
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
  Input in(data, size);
  const Config config = FuzzConfig(in);
  if ((in.Byte() & 1U) != 0U) {
    RunWith<FuzzFull>(in, config);
  } else {
    RunWith<FuzzMinimal>(in, config);
  }
  return 0;
}
