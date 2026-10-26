#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

#if defined(SKYOS_SKYSEARCH_EXPORTS)
#define SKYOS_SKYSEARCH_API __declspec(dllexport)
#else
#define SKYOS_SKYSEARCH_API __declspec(dllimport)
#endif

namespace SkyOS {
    [[nodiscard]] SKYOS_SKYSEARCH_API std::string SearchSkyrim(const std::string& query, const std::string& category, const std::string& modName = {}, std::size_t maxResults = 500);
    [[nodiscard]] SKYOS_SKYSEARCH_API bool ExecuteSkySearchAction(const std::string& action, std::uint32_t formId, std::int32_t count, std::string& error, std::string& feedback);
}
