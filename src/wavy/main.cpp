/**
 * @file   main.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2023.10.06
 *
 * @brief  Implements the applications entry point for windows.
 */

#include "main.h"
#include "app_constants.h"
#include "core/spdlog/sinks/filesink.h"
#include "sph/sph.h"

#include <angelscipt_helper.h>
#include <SFML/Graphics.hpp>
#include <imgui-SFML.h>
#include <imgui.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <iostream>

namespace wavy::test::utils {
    namespace fs = std::filesystem;

    class config
    {
    public:
        const fs::path& get_log_directory() const { return m_log_directory; }
        void set_log_directory(const fs::path& log_directory) { m_log_directory = log_directory; }
        void set_log_directory(const std::string& log_directory) { m_log_directory = log_directory; }
        const fs::path& get_log_filename() const { return m_log_filename; }
        void set_log_filename(const fs::path& log_filename) { m_log_filename = log_filename; }
        void set_log_filename(const std::string& log_filename) { m_log_filename = log_filename; }
        const std::string& get_log_tag() const { return m_log_tag; }
        void set_log_tag(const std::string& log_tag) { m_log_tag = log_tag; }

        unsigned int get_window_width() const { return m_window_width; }
        void set_window_width(unsigned int window_width) { m_window_width = window_width; }
        unsigned int get_window_height() const { return m_window_height; }
        void set_window_height(unsigned int window_height) { m_window_height = window_height; }

        static void register_with_angelscript(asIScriptEngine* as_engine)
        {
            CHECK_AS_CALL(as_engine->RegisterObjectType("config", 0, asOBJ_REF | asOBJ_NOHANDLE));
            CHECK_AS_CALL(as_engine->RegisterObjectMethod(
                "config", "void set_log_directory(const string &in)",
                asMETHODPR(config, set_log_directory, (const std::string&), void), asCALL_THISCALL));
            CHECK_AS_CALL(as_engine->RegisterObjectMethod(
                "config", "void set_log_filename(const string &in)",
                asMETHODPR(config, set_log_filename, (const std::string&), void), asCALL_THISCALL));
            CHECK_AS_CALL(as_engine->RegisterObjectMethod("config", "void set_log_tag(const string &in)",
                                                          asMETHODPR(config, set_log_tag, (const std::string&), void),
                                                          asCALL_THISCALL));
            CHECK_AS_CALL(as_engine->RegisterObjectMethod("config", "void set_window_width(uint)",
                                                          asMETHODPR(config, set_window_width, (unsigned int), void),
                                                          asCALL_THISCALL));
            CHECK_AS_CALL(as_engine->RegisterObjectMethod("config", "void set_window_height(uint)",
                                                          asMETHODPR(config, set_window_height, (unsigned int), void),
                                                          asCALL_THISCALL));
        }

    private:
        fs::path m_log_directory;
        fs::path m_log_filename = "tests.log";
        std::string m_log_tag = "test";
        unsigned int m_window_width = 1920;
        unsigned int m_window_height = 1080;
    };
}

int main(int /* argc */, const char** /* argv */) // NOLINT(bugprone-exception-escape)
{
    wavy::test::utils::config conf;
    constexpr std::string_view config_file = "config.as";
    constexpr std::string_view user_config_file = "config.user.as";

    auto as_helper = std::make_unique<wavy::utils::angelscript_helper>();
    as_helper->setup_types([&conf](auto as_eng) {
        wavy::test::utils::config::register_with_angelscript(as_eng);
        CHECK_AS_CALL(as_eng->RegisterGlobalProperty("config cfg", &conf));
    });

    as_helper->execute_as_script_file(config_file, "cfg_module");
    as_helper->execute_as_script_file(user_config_file, "user_cfg_module");

    try {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::warn);
        console_sink->set_pattern(fmt::format("[{}] [%^%l%$] %v", conf.get_log_tag()));

        auto devenv_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
        devenv_sink->set_level(spdlog::level::err);
        devenv_sink->set_pattern(fmt::format("[{}] [%^%l%$] %v", conf.get_log_tag()));

        std::shared_ptr<spdlog::sinks::base_sink<std::mutex>> file_sink;
        if constexpr (wavy::debug_build) {
            file_sink = std::make_shared<mysh::core::spdlog::sinks::rotating_open_file_sink_mt>(
                (conf.get_log_directory().empty() ? conf.get_log_filename()
                                                  : conf.get_log_directory() / conf.get_log_filename())
                    .string(),
                true);
            file_sink->set_level(spdlog::level::trace);
        } else {
            file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                (conf.get_log_directory().empty() ? conf.get_log_filename()
                                                  : conf.get_log_directory() / conf.get_log_filename())
                    .string(),
                true);
            file_sink->set_level(spdlog::level::trace);
        }

        const spdlog::sinks_init_list sink_list = {file_sink, console_sink, devenv_sink};
        auto logger = std::make_shared<spdlog::logger>(conf.get_log_tag(), sink_list.begin(), sink_list.end());

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

    spdlog::info("Creating SFML window.");

    sf::ContextSettings settings;
    settings.antialiasingLevel = 8;
    sf::RenderWindow window(sf::VideoMode(conf.get_window_width(), conf.get_window_height()), "wavy",
                            sf::Style::Default, settings);
    window.setFramerateLimit(60);
    ImGui::SFML::Init(window);

    spdlog::debug("Starting main loop.");
    // wavy::sph::sph sph_manager{glm::vec2{1920.0f, 1080.0f}};
    wavy::sph::sph sph_manager{glm::vec2{16.f, 9.f}};

    sf::Clock delta_clock;
    sf::Clock poll_clock;
    while (window.isOpen()) {
        // check all the window's events that were triggered since the last iteration of the loop
        poll_clock.restart();
        sf::Event event;
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(window, event);

            // "close requested" event: we close the window
            if (event.type == sf::Event::Closed) { window.close(); }
            if (event.type == sf::Event::Resized) {
                // update the view to the new size of the window
                sf::FloatRect visibleArea(0.f, 0.f, static_cast<float>(event.size.width),
                                          static_cast<float>(event.size.height));
                window.setView(sf::View(visibleArea));
            } else {
                sph_manager.process_event(event);
            }
        }
        auto delta_t = delta_clock.restart();
        if (poll_clock.restart().asSeconds() > .5f) { continue; }

        ImGui::SFML::Update(window, delta_t);

        sph_manager.simulation_frame(delta_t.asSeconds());

        // ImGui::ShowDemoWindow();

        // clear the window with black color
        window.clear(sf::Color::Black);

        sph_manager.draw_simulation(window);
        sph_manager.draw_gui();

        ImGui::SFML::Render(window);

        // end the current frame
        window.display();
    }

    spdlog::debug("Main loop ended.");

    return 0;
}
