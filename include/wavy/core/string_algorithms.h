/**
 * @file   string_algorithms.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2017.03.16
 *
 * @brief  Implementation of replacements for boost string algorithms (lexical_cast, split, ...).
 */

#pragma once

#include <vector>
#include <regex>
#include <sstream>

namespace mysh::core {

    template<typename T> T lexical_cast(const std::string& arg) {
        std::stringstream sstr(arg);
        T result;
        sstr >> result;
        return result;
    }

    inline bool ends_with(const std::string& value, const std::string& ending)
    {
        if (ending.size() > value.size()) { return false; }
        return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
    }
}
