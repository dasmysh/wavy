/**
 * @file   main.cpp
 * @author Sebastian Maisch <Sebastian Maisch_EMAIL>
 * @date   2023.10.06
 *
 * @brief  Implements the applications entry point for windows.
 */

#include "main.h"
#include "app_constants.h"
#include "core/spdlog/sinks/filesink.h"
#include "sph/sph.h"

#include <SFML/Graphics.hpp>
#include <imgui-SFML.h>
#include <imgui.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <iostream>


int main(int /* argc */, const char** /* argv */) // NOLINT(bugprone-exception-escape)
{
    try {
        constexpr std::string_view directory; // = "";
        constexpr std::string_view name = wavy::logFileName;

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::warn);
        console_sink->set_pattern(fmt::format("[{}] [%^%l%$] %v", wavy::logTag));

        auto devenv_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
        devenv_sink->set_level(spdlog::level::err);
        devenv_sink->set_pattern(fmt::format("[{}] [%^%l%$] %v", wavy::logTag));

        std::shared_ptr<spdlog::sinks::base_sink<std::mutex>> file_sink;
        if constexpr (wavy::debug_build) {
            file_sink = std::make_shared<mysh::core::spdlog::sinks::rotating_open_file_sink_mt>(
                directory.empty() ? std::string{name} : std::string{directory}.append("/").append(name), 5);
            file_sink->set_level(spdlog::level::trace);
        } else {
            file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                directory.empty() ? std::string{name} : std::string{directory}.append("/").append(name), 5);
            file_sink->set_level(spdlog::level::trace);
        }

        const spdlog::sinks_init_list sink_list = {file_sink, console_sink, devenv_sink};
        auto logger = std::make_shared<spdlog::logger>(wavy::logTag.data(), sink_list.begin(), sink_list.end());

        spdlog::set_default_logger(logger);
        spdlog::flush_on(spdlog::level::err);

        if constexpr (wavy::debug_build) {
            spdlog::set_level(spdlog::level::trace);
        } else {
            spdlog::set_level(spdlog::level::err);
        }

        spdlog::info("Log created.");

    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl; // NOLINT(performance-avoid-endl)
        return 0;
    }

    constexpr unsigned int window_width = 1920;
    constexpr unsigned int window_height = 1080;

    spdlog::info("Creating SFML window.");

    sf::ContextSettings settings;
    settings.antialiasingLevel = 8;
    sf::RenderWindow window(sf::VideoMode(window_width, window_height), "wavy", sf::Style::Default, settings);
    window.setFramerateLimit(60);
    ImGui::SFML::Init(window);
    sf::Clock deltaClock;

    spdlog::debug("Starting main loop.");
    wavy::sph::sph sph_manager{glm::vec2{ 1920.0f, 1080.0f }};
    while (window.isOpen()) {
        // check all the window's events that were triggered since the last iteration of the loop
        sf::Event event;
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(window, event);

            // "close requested" event: we close the window
            if (event.type == sf::Event::Closed) { window.close(); }
            if (event.type == sf::Event::Resized) {
                // update the view to the new size of the window
                sf::FloatRect visibleArea(0.f, 0.f, static_cast<float>(event.size.width), static_cast<float>(event.size.height));
                window.setView(sf::View(visibleArea));
            }
        }

        ImGui::SFML::Update(window, deltaClock.restart());

        // ImGui::ShowDemoWindow();

        // clear the window with black color
        window.clear(sf::Color::Black);
        sph_manager.draw(window);
        ImGui::SFML::Render(window);

        // end the current frame
        window.display();
    }

    spdlog::debug("Main loop ended.");

    return 0;
}
