/**
 * @file   angelscipt_helper.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.03.10
 *
 * @brief  Helper class and functions for angelscript.
 */

#include "angelscipt_helper.h"

#include <spdlog/spdlog.h>
#include <angelscript/scriptstdstring/scriptstdstring.h>
#include <angelscript/scriptbuilder/scriptbuilder.h>
#include <iostream>

#include <angelscript/scriptstdstring/scriptstdstring.cpp>
#include <angelscript/scriptbuilder/scriptbuilder.cpp>

namespace wavy::utils {
    void as_message_callback(const asSMessageInfo* msg, void*)
    {
        const char* type = "ERR ";
        if (msg->type == asMSGTYPE_WARNING)
            type = "WARN";
        else if (msg->type == asMSGTYPE_INFORMATION)
            type = "INFO";
        if (spdlog::default_logger()) {
            spdlog::error("{} ({}, {}) : {} : {}\n", msg->section, msg->row, msg->col, type, msg->message);
        } else {
            std::cerr << fmt::format("{} ({}, {}) : {} : {}\n", msg->section, msg->row, msg->col, type, msg->message);
        }
    }

    angelscript_helper::angelscript_helper()
        : m_as_engine{asCreateScriptEngine()}
    {
        CHECK_AS_CALL(m_as_engine->SetMessageCallback(asFUNCTION(as_message_callback), nullptr, asCALL_CDECL));
        RegisterStdString(m_as_engine);
    }

    angelscript_helper::~angelscript_helper()
    {
        m_as_engine->ShutDownAndRelease();
    }

    void angelscript_helper::execute_as_script_file(const std::filesystem::path& filename,
                                                    const std::string& as_module_name)
    {
        CScriptBuilder script_builder;
        CHECK_AS_CALL(script_builder.StartNewModule(m_as_engine, as_module_name.c_str()));
        auto as_module = script_builder.GetModule();

        CHECK_AS_CALL(script_builder.AddSectionFromFile(filename.string().c_str()));
        CHECK_AS_CALL(script_builder.BuildModule());

        auto func = as_module->GetFunctionByDecl("void main()");
        if (func == nullptr) {
            std::cout << "The script must have the function 'void main()'. Please add it and try again.\n";
            return;
        }

        auto ctx = m_as_engine->CreateContext();
        ctx->Prepare(func);
        CHECK_AS_CTX_CALL(ctx->Execute(), ctx);

        func->Release();
    }
}
