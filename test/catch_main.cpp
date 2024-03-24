/**
 * @file   catch_main.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.03.10
 *
 * @brief  Custom main file for catch2 tests.
 */

#include <angelscipt_helper.h>
#include <catch.hpp>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <iostream>
#include <string_view>
#include <filesystem>

namespace wavy::test::utils
{
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
        }

    private:
        fs::path m_log_directory;
        fs::path m_log_filename = "tests.log";
        std::string m_log_tag = "test";
    };
}

int main(int argc, char* argv[])
{
    bool init_log = true;
    for (int i = 0; i < argc; ++i) {
        if (argv[i] == std::string{"--list-tests"}) { init_log = false; }
    }

    std::unique_ptr<wavy::utils::angelscript_helper> as_helper;
    if (init_log) {
        wavy::test::utils::config conf;
        constexpr std::string_view test_config_file = "test_config.as";
        constexpr std::string_view user_test_config_file = "test_config.user.as";

        as_helper = std::make_unique<wavy::utils::angelscript_helper>();
        as_helper->setup_types([&conf](auto as_eng) {
            wavy::test::utils::config::register_with_angelscript(as_eng);
            CHECK_AS_CALL(as_eng->RegisterGlobalProperty("config cfg", &conf));
        });

        as_helper->execute_as_script_file(test_config_file, "test_cfg_module");
        as_helper->execute_as_script_file(user_test_config_file, "test_user_cfg_module");

        try {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::warn);
            console_sink->set_pattern(fmt::format("[{}] [%^%l%$] [%t] %v", conf.get_log_tag()));

            auto devenv_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
            devenv_sink->set_level(spdlog::level::err);
            devenv_sink->set_pattern(fmt::format("[{}] [%^%l%$] [%t] %v", conf.get_log_tag()));

            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                (conf.get_log_directory().empty() ? conf.get_log_filename()
                                                 : conf.get_log_directory() / conf.get_log_filename()).string(),
                true);
            file_sink->set_level(spdlog::level::trace);
            file_sink->set_pattern(fmt::format("[%T %f] [{}] [%^%l%$] [%t] %v", conf.get_log_tag()));

            const spdlog::sinks_init_list sink_list = {file_sink, console_sink, devenv_sink};
            auto logger = std::make_shared<spdlog::logger>(conf.get_log_tag(), sink_list.begin(), sink_list.end());

            spdlog::set_default_logger(logger);
            spdlog::flush_on(spdlog::level::trace);

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
