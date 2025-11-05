#include "LuaApi.h"

#include <sstream>
#include <stdexcept>
#include <chrono>

#include <fmt/format.h>
#include <fmt/args.h>

#include "Logger.h"
#include "TASEngine.h"
#include "TASProject.h"
#include "ScriptContext.h"
#include "GameInterface.h"

// Helper function to format strings with variadic arguments
std::string FormatString(const std::string &fmt, sol::variadic_args va) {
    // Fast path for empty format strings
    if (fmt.empty()) {
        return fmt;
    }

    // Fast path for format strings without placeholders
    if (fmt.find('{') == std::string::npos) {
        return fmt;
    }

    fmt::dynamic_format_arg_store<fmt::format_context> store;

    for (const auto &arg : va) {
        sol::type argType = arg.get_type();
        switch (argType) {
            case sol::type::lua_nil: {
                store.push_back("nil");
                break;
            }
            case sol::type::boolean: {
                bool value = arg.as<bool>();
                store.push_back(value ? "true" : "false");
                break;
            }
            case sol::type::number: {
                store.push_back(arg.as<double>());
                break;
            }
            case sol::type::string: {
                sol::string_view sv = arg.as<sol::string_view>();
                store.push_back(sv);
                break;
            }
            case sol::type::table: {
                sol::table table = arg.as<sol::table>();
                sol::table metatable = table[sol::metatable_key];

                // Check if table has a __tostring metamethod
                sol::optional<sol::function> tostring = metatable[sol::to_string(sol::meta_function::to_string)];
                if (tostring) {
                    try {
                        sol::protected_function_result result = tostring.value()(table);
                        if (result.valid()) {
                            store.push_back(result.get<std::string>());
                            break;
                        }
                    } catch (const std::exception &) {
                        // Fall through to default table representation
                    }
                }

                store.push_back("<table>");
                break;
            }
            case sol::type::userdata: {
                sol::userdata ud = arg.as<sol::userdata>();
                sol::table metatable = ud[sol::metatable_key];

                // Check if userdata has a __tostring metamethod
                auto tostring = metatable[sol::to_string(sol::meta_function::to_string)];
                if (tostring.valid() && tostring.get_type() == sol::type::function) {
                    try {
                        // CRITICAL FIX: Convert to protected_function before calling
                        sol::protected_function tostringFunc = tostring;
                        sol::protected_function_result res = tostringFunc(ud);
                        if (res.valid() && res.get_type() == sol::type::string) {
                            store.push_back(res.get<std::string>());
                            break;
                        }
                    } catch (const std::exception &) {
                        // Fall through to default userdata representation
                    }
                }

                store.push_back("<userdata>");
                break;
            }
            case sol::type::function: {
                store.push_back("<function>");
                break;
            }
            case sol::type::thread: {
                store.push_back("<thread>");
                break;
            }
            case sol::type::lightuserdata: {
                store.push_back("<lightuserdata>");
                break;
            }
            default: {
                store.push_back("<unknown>");
                break;
            }
        }
    }

    try {
        return fmt::vformat(fmt, store);
    } catch (const fmt::format_error &e) {
        // If formatting fails, return an error message
        std::stringstream stream;
        stream << "[Format Error: " << e.what() << "] " << fmt;
        return stream.str();
    } catch (const std::exception &e) {
        // Catch any other standard exceptions
        std::stringstream stream;
        stream << "[Exception: " << e.what() << "] " << fmt;
        return stream.str();
    } catch (...) {
        // Catch any unknown exceptions
        std::stringstream stream;
        stream << "[Unknown Error] " << fmt;
        return stream.str();
    }
}

// ===================================================================
//  Core & Flow Control API Registration
// ===================================================================

void LuaApi::RegisterCoreApi(sol::table &tas, ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::RegisterCoreApi requires a valid ScriptContext");
    }

    std::string logPrefix = "[" + context->GetName() + "]";

    // tas.log(format_string, ...)
    tas["log"] = [logPrefix](const std::string &fmt, sol::variadic_args va) {
        try {
            std::string text = FormatString(fmt, va);
            Log::Info("%s %s", logPrefix.c_str(), text.c_str());
        } catch (const std::exception &e) {
            Log::Error("%s Error in log: %s", logPrefix.c_str(), e.what());
        }
    };

    // tas.warn(format_string, ...)
    tas["warn"] = [logPrefix](const std::string &fmt, sol::variadic_args va) {
        try {
            std::string text = FormatString(fmt, va);
            Log::Warn("%s %s", logPrefix.c_str(), text.c_str());
        } catch (const std::exception &e) {
            Log::Error("%s Error in warn: %s", logPrefix.c_str(), e.what());
        }
    };

    // tas.error(format_string, ...)
    tas["error"] = [logPrefix](const std::string &fmt, sol::variadic_args va) {
        try {
            std::string text = FormatString(fmt, va);
            Log::Error("%s %s", logPrefix.c_str(), text.c_str());
        } catch (const std::exception &e) {
            Log::Error("%s Error in error: %s", logPrefix.c_str(), e.what());
        }
    };

    // tas.print(format_string, ...)
    tas["print"] = [logPrefix, context](const std::string &fmt, sol::variadic_args va) {
        try {
            std::string text = FormatString(fmt, va);
            context->GetGameInterface()->PrintMessage(text.c_str());
        } catch (const std::exception &e) {
            Log::Error("%s Error in print: %s", logPrefix.c_str(), e.what());
        }
    };

    // tas.get_tick()
    tas["get_tick"] = [context]() -> unsigned int {
        return context->GetCurrentTick();
    };

    // tas.get_manifest()
    tas["get_manifest"] = [context]() -> sol::table {
        try {
            const TASProject *project = context->GetCurrentProject();
            if (project) {
                return project->GetManifestTable();
            }
        } catch (const std::exception &) {
            // Fall through to return empty table
        }
        return context->GetLuaState().create_table();
    };
}
