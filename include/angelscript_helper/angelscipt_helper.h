/**
 * @file   angelscipt_helper.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.03.10
 *
 * @brief  Helper class and functions for angelscript.
 */


#include <angelscript.h>
#include <filesystem>

#pragma once

#define CHECK_AS_HANDLE_ERR(as_call, r) \
    std::cerr << "Error during angel script call: " << r << "\n\tcall: " << #as_call << '\n'

#define CHECK_AS_CALL(as_call) \
    if (int r = as_call; r < 0) CHECK_AS_HANDLE_ERR(as_call, r)

#define CHECK_AS_CTX_CALL(as_call, ctx)                                                                        \
    if (int r = as_call; r < 0) {                                                                             \
        CHECK_AS_HANDLE_ERR(as_call, r);                                                                       \
        if (r == asEXECUTION_EXCEPTION) { std::cerr << "\tException: " << ctx->GetExceptionString() << '\n'; } \
    }

namespace wavy::utils {
    class angelscript_helper
    {
    public:
        angelscript_helper();
        ~angelscript_helper() noexcept;

        template<typename Pred> void setup_types(Pred setup_fn);
        void execute_as_script_file(const std::filesystem::path& filename, const std::string& as_module_name);

    private:
        asIScriptEngine* m_as_engine = nullptr;
    };

    template<typename Pred> inline void angelscript_helper::setup_types(Pred setup_fn)
    {
        setup_fn(m_as_engine);
    }
}
