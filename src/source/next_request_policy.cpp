#include "next_request_policy.hpp"

#include <algorithm>
#include <cctype>

namespace setolia {

bool isNextRequestAllowed(const std::string &origin, int port, bool restrict)
{
	if (!restrict || origin.empty()) {
		return true;
	}

	// Origins are serialized in lower case by browsers; compare case-insensitively anyway.
	std::string normalized = origin;
	std::transform(normalized.begin(), normalized.end(), normalized.begin(),
		       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	const std::string suffix = ":" + std::to_string(port);
	return normalized == "http://127.0.0.1" + suffix || normalized == "http://localhost" + suffix;
}

} // namespace setolia
