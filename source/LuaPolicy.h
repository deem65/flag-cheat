#pragma once

#include "Matcher.h"

#include <filesystem>
#include <string>
#include <vector>

struct lua_State;

namespace drek_flag_cheat {

    enum class MatchStatus { Match, Ambiguous, Uncertain };

    struct Candidate {
        std::string code;
        std::string name;
        double error = 0;
    };

    struct Recognition {
        MatchStatus status = MatchStatus::Uncertain;
        std::vector<Candidate> candidates;
    };

    class LuaPolicy {
    public:
        explicit LuaPolicy(const Matcher& matcher);
        ~LuaPolicy();
        LuaPolicy(const LuaPolicy&) = delete;
        LuaPolicy& operator=(const LuaPolicy&) = delete;

        std::vector<FlagRecord> LoadCatalog(const std::filesystem::path& path);
        void LoadPolicy(const std::filesystem::path& path);
        Recognition Recognize(const ImageFeatures& query);

    private:
        static int CompareFromLua(lua_State* state);
        static int DescribeFromLua(lua_State* state);
        void RunFile(const std::filesystem::path& path);

        lua_State* state_ = nullptr;
        const Matcher& matcher_;
        const ImageFeatures* currentQuery_ = nullptr;
        int policyReference_ = -2;
        std::string nativeError_;
    };

}


