#include "LuaApi.h"

#include "Logger.h"
#include "TASEngine.h"
#include "ScriptContext.h"

// ===================================================================
//  Result Type for Error Handling
// ===================================================================

/**
 * @brief Rust-inspired Result<T, E> type for Lua error handling
 *
 * Provides type-safe, composable error handling as an alternative to pcall/xpcall.
 * A Result can be in one of two states:
 * - Ok(value): Success with a value
 * - Err(error): Failure with an error message
 *
 * Example usage:
 * @code
 *   local result = tas.result.try(function()
 *       if condition then
 *           error("Failed!")
 *       end
 *       return 42
 *   end)
 *
 *   return result
 *       :map(function(x) return x * 2 end)
 *       :unwrap_or(0)
 * @endcode
 */
struct LuaResult {
    bool is_success;
    sol::object value;     // Value if Ok
    std::string error_msg; // Error message if Err

    LuaResult(bool success, sol::object val, std::string err = "")
        : is_success(success), value(std::move(val)), error_msg(std::move(err)) {}

    // Factory methods
    static LuaResult Ok(sol::object value) {
        return LuaResult(true, std::move(value), "");
    }

    static LuaResult Err(const std::string &error) {
        return LuaResult(false, sol::nil, error);
    }
};

void LuaApi::RegisterResultApi(sol::table &tas, ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::RegisterResultApi requires a valid ScriptContext");
    }

    std::string logPrefix = "[" + context->GetName() + "]";
    sol::state &lua = context->GetLuaState();

    // Register Result userdata type
    sol::usertype<LuaResult> result_type = lua.new_usertype<LuaResult>(
        "Result",
        sol::no_constructor // Use factory functions
    );

    // --- State Checking Methods ---

    // result:is_ok() - Check if result is success
    result_type["is_ok"] = [](const LuaResult &self) -> bool {
        return self.is_success;
    };

    // result:is_err() - Check if result is error
    result_type["is_err"] = [](const LuaResult &self) -> bool {
        return !self.is_success;
    };

    // --- Value Extraction Methods ---

    // result:unwrap() - Get value or throw error
    result_type["unwrap"] = [](const LuaResult &self) -> sol::object {
        if (!self.is_success) {
            throw sol::error("Called unwrap() on Err result: " + self.error_msg);
        }
        return self.value;
    };

    // result:unwrap_or(default) - Get value or default
    result_type["unwrap_or"] = [](const LuaResult &self, sol::object default_value) -> sol::object {
        if (self.is_success) {
            return self.value;
        }
        return default_value;
    };

    // result:unwrap_or_else(fn) - Get value or compute default
    result_type["unwrap_or_else"] = [](const LuaResult &self, sol::function fn) -> sol::object {
        if (self.is_success) {
            return self.value;
        }
        sol::protected_function_result result = fn();
        if (!result.valid()) {
            sol::error err = result;
            throw sol::error("unwrap_or_else function failed: " + std::string(err.what()));
        }
        return result;
    };

    // result:expect(msg) - Get value or throw custom error
    result_type["expect"] = [](const LuaResult &self, const std::string &msg) -> sol::object {
        if (!self.is_success) {
            throw sol::error(msg + ": " + self.error_msg);
        }
        return self.value;
    };

    // --- Error Extraction Methods ---

    // result:error() - Get error message (or nil if Ok)
    result_type["error"] = [](const LuaResult &self, sol::this_state s) -> sol::object {
        sol::state_view lua(s);
        if (self.is_success) {
            return sol::nil;
        }
        return sol::make_object(lua, self.error_msg);
    };

    // --- Transformation Methods ---

    // result:map(fn) - Transform Ok value
    result_type["map"] = [](const LuaResult &self, sol::function fn, sol::this_state s) -> LuaResult {
        sol::state_view lua(s);

        if (!self.is_success) {
            // Propagate error
            return LuaResult::Err(self.error_msg);
        }

        // Apply function to value
        sol::protected_function_result result = fn(self.value);
        if (!result.valid()) {
            sol::error err = result;
            return LuaResult::Err("map function failed: " + std::string(err.what()));
        }

        return LuaResult::Ok(result);
    };

    // result:map_err(fn) - Transform Err message
    result_type["map_err"] = [](const LuaResult &self, sol::function fn, sol::this_state s) -> LuaResult {
        sol::state_view lua(s);

        if (self.is_success) {
            // Propagate success
            return LuaResult::Ok(self.value);
        }

        // Apply function to error
        sol::protected_function_result result = fn(sol::make_object(lua, self.error_msg));
        if (!result.valid()) {
            sol::error err = result;
            return LuaResult::Err("map_err function failed: " + std::string(err.what()));
        }

        if (result.get_type() == sol::type::string) {
            return LuaResult::Err(result.get<std::string>());
        }

        return LuaResult::Err(self.error_msg); // Keep original if transform fails
    };

    // result:and_then(fn) - Flat-map (fn returns Result)
    result_type["and_then"] = [](const LuaResult &self, sol::function fn, sol::this_state s) -> LuaResult {
        sol::state_view lua(s);

        if (!self.is_success) {
            // Propagate error
            return LuaResult::Err(self.error_msg);
        }

        // Apply function (should return Result)
        sol::protected_function_result result = fn(self.value);
        if (!result.valid()) {
            sol::error err = result;
            return LuaResult::Err("and_then function failed: " + std::string(err.what()));
        }

        // Extract Result from function result
        sol::object obj = result;
        if (obj.is<LuaResult>()) {
            return obj.as<LuaResult>();
        }

        // If function doesn't return Result, wrap it
        return LuaResult::Ok(obj);
    };

    // result:or_else(fn) - Recover from error (fn returns Result)
    result_type["or_else"] = [](const LuaResult &self, sol::function fn, sol::this_state s) -> LuaResult {
        sol::state_view lua(s);

        if (self.is_success) {
            // Propagate success
            return LuaResult::Ok(self.value);
        }

        // Apply recovery function
        sol::protected_function_result result = fn(sol::make_object(lua, self.error_msg));
        if (!result.valid()) {
            sol::error err = result;
            return LuaResult::Err("or_else function failed: " + std::string(err.what()));
        }

        // Extract Result from function result
        sol::object obj = result;
        if (obj.is<LuaResult>()) {
            return obj.as<LuaResult>();
        }

        // If function doesn't return Result, wrap it
        return LuaResult::Ok(obj);
    };

    // --- Pattern Matching ---

    // result:match({ok = fn, err = fn}) - Pattern match
    result_type["match"] = [](const LuaResult &self, sol::table handlers) -> sol::object {
        if (self.is_success) {
            sol::optional<sol::function> ok_handler = handlers["ok"];
            if (ok_handler) {
                sol::protected_function_result result = ok_handler.value()(self.value);
                if (result.valid()) {
                    return result;
                }
            }
            return self.value;
        } else {
            sol::optional<sol::function> err_handler = handlers["err"];
            if (err_handler) {
                sol::protected_function_result result = err_handler.value()(self.error_msg);
                if (result.valid()) {
                    return result;
                }
            }
            return sol::nil;
        }
    };

    // --- Create nested 'result' table for factory functions ---
    sol::table result_api = tas["result"] = tas.create();

    // tas.result.ok(value) - Create success result
    result_api["ok"] = [](sol::object value, sol::this_state s) -> LuaResult {
        return LuaResult::Ok(value);
    };

    // tas.result.err(error_msg) - Create error result
    result_api["err"] = [](const std::string &error_msg, sol::this_state s) -> LuaResult {
        return LuaResult::Err(error_msg);
    };

    // tas.result.try(fn) - Try function and capture error
    result_api["try"] = [logPrefix](sol::function fn, sol::this_state s) -> LuaResult {
        sol::state_view lua(s);

        sol::protected_function_result result = fn();

        if (result.valid()) {
            // Success
            sol::object value = result;
            return LuaResult::Ok(value);
        } else {
            // Error
            sol::error err = result;
            std::string error_msg = err.what();
            Log::Warn("%s Result.try caught error: %s", logPrefix.c_str(), error_msg.c_str());
            return LuaResult::Err(error_msg);
        }
    };

    // tas.result.all(results) - Combine multiple Results (all must be Ok)
    result_api["all"] = [](sol::table results, sol::this_state s) -> LuaResult {
        sol::state_view lua(s);
        sol::table values = lua.create_table();

        size_t index = 1;
        for (const auto &pair : results) {
            sol::object obj = pair.second;
            if (!obj.is<LuaResult>()) {
                return LuaResult::Err("result.all: all elements must be Result");
            }

            LuaResult &result = obj.as<LuaResult &>();
            if (!result.is_success) {
                // First error short-circuits
                return LuaResult::Err(result.error_msg);
            }

            values[index++] = result.value;
        }

        return LuaResult::Ok(values);
    };

    // tas.result.any(results) - First successful Result
    result_api["any"] = [](sol::table results, sol::this_state s) -> LuaResult {
        sol::state_view lua(s);
        std::vector<std::string> errors;

        for (const auto &pair : results) {
            sol::object obj = pair.second;
            if (!obj.is<LuaResult>()) {
                return LuaResult::Err("result.any: all elements must be Result");
            }

            LuaResult &result = obj.as<LuaResult &>();
            if (result.is_success) {
                // First success wins
                return LuaResult::Ok(result.value);
            }

            errors.push_back(result.error_msg);
        }

        // All failed
        std::string combined_error = "result.any: all results failed:\n";
        for (size_t i = 0; i < errors.size(); ++i) {
            combined_error += "  [" + std::to_string(i + 1) + "] " + errors[i] + "\n";
        }

        return LuaResult::Err(combined_error);
    };
}
