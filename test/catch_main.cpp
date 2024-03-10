#include <catch.hpp>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <iostream>
#include <string_view>

int main(int argc, char* argv[])
{
    bool init_log = true;
    for (int i = 0; i < argc; ++i) {
        if (argv[i] == std::string{"--list-tests"}) { init_log = false; }
    }

    if (init_log) {
        try {
            constexpr std::string_view directory; // = "";
            constexpr std::string_view name = "tests.log";

            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::warn);
            console_sink->set_pattern(fmt::format("[{}] [%^%l%$] %v", "test"));

            auto devenv_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
            devenv_sink->set_level(spdlog::level::err);
            devenv_sink->set_pattern(fmt::format("[{}] [%^%l%$] %v", "test"));

            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                directory.empty() ? std::string{name} : std::string{directory}.append("/").append(name), 5);
            file_sink->set_level(spdlog::level::trace);

            const spdlog::sinks_init_list sink_list = {file_sink, console_sink, devenv_sink};
            auto logger = std::make_shared<spdlog::logger>("test", sink_list.begin(), sink_list.end());

            spdlog::set_default_logger(logger);
            spdlog::flush_on(spdlog::level::err);

            spdlog::set_level(spdlog::level::trace);

            spdlog::info("Log created.");
        } catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "Log initialization failed: " << ex.what() << std::endl; // NOLINT(performance-avoid-endl)
            return 0;
        }
    }

    int result = Catch::Session().run(argc, argv);

    return result;
}
