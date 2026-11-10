#pragma once

// Tiny assertion helpers so the OBS-independent unit tests need no test framework.

#include <iostream>
#include <sstream>
#include <string>

namespace setolia_test {

inline int &failureCount()
{
	static int count = 0;
	return count;
}

inline void reportFailure(const char *file, int line, const std::string &message)
{
	failureCount()++;
	std::cerr << file << ":" << line << ": FAILED: " << message << "\n";
}

inline int finish(const char *suite)
{
	if (failureCount() == 0) {
		std::cout << suite << ": all tests passed\n";
		return 0;
	}
	std::cerr << suite << ": " << failureCount() << " failure(s)\n";
	return 1;
}

} // namespace setolia_test

#define CHECK(cond)                                                                   \
	do {                                                                          \
		if (!(cond)) {                                                        \
			setolia_test::reportFailure(__FILE__, __LINE__, #cond);       \
		}                                                                     \
	} while (0)

#define CHECK_EQ(actual, expected)                                                    \
	do {                                                                          \
		const auto &actual_ = (actual);                                       \
		const auto &expected_ = (expected);                                   \
		if (!(actual_ == expected_)) {                                        \
			std::ostringstream message_;                                  \
			message_ << #actual << " == " << #expected << " (got "         \
				 << actual_ << ", expected " << expected_ << ")";      \
			setolia_test::reportFailure(__FILE__, __LINE__, message_.str()); \
		}                                                                     \
	} while (0)
