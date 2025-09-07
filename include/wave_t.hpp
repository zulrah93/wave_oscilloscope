#ifndef WAVE_T_HPP
#define WAVE_T_HPP

#include <bit>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <future>
#include <limits>
#include <numbers>
#include <optional>
#include <sstream>
#include <stdarg.h>
#include <string>
#include <vector>

constexpr auto RIFF_ASCII = std::byteswap(0x52494646);
constexpr auto WAVE_ASCII = std::byteswap(0x57415645);
constexpr auto FMT_ASCII = std::byteswap(0x666d7420);
constexpr auto DATA_ASCII = std::byteswap(0x64617461);
constexpr auto PCM = 1;
constexpr auto _8_BITS_PER_SAMPLE = 8;
constexpr auto _16_BITS_PER_SAMPLE = 16;
constexpr auto _24_BITS_PER_SAMPLE = 24;
constexpr auto DEFAULT_SUB_CHUNK_1_SIZE = 16;
constexpr auto DEFAULT_RESERVE_VALUE = 44100 * 60 * 5;

enum wave_type_t : uint8_t { invalid = 0, sine = 1, triangle = 2, square = 4, sawtooth = 8 };

// Provides functions that generate signals and dft and invesre dft function
namespace helper {

constexpr double set_volume(double percent) {
  if (percent > 1.0) {
    percent = 1.0;
  }
  if (percent < 0.0) {
    percent = 0.0;
  }
  return static_cast<double>(INT16_MAX - 1) * percent;
}

// Source: https://en.wikipedia.org/wiki/Sign_function
constexpr double sign(double x) {
  if (x > 0) {
    return 1.0;
  } else if (x < 0) {
    return -1.0;
  } else {
    return 0.0;
  }
}

// Source: https://en.wikipedia.org/wiki/Sine_wave
constexpr int16_t pcm_sine(double frequency, double time, double amplititude,
                           double phase) {
  return static_cast<int16_t>(
      amplititude * sin((2.0 * std::numbers::pi * frequency * time) + phase));
}

// Source: https://en.wikipedia.org/wiki/Triangle_wave
constexpr int16_t pcm_triangle(double time, double amplititude,
                               double frequency) {
  const double period = (1.0 / frequency);
  return static_cast<int16_t>(
      fabs(((2.0 * amplititude) / std::numbers::pi) *
           asin(sin(2.0 * time * (std::numbers::pi / period)))));
};

// Source: https://en.wikipedia.org/wiki/Square_wave_(waveform)
constexpr int16_t pcm_square(double time, double amplititude,
                             double frequency) {
  return static_cast<int16_t>(
      amplititude * sign(sin(2 * std::numbers::pi * frequency * time)));
}

// Source: https://en.wikipedia.org/wiki/Sawtooth_wave
constexpr int16_t pcm_saw_tooth(double time, double amplititude,
                                double frequency) {
  const double period = (1.0 / frequency);
  return static_cast<int16_t>(2.0 * amplititude *
                              ((time / period) - floor(0.5 + (time / period))));
}

void inverse_discrete_fourier_transform_async(
    size_t sample_size, std::vector<float> &time_domain,
    const std::vector<std::complex<float>> &frequency_domain) {
  std::vector<std::future<float>> futures;
  for (size_t frequency = 0; frequency < sample_size; frequency++) {
    auto future = std::async(std::launch::async, [&sample_size, frequency,
                                                  &frequency_domain]() {
      float real = 0.0f;
      for (size_t n = 0; n < sample_size; n++) {
        const float x = std::norm(frequency_domain[n]);
        float ratio = (static_cast<float>(n) / static_cast<float>(sample_size));
        float z =
            2.0 * std::numbers::pi * static_cast<float>(frequency) * ratio;
        real += x * cos(z);
      }
      return (real / static_cast<float>(sample_size));
    });
    futures.push_back(std::move(future));
  }
  for (auto &future : futures) {
    time_domain.push_back(future.get());
  }
}

void inverse_discrete_fourier_transform(
    size_t sample_size, std::vector<float> &time_domain,
    const std::vector<std::complex<float>> &frequency_domain) {
  for (size_t frequency = 0; frequency < sample_size; frequency++) {
    float real = 0.0f;
    for (size_t n = 0; n < sample_size; n++) {
      const float x = std::norm(frequency_domain[n]);
      float ratio = (static_cast<float>(n) / static_cast<float>(sample_size));
      float z = 2.0 * std::numbers::pi * static_cast<float>(frequency) * ratio;
      real += x * cos(z);
    }
    time_domain.push_back(real / static_cast<float>(sample_size));
  }
}

void discrete_fourier_transform(
    size_t sample_size, const std::vector<float> &time_domain,
    std::vector<std::complex<float>> &frequency_domain) {
  // O(n^2)
  for (size_t frequency = 0; frequency < sample_size; frequency++) {
    std::complex<float> result = 0.0f + 0.0if;
    for (size_t n = 0; n < sample_size; n++) {
      const float &x = time_domain[n];
      float ratio = (static_cast<float>(n) / static_cast<float>(sample_size));
      float z = 2.0 * std::numbers::pi * static_cast<float>(frequency) * ratio;
      result += std::complex<float>((x * cos(z)), -(x * sin(z)));
    }
    frequency_domain.push_back(result);
  }
}

void discrete_fourier_transform_async(
    size_t sample_size, const std::vector<float> &time_domain,
    std::vector<std::complex<float>> &frequency_domain) {
  std::vector<std::future<std::complex<float>>> futures;
  for (size_t frequency = 0; frequency < sample_size; frequency++) {
    auto future = std::async(
        std::launch::async, [&sample_size, &time_domain, frequency]() {
          std::complex<float> result = 0.0f + 0.0if;
          for (size_t n = 0; n < sample_size; n++) {
            const float &x = time_domain[n];
            const float ratio =
                (static_cast<float>(n) / static_cast<float>(sample_size));
            const float z =
                2.0 * std::numbers::pi * static_cast<float>(frequency) * ratio;
            result += std::complex<float>((x * cos(z)), -(x * sin(z)));
          }
          return result;
        });
    futures.push_back(std::move(future));
  }
  for (auto &future : futures) {
    frequency_domain.push_back(future.get());
  }
}

} // namespace helper

struct wave_header_t {
  uint32_t chunk_id;
  uint32_t chunk_size;
  uint32_t format;
  uint32_t sub_chunk_1_id;
  uint32_t sub_chunk_1_size;
  uint16_t audio_format;
  uint16_t number_of_channels;
  uint32_t sample_rate;
  uint32_t byte_rate;
  uint16_t block_align;
  uint16_t bits_per_sample;
  uint32_t sub_chunk_2_id;
  uint32_t sub_chunk_2_size;
};

class wave_file_t {
public:
  enum oscillator_type_t : uint8_t { empty = 0, carrier = 1, modulation = 2 };
  struct oscillator_config_t {
    oscillator_type_t operator_type;
    uint8_t
        wave_type; // Can be a combination of multiple waves so this oscilator
                   // can be already a saw and sqaure for example
    double frequency;
    const oscillator_config_t
        *carrier_to_modulate; // Null if oscillator is carrier and not modulator
  };
  struct synth_config_t {
    oscillator_config_t oscillator_a;
    oscillator_config_t oscillator_b;
    oscillator_config_t oscillator_c;
    oscillator_config_t oscillator_d;
    bool empty(void) const {
      return oscillator_a.operator_type == oscillator_type_t::empty &&
             oscillator_b.operator_type == oscillator_type_t::empty &&
             oscillator_c.operator_type == oscillator_type_t::empty &&
             oscillator_d.operator_type == oscillator_type_t::empty;
    }
  };

  wave_file_t(void) {
    memset(&m_header, 0, sizeof(m_header));
    m_header.chunk_id = RIFF_ASCII;
    m_header.format = WAVE_ASCII;
    m_header.sub_chunk_1_id = FMT_ASCII;
    m_header.sub_chunk_2_id = DATA_ASCII;
    m_header.audio_format = PCM;
    m_header.sub_chunk_1_size = DEFAULT_SUB_CHUNK_1_SIZE;
    m_samples.reserve(DEFAULT_RESERVE_VALUE);
  }

  // Takes a frequency domain and constructs wave samples for playback --
  // supports async loading of samples or not
  explicit wave_file_t(const size_t sample_size,
                       const std::vector<std::complex<float>> &frequency_domain,
                       bool async = true)
      : wave_file_t() {
    std::vector<float> time_domain;
    if (sample_size > DEFAULT_RESERVE_VALUE) {
      m_samples.reserve(sample_size);
    }
    time_domain.reserve(sample_size);
    if (async) {
      helper::inverse_discrete_fourier_transform_async(sample_size, time_domain,
                                                       frequency_domain);
    } else {
      helper::inverse_discrete_fourier_transform(sample_size, time_domain,
                                                 frequency_domain);
    }
    for (auto &time_domain_sample : time_domain) {
      m_samples.push_back(time_domain_sample);
    }
  }

  wave_file_t(const std::string &wav_file_path) {
    memset(&m_header, 0, sizeof(m_header));
    auto wave_file_handle = fopen(wav_file_path.c_str(), "rb");
    if (wave_file_handle) {
      size_t bytes_read =
          fread(&m_header, sizeof(uint8_t), sizeof(m_header), wave_file_handle);
      if (bytes_read == sizeof(m_header)) {
        const size_t sample_size =
            m_header.sub_chunk_2_size / m_header.block_align;
        switch (m_header.bits_per_sample) {
        case _8_BITS_PER_SAMPLE: {
          int8_t *samples = new int8_t[sample_size];
          bytes_read =
              fread(samples, sizeof(int8_t), sample_size, wave_file_handle);
          if (bytes_read == sample_size) {
            for (size_t index = 0; index < sample_size; index++) {
              m_samples.push_back(static_cast<uint32_t>(samples[index]));
            }
          }
          delete[] samples;
          break;
        }
        case _16_BITS_PER_SAMPLE: {
          int16_t *samples = new int16_t[sample_size];
          bytes_read =
              fread(samples, sizeof(int16_t), sample_size, wave_file_handle);
          if (bytes_read <= (m_header.block_align * sample_size)) {
            for (size_t index = 0; index < sample_size; index++) {
              m_samples.push_back(static_cast<uint32_t>(samples[index]));
            }
          }
          delete[] samples;
          break;
        }
        case _24_BITS_PER_SAMPLE:
          break;
        default:
          break;
        }
      }
    }
  }

  operator bool() {
    return m_header.chunk_id == RIFF_ASCII && m_header.format == WAVE_ASCII &&
           m_header.sub_chunk_1_id == FMT_ASCII &&
           m_header.sub_chunk_2_id == DATA_ASCII &&
           (m_header.audio_format != 0) && (m_header.bits_per_sample > 0) &&
           (m_header.number_of_channels > 0) && (m_header.sample_rate > 0) &&
           (m_header.byte_rate > 0) && (m_header.chunk_size > 0) &&
           (m_header.sub_chunk_1_size == DEFAULT_SUB_CHUNK_1_SIZE) &&
           (m_header.sub_chunk_2_size > 0);
  }

  std::optional<uint32_t> operator[](size_t index) const {
    if (index < m_samples.size()) {
      return std::make_optional<uint32_t>(m_samples[index]);
    }
    return std::nullopt;
  }

  size_t sample_size(void) const { return m_samples.size(); }

  void set_sample_rate(const int32_t sample_rate) {
    m_header.sample_rate = sample_rate;
  }
  void set_number_of_channels(const int16_t channel_count) {
    m_header.number_of_channels = channel_count;
  }
  void set_bits_per_sample(const int16_t bits_per_sample) {
    m_header.bits_per_sample = bits_per_sample;
  }
  const wave_header_t &get_header(void) const { return m_header; }

  bool add_24_bits_sample(const int32_t sample, ...) {
    va_list args;
    if (m_header.number_of_channels > 1) {
      va_start(args, sample);
    }
    m_samples.push_back(sample);
    for (size_t _ = 1;
         m_header.number_of_channels > 1 && _ < m_header.number_of_channels;
         _++) {
      m_samples.push_back(va_arg(args, const int32_t));
    }
    if (m_header.number_of_channels > 1) {
      va_end(args);
    }
    return true;
  }

  bool add_16_bits_sample(const int16_t sample, ...) {
    va_list args;
    if (m_header.number_of_channels > 1) {
      va_start(args, sample);
    }
    m_samples.push_back(static_cast<int32_t>(sample));
    for (size_t _ = 1;
         m_header.number_of_channels > 1 && _ < m_header.number_of_channels;
         _++) {
      m_samples.push_back(static_cast<int32_t>(va_arg(args, const int16_t)));
    }
    if (m_header.number_of_channels > 1) {
      va_end(args);
    }
    return true;
  }

  bool add_8_bits_sample(const int8_t sample, ...) {
    va_list args;
    if (m_header.number_of_channels > 1) {
      va_start(args, sample);
    }
    m_samples.push_back(static_cast<int32_t>(sample));
    for (size_t _ = 1;
         m_header.number_of_channels > 1 && _ < m_header.number_of_channels;
         _++) {
      m_samples.push_back(static_cast<int32_t>(va_arg(args, const int8_t)));
    }
    if (m_header.number_of_channels > 1) {
      va_end(args);
    }
    return true;
  }

  bool generate_synth(size_t sample_size, double volume_percent,
                      wave_file_t::synth_config_t &configuration) {

    if (m_header.sample_rate == 0) {
      return false;
    }

    if (configuration.empty()) {
      return false;
    }

    const double volume = helper::set_volume(volume_percent);

    const bool is_stereo = m_header.number_of_channels == 2;

    std::vector<uint32_t> output;

    std::vector<const oscillator_config_t *> carrier_oscialltors{};

    uint8_t modulating_wave_type{};

    double* frequency = nullptr;
    double modulating_frequency{};

    if (configuration.oscillator_a.operator_type !=
        oscillator_type_t::modulation) {
      carrier_oscialltors.push_back(
          &configuration
               .oscillator_a); // NOTES: As long as they share the same lifetime
                               // this is safe but we should try to use an
                               // std::optional in the future
        frequency = &configuration.oscillator_a.frequency;
    } else {
      modulating_wave_type = configuration.oscillator_a.wave_type;
      modulating_frequency = configuration.oscillator_a.frequency;
    }

    if (configuration.oscillator_b.operator_type !=
        oscillator_type_t::modulation) {
      carrier_oscialltors.push_back(
          &configuration
               .oscillator_b); // NOTES: As long as they share the same lifetime
                               // this is safe but we should try to use an
                               // std::optional in the future
      if (nullptr == frequency) {
        frequency = &configuration.oscillator_b.frequency;
      }
    } else if (wave_type_t::invalid == modulating_wave_type) {
      modulating_wave_type = configuration.oscillator_b.wave_type;
      modulating_frequency = configuration.oscillator_b.frequency;
    }

    if (configuration.oscillator_c.operator_type !=
        oscillator_type_t::modulation) {
      carrier_oscialltors.push_back(
          &configuration
               .oscillator_c); // NOTES: As long as they share the same lifetime
                               // this is safe but we should try to use an
                               // std::optional in the future
      if (nullptr == frequency) {
        frequency = &configuration.oscillator_c.frequency;
      }
    } else if (wave_type_t::invalid == modulating_wave_type) {
      modulating_wave_type = configuration.oscillator_c.wave_type;
      modulating_frequency = configuration.oscillator_c.frequency;   
    }

    if (configuration.oscillator_d.operator_type !=
        oscillator_type_t::modulation) {
      carrier_oscialltors.push_back(
          &configuration
               .oscillator_d); // NOTES: As long as they share the same lifetime
                               // this is safe but we should try to use an
                               // std::optional in the future
      if (nullptr == frequency) {
        frequency = &configuration.oscillator_d.frequency;
      }
    } else if (wave_type_t::invalid == modulating_wave_type) {
      modulating_wave_type = configuration.oscillator_d.wave_type;
      modulating_frequency = configuration.oscillator_d.frequency;
    }

    if (nullptr == frequency) {
      return false;
    }

    if (carrier_oscialltors.empty()) {
      return false; // Something is wrong -- no carrier means no music :(
    }

    auto carrier_oscilltor = carrier_oscialltors.front();
    const auto& wave_type = carrier_oscilltor->wave_type;
    const double phase{};
    double time{};
    for (size_t _{}; _ < sample_size; _++) {
      int16_t sample{};
      double frequency_offset = 0.0; 
      if ((modulating_wave_type & wave_type_t::sine)) {
        frequency_offset += helper::pcm_sine(modulating_frequency, time, volume, phase);
      }
      if ((modulating_wave_type & wave_type_t::triangle)) {
        frequency_offset += helper::pcm_triangle(time, volume, modulating_frequency);
      }
      if ((modulating_wave_type & wave_type_t::square)) {
        frequency_offset += helper::pcm_square(time, volume, modulating_frequency);
      }
      if ((modulating_wave_type & wave_type_t::sawtooth)) {
        frequency_offset += helper::pcm_saw_tooth(time, volume, modulating_frequency);
      }

      frequency_offset /= volume;
      frequency_offset *= std::numbers::pi;
      
      if ((wave_type & wave_type_t::sine)) {
        sample += helper::pcm_sine(*frequency + frequency_offset, time, volume, phase);
      }
      if ((wave_type & wave_type_t::triangle)) {
        sample += helper::pcm_triangle(time, volume, *frequency + frequency_offset);
      }
      if ((wave_type & wave_type_t::square)) {
        sample += helper::pcm_square(time, volume, *frequency + frequency_offset);
      }
      if ((wave_type & wave_type_t::sawtooth)) {
        sample += helper::pcm_saw_tooth(time, volume, *frequency + frequency_offset);
      }
      

      switch (m_header.bits_per_sample) {
      case _8_BITS_PER_SAMPLE:
        !is_stereo ? add_8_bits_sample(sample)
                   : add_8_bits_sample(sample, sample);
        break;
      case _16_BITS_PER_SAMPLE:
        !is_stereo ? add_16_bits_sample(sample)
                   : add_16_bits_sample(sample, sample);
        break;
      case _24_BITS_PER_SAMPLE:
        !is_stereo ? add_24_bits_sample(sample)
                   : add_16_bits_sample(sample, sample);
        break;
      default:
        break;
      }

      time += (1.0 / static_cast<double>(m_header.sample_rate));
    }

    return true;
  }

  bool generate_wave(uint8_t wave_type, size_t sample_size, double frequency,
                     double volume_percent) {

    if (m_header.sample_rate == 0) {
      return false;
    }

    const bool is_stereo = m_header.number_of_channels == 2;
    const double phase{};
    double time{};

    size_t wave_count =
          static_cast<size_t>((wave_type & wave_type_t::sine) ==
                              (wave_type_t::sine)) +
          static_cast<size_t>((wave_type & wave_type_t::triangle) ==
                              (wave_type_t::triangle)) +
          static_cast<size_t>((wave_type & wave_type_t::square) ==
                              (wave_type_t::square)) +
          static_cast<size_t>((wave_type & wave_type_t::sawtooth) ==
                              (wave_type_t::sawtooth));

    const double volume = helper::set_volume(volume_percent / static_cast<double>(wave_count));

    for (size_t _ = 0ul; _ < sample_size; _++) {  
      int16_t sample{};
      if ((wave_type & wave_type_t::sine)) {
        sample += helper::pcm_sine(frequency, time, volume, phase);
      }
      if ((wave_type & wave_type_t::triangle)) {
        sample += helper::pcm_triangle(time, volume, frequency);
      }
      if ((wave_type & wave_type_t::square)) {
        sample += helper::pcm_square(time, volume, frequency);
      }
      if ((wave_type & wave_type_t::sawtooth)) {
        sample += helper::pcm_saw_tooth(time, volume, frequency);
      }
      switch (m_header.bits_per_sample) {
      case _8_BITS_PER_SAMPLE:
        !is_stereo ? add_8_bits_sample(sample)
                   : add_8_bits_sample(sample, sample);
        break;
      case _16_BITS_PER_SAMPLE:
        !is_stereo ? add_16_bits_sample(sample)
                   : add_16_bits_sample(sample, sample);
        break;
      case _24_BITS_PER_SAMPLE:
        !is_stereo ? add_24_bits_sample(sample)
                   : add_16_bits_sample(sample, sample);
        break;
      default:
        return false;
      }
      time += (1.0 / static_cast<double>(m_header.sample_rate));
    }

    return true;
  }

  bool save(const std::string &file_path) {
    m_header.sub_chunk_2_size =
        (m_samples.size() * m_header.number_of_channels *
         (m_header.bits_per_sample / 8));
    m_header.block_align =
        (m_header.number_of_channels * (m_header.bits_per_sample / 8));
    m_header.byte_rate = (m_header.sample_rate * m_header.number_of_channels *
                          (m_header.bits_per_sample / 8));
    m_header.chunk_size = 36 + m_header.sub_chunk_2_size;
    switch (m_header.bits_per_sample) {
    case _8_BITS_PER_SAMPLE: {
      return save_as_8_bits(file_path);
    }
    case _16_BITS_PER_SAMPLE: {
      return save_as_16_bits(file_path);
    }
    case _24_BITS_PER_SAMPLE: {
      return save_as_24_bits(file_path);
    }
    default: {
      return false;
    }
    }
  }

  std::vector<std::complex<float>> get_frequency_domain(size_t sample_size,
                                                        bool async) {
    std::vector<std::complex<float>> frequency_domain;
    std::vector<float> time_domain;
    time_domain.reserve(m_samples.size());
    for (auto &sample : m_samples) {
      time_domain.push_back(static_cast<double>(sample));
    }
    if (async) {
      helper::discrete_fourier_transform_async(sample_size, time_domain,
                                               frequency_domain);
    } else {
      helper::discrete_fourier_transform(sample_size, time_domain,
                                         frequency_domain);
    }
    return frequency_domain;
  }

#ifdef DEBUG

  std::string get_readable_wave_header(void) {
    std::stringstream header;
    header << "chunk_id=0x" << std::hex << std::byteswap(m_header.chunk_id)
           << std::endl;
    header << "chunk_size=" << std::dec << m_header.chunk_size << std::endl;
    header << "format=0x" << std::hex << std::byteswap(m_header.format)
           << std::endl;
    header << "sub_chunk_1_id=0x" << std::byteswap(m_header.sub_chunk_1_id)
           << std::endl;
    header << "sub_chunk_1_size=" << std::dec << m_header.sub_chunk_1_size
           << std::endl;
    header << "audio_format="
           << ((m_header.audio_format == 1) ? "PCM" : "Unknown") << std::endl;
    header << "number_of_channels=" << m_header.number_of_channels << std::endl;
    header << "sample_rate=" << m_header.sample_rate << std::endl;
    header << "byte_rate=" << m_header.byte_rate << std::endl;
    header << "block_align=" << m_header.block_align << std::endl;
    header << "bits_per_sample=" << m_header.bits_per_sample << std::endl;
    header << "sub_chunk_2_id=0x" << std::hex
           << std::byteswap(m_header.sub_chunk_2_id) << std::endl;
    header << "sub_chunk_2_size=" << std::dec << m_header.sub_chunk_2_size
           << std::endl;
    return header.str();
  }

#endif

private:
  bool save_as_8_bits(const std::string &file_path) {
    std::vector<int8_t> samples;
    samples.reserve(m_samples.size());
    for (auto &sample : m_samples) {
      samples.push_back(static_cast<int8_t>(sample));
    }

    auto wav_file_handle = fopen(file_path.c_str(), "wb");
    if (nullptr == wav_file_handle) {
      return false;
    }

    const size_t expected_bytes = sizeof(m_header) + samples.size();
    size_t written_bytes =
        fwrite(reinterpret_cast<int8_t *>(&m_header), sizeof(int8_t),
               sizeof(m_header), wav_file_handle);
    written_bytes +=
        fwrite(samples.data(), sizeof(int8_t), samples.size(), wav_file_handle);
    fclose(wav_file_handle);
    return written_bytes == expected_bytes;
  }

  bool save_as_16_bits(const std::string &file_path) {
    std::vector<int16_t> samples;
    samples.reserve(m_samples.size());
    for (auto &sample : m_samples) {
      samples.push_back(static_cast<int16_t>(sample));
    }

    auto wav_file_handle = fopen(file_path.c_str(), "wb");
    if (nullptr == wav_file_handle) {
      return false;
    }

    const size_t expected_bytes =
        sizeof(m_header) + (sizeof(int16_t) * samples.size());
    size_t written_bytes =
        fwrite(reinterpret_cast<int8_t *>(&m_header), sizeof(int8_t),
               sizeof(m_header), wav_file_handle);
    written_bytes += fwrite(samples.data(), sizeof(int16_t), samples.size(),
                            wav_file_handle);
    fclose(wav_file_handle);
    return written_bytes == expected_bytes;
  }

  bool save_as_24_bits(const std::string &file_path) { return false; }
  wave_header_t m_header;
  std::vector<int32_t> m_samples;
};

#endif
