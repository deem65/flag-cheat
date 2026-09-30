#include "LuaPolicy.h"
#include "FileIO.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <cmath>
#include <stdexcept>
#include <utility>

namespace drek_flag_cheat {
    namespace {
        class StackRestore {
        public:
            explicit StackRestore(lua_State* state) : state_(state), top_(lua_gettop(state)) {}
            ~StackRestore() { lua_settop(state_, top_); }
        private:
            lua_State* state_;
            int top_;
        };

        std::string LuaError(lua_State* state) {
            const char* message = lua_tostring(state, -1);
            return message ? message : "Lua reported a non-text error.";
        }

        std::string StringField(lua_State* state, int table, const char* field) {
            lua_getfield(state, table, field);
            if (lua_type(state, -1) != LUA_TSTRING) {
                throw std::runtime_error(std::string("Lua field must be a string: ") + field);
            }
            std::size_t length = 0;
            const char* text = lua_tolstring(state, -1, &length);
            std::string value(text, length);
            lua_pop(state, 1);
            if (value.empty() || value.size() > 512 || value.find('\0') != std::string::npos) {
                throw std::runtime_error(std::string("Invalid Lua text field: ") + field);
            }
            return value;
        }

        void SetStringField(lua_State* state, const char* field, const std::string& value) {
            lua_pushlstring(state, value.data(), value.size());
            lua_setfield(state, -2, field);
        }

        void SetNumberField(lua_State* state, const char* field, double value) {
            lua_pushnumber(state, value);
            lua_setfield(state, -2, field);
        }
    } 

    LuaPolicy::LuaPolicy(const Matcher& matcher) : matcher_(matcher) {
        state_ = luaL_newstate();
        if (!state_) {
            throw std::runtime_error("Could not create the Lua interpreter.");
        }
        luaL_openlibs(state_);
        lua_newtable(state_);
        lua_pushlightuserdata(state_, this);
        lua_pushcclosure(state_, CompareFromLua, 1);
        lua_setfield(state_, -2, "compare");
        lua_pushlightuserdata(state_, this);
        lua_pushcclosure(state_, DescribeFromLua, 1);
        lua_setfield(state_, -2, "describe");
        lua_setglobal(state_, "native");
    }

    LuaPolicy::~LuaPolicy() {
        if (state_) {
            lua_close(state_);
        }
    }

    void LuaPolicy::RunFile(const std::filesystem::path& path) {
        const auto bytes = ReadFileBytes(path, 2 * 1024 * 1024);
        const auto name = path.filename().u8string();
        if (luaL_loadbufferx(state_, reinterpret_cast<const char*>(bytes.data()), bytes.size(), name.c_str(), "t") != LUA_OK ||
            lua_pcall(state_, 0, 1, 0) != LUA_OK) {
            throw std::runtime_error(name + ": " + LuaError(state_));
        }
        if (!lua_istable(state_, -1)) {
            throw std::runtime_error(name + " must return a table.");
        }
    }

    std::vector<FlagRecord> LuaPolicy::LoadCatalog(const std::filesystem::path& path) {
        StackRestore restore(state_);
        RunFile(path);
        const auto count = lua_rawlen(state_, -1);
        if (count == 0 || count > 2048) {
            throw std::runtime_error("Flag catalog must contain between 1 and 2048 references.");
        }
        std::vector<FlagRecord> records;
        records.reserve(count);
        for (std::size_t index = 1; index <= count; ++index) {
            lua_rawgeti(state_, -1, static_cast<lua_Integer>(index));
            if (!lua_istable(state_, -1)) {
                throw std::runtime_error("Each catalog entry must be a table.");
            }
            FlagRecord record{ StringField(state_, -1, "code"), StringField(state_, -1, "name"),
                              StringField(state_, -1, "file") };
            if (record.file.find_first_of("/\\:") != std::string::npos ||
                record.file == "." || record.file == "..") {
                throw std::runtime_error("Catalog image entries must be simple filenames.");
            }
            records.push_back(std::move(record));
            lua_pop(state_, 1);
        }
        return records;
    }

    void LuaPolicy::LoadPolicy(const std::filesystem::path& path) {
        StackRestore restore(state_);
        RunFile(path);
        lua_getfield(state_, -1, "recognize");
        if (!lua_isfunction(state_, -1)) {
            throw std::runtime_error("recognize.lua must return a table containing a recognize function.");
        }
        lua_pop(state_, 1);
        if (policyReference_ != LUA_NOREF) {
            luaL_unref(state_, LUA_REGISTRYINDEX, policyReference_);
        }
        policyReference_ = luaL_ref(state_, LUA_REGISTRYINDEX);
    }

    Recognition LuaPolicy::Recognize(const ImageFeatures& query) {
        if (policyReference_ == LUA_NOREF) {
            throw std::runtime_error("The recognition policy has not been loaded.");
        }
        StackRestore restore(state_);
        lua_rawgeti(state_, LUA_REGISTRYINDEX, policyReference_);
        lua_getfield(state_, -1, "recognize");
        currentQuery_ = &query;
        const int status = lua_pcall(state_, 0, 1, 0);
        currentQuery_ = nullptr;
        if (status != LUA_OK) {
            throw std::runtime_error("Recognition policy: " + LuaError(state_));
        }
        if (!lua_istable(state_, -1)) {
            throw std::runtime_error("Recognition policy must return a result table.");
        }
        Recognition result;
        const auto resultStatus = StringField(state_, -1, "status");
        if (resultStatus == "match") result.status = MatchStatus::Match;
        else if (resultStatus == "ambiguous") result.status = MatchStatus::Ambiguous;
        else if (resultStatus == "uncertain") result.status = MatchStatus::Uncertain;
        else throw std::runtime_error("Unknown recognition status: " + resultStatus);

        lua_getfield(state_, -1, "candidates");
        if (!lua_istable(state_, -1)) {
            throw std::runtime_error("Recognition candidates must be a table.");
        }
        const auto count = lua_rawlen(state_, -1);
        if (count == 0 || count > 512) {
            throw std::runtime_error("Invalid number of recognition candidates.");
        }
        for (std::size_t index = 1; index <= count; ++index) {
            lua_rawgeti(state_, -1, static_cast<lua_Integer>(index));
            if (!lua_istable(state_, -1)) {
                throw std::runtime_error("Invalid candidate entry.");
            }
            Candidate candidate{ StringField(state_, -1, "code"), StringField(state_, -1, "name"), 0 };
            lua_getfield(state_, -1, "error");
            if (!lua_isnumber(state_, -1)) {
                throw std::runtime_error("Candidate error must be a number.");
            }
            candidate.error = lua_tonumber(state_, -1);
            if (!std::isfinite(candidate.error) || candidate.error < 0) {
                throw std::runtime_error("Candidate error must be finite and nonnegative.");
            }
            lua_pop(state_, 2);
            result.candidates.push_back(std::move(candidate));
        }
        return result;
    }

    int LuaPolicy::DescribeFromLua(lua_State* state) {
        const auto* self = static_cast<LuaPolicy*>(lua_touserdata(state, lua_upvalueindex(1)));
        if (!self || !self->currentQuery_) {
            return luaL_error(state, "native.describe is available only during recognition");
        }
        lua_createtable(state, 0, 3);
        SetNumberField(state, "width", self->currentQuery_->sourceWidth);
        SetNumberField(state, "height", self->currentQuery_->sourceHeight);
        SetNumberField(state, "variation", self->currentQuery_->colourVariation);
        return 1;
    }

    int LuaPolicy::CompareFromLua(lua_State* state) {
        auto* self = static_cast<LuaPolicy*>(lua_touserdata(state, lua_upvalueindex(1)));
        const auto requestedCount = luaL_checkinteger(state, 1);
        if (!self || !self->currentQuery_) {
            return luaL_error(state, "native.compare is available only during recognition");
        }
        if (requestedCount < 1 || requestedCount > 512) {
            return luaL_error(state, "shortlist_size must be between 1 and 512");
        }
        try {
            const auto comparisons = self->matcher_.Compare(*self->currentQuery_, static_cast<int>(requestedCount));
            lua_createtable(state, static_cast<int>(comparisons.size()), 0);
            int index = 1;
            for (const auto& comparison : comparisons) {
                lua_createtable(state, 0, 6);
                SetStringField(state, "code", comparison.code);
                SetStringField(state, "name", comparison.name);
                SetNumberField(state, "colour", comparison.colourError);
                SetNumberField(state, "detail", comparison.detailError);
                SetNumberField(state, "edge", comparison.edgeError);
                SetNumberField(state, "aspect", comparison.aspectError);
                lua_rawseti(state, -2, index++);
            }
            return 1;
        }
        catch (const std::exception& error) {
            self->nativeError_ = error.what();
        }
        catch (...) {
            self->nativeError_ = "Unknown error in the native matcher.";
        }
        lua_pushlstring(state, self->nativeError_.data(), self->nativeError_.size());
        return lua_error(state);
    }

}
