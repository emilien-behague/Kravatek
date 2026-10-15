#pragma once
#include "Profile.h"
#include <string_view>
namespace kravatek::genetic {
std::string encodeProfile(const GenreProfile &);
GenreProfile decodeProfile(std::string_view);
std::vector<GenreProfile> decodeCatalog(std::string_view);
std::vector<GenreProfile> bundledProfiles();
} // namespace kravatek::genetic
