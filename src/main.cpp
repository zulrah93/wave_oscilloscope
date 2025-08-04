#include <SFML/Graphics.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <future>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <vector>
#include <wave_t.hpp>

using namespace std::chrono_literals;

constexpr size_t WIDTH = 2048;
constexpr size_t HEIGHT = 1024;
constexpr size_t MAX_RENDER_SAMPLE_LIMIT = 4000000;
constexpr auto FONT_FILE = "/GohuFontuni14NerdFont-Regular.ttf";

struct fundamental_frequency_future_result { // Yes its really long :p
  std::unique_ptr<std::vector<complex_t>> frequency_domain;
  float fundamental_frequency;
};

int main(int argument_count, char **arguments) {
  if (argument_count < 2) {
    std::cout
        << "This program needs a wav file as input to display PCM signal..."
        << std::endl;
    return 1;
  }

  const auto wave_file_path = arguments[1];

  wave_file_t wave_file(wave_file_path);

  if (!wave_file) {
    std::cout << "Cannot open '" << wave_file_path
              << "' its not a valid wav file!!" << std::endl;
    return 1;
  }

  std::future<fundamental_frequency_future_result>
      fundamental_frequency_future =
          std::async(std::launch::async, [&wave_file](void) {
            const auto &wav_header = wave_file.get_header();
            const size_t sample_rate = wav_header.sample_rate;
            const size_t dft_sample_size = sample_rate / 2;
            constexpr bool async = true; // Set this to false if you dare
            auto frequency_domain_result =
                wave_file.get_frequency_domain(dft_sample_size, async);
            fundamental_frequency_future_result result;
            result.frequency_domain = std::make_unique<std::vector<complex_t>>(
                std::move(frequency_domain_result));
            result.fundamental_frequency = 0.0f;
            double max = std::numeric_limits<float>::min();
            for (size_t frequency = 0;
                 frequency < result.frequency_domain->size(); frequency++) {
              float magnitude =
                  result.frequency_domain->at(frequency).magnitude;
              if (magnitude >= max) {
                result.fundamental_frequency = static_cast<float>(frequency);
                max = magnitude;
              }
            }
            result.fundamental_frequency *=
                (static_cast<float>(sample_rate) /
                 (static_cast<float>(dft_sample_size)));

            return result;
          });

  const auto &wav_header = wave_file.get_header();
  const auto wave_header_readable_string = wave_file.get_readable_wave_header();
  float max_sample_float_value = (wav_header.bits_per_sample == 8)
                                     ? static_cast<float>(INT8_MAX - 1)
                                     : 0.0f;

  if (wav_header.bits_per_sample == 16) {
    max_sample_float_value = static_cast<float>(INT16_MAX - 1);
  } else {
    max_sample_float_value = pow(2.0f, 24.0f) - 1.0f; // 2^24 - 1
  }

  if (max_sample_float_value <= std::numeric_limits<float>::epsilon()) {
    std::cout << "Unknown bitrate!! Cannot interpret!!" << std::endl;
    return 1;
  }

  std::vector<float> normalized_samples; // Normalized PCM samples to 0.0-1.0

  std::cout << "max_sample_float_value=" << max_sample_float_value << std::endl;

  normalized_samples.reserve(wave_file.sample_size());

  for (size_t index = 0; index < wave_file.sample_size(); index++) {
    float sample_float = static_cast<float>(wave_file[index].value_or(0.0f));
    normalized_samples.push_back(1.0f -
                                 (sample_float / max_sample_float_value));
  }

  std::stringstream title_window_sstream;

  title_window_sstream << "Wav Oscilloscope [" << wave_file_path << "]";

  sf::Font font_handle;
  if (!font_handle.loadFromFile(std::filesystem::current_path().string() +
                                FONT_FILE)) {
    std::cout << "Failed to find font (" << FONT_FILE
              << ") needed to run program!!" << std::endl;
    return 1;
  }

  sf::RenderWindow window(sf::VideoMode({WIDTH, HEIGHT}),
                          title_window_sstream.str());

  bool ready = (std::future_status::ready ==
                fundamental_frequency_future.wait_for(3ms));

  std::optional<fundamental_frequency_future_result> dft_result{std::nullopt};
  if (ready) {
    dft_result = fundamental_frequency_future.get();
  }

  bool display_time_domain = true;

  while (window.isOpen()) {
    sf::Event event;
    while (window.pollEvent(event)) {
      if (event.type == sf::Event::Closed) {
        window.close();
      }
    }

    window.clear(sf::Color::Black);

    sf::Text wave_header_info_text;
    wave_header_info_text.setFont(font_handle);
    wave_header_info_text.setString(wave_header_readable_string.c_str());
    wave_header_info_text.setCharacterSize(24);
    wave_header_info_text.setFillColor(sf::Color::Green);
    wave_header_info_text.setStyle(sf::Text::Bold);

    window.draw(wave_header_info_text);

    if (dft_result.has_value()) {
      sf::Text dft_info_text;
      dft_info_text.setFont(font_handle);
      dft_info_text.setString(
          std::format("Frequency: {} Hz\nF12: {}",
                      dft_result->fundamental_frequency,
                      (!display_time_domain ? "Time Domain" : "Freq Domain"))
              .c_str());
      dft_info_text.setCharacterSize(24);
      dft_info_text.setFillColor(sf::Color::Green);
      dft_info_text.setStyle(sf::Text::Bold);
      dft_info_text.setPosition(static_cast<float>(WIDTH) - 300.0f, 0.0f);
      window.draw(dft_info_text);
    }

    size_t sample_count = 0;
    const float delta_x =
        static_cast<float>(WIDTH) /
        static_cast<float>((wave_file.sample_size() < MAX_RENDER_SAMPLE_LIMIT)
                               ? wave_file.sample_size()
                               : MAX_RENDER_SAMPLE_LIMIT);
    for (float x = 0; x < WIDTH; x += delta_x) {
      if (normalized_samples[x] <= std::numeric_limits<float>::epsilon()) {
        continue;
      }
      sf::Vertex line[] = {
          sf::Vertex(sf::Vector2f(x, (normalized_samples[x] *
                                      static_cast<float>(HEIGHT - 1))),
                     sf::Color::Green),
          sf::Vertex(sf::Vector2f(x, static_cast<float>(HEIGHT - 1)),
                     sf::Color::Green)};

      window.draw(line, 2, sf::Lines);

      if (sample_count++ >= MAX_RENDER_SAMPLE_LIMIT) {
        break;
      }
    }

    if (ready) {
      window.display(); // We don't want to get the future result anymore.
      continue;
    }

    ready = (std::future_status::ready ==
             fundamental_frequency_future.wait_for(3ms));

    if (ready) {
      dft_result = fundamental_frequency_future.get();
    }
    window.display();
  }

  return 0;
}
