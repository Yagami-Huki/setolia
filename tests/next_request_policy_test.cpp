#include "../src/source/next_request_policy.hpp"

#include "test_support.hpp"

namespace {

using setolia::isNextRequestAllowed;

void testUnrestrictedAllowsEverything()
{
	CHECK(isNextRequestAllowed("", 8080, false));
	CHECK(isNextRequestAllowed("https://evil.example", 8080, false));
	CHECK(isNextRequestAllowed("null", 8080, false));
}

void testRestrictedWithoutOrigin()
{
	// curl, most Stream Deck plugins
	CHECK(isNextRequestAllowed("", 8080, true));
}

void testRestrictedOwnOrigin()
{
	CHECK(isNextRequestAllowed("http://127.0.0.1:8080", 8080, true));
	CHECK(isNextRequestAllowed("http://localhost:8080", 8080, true));
	CHECK(isNextRequestAllowed("HTTP://LocalHost:18080", 18080, true));
}

void testRestrictedOtherOrigins()
{
	CHECK(!isNextRequestAllowed("https://evil.example", 8080, true));
	CHECK(!isNextRequestAllowed("null", 8080, true));
	CHECK(!isNextRequestAllowed("http://127.0.0.1:9000", 8080, true));  // other port
	CHECK(!isNextRequestAllowed("http://127.0.0.1", 8080, true));       // no port
	CHECK(!isNextRequestAllowed("https://127.0.0.1:8080", 8080, true)); // other scheme
	CHECK(!isNextRequestAllowed("http://127.0.0.1:80800", 8080, true)); // prefix only
	CHECK(!isNextRequestAllowed("http://localhost.evil:8080", 8080, true));
	CHECK(!isNextRequestAllowed("http://192.168.0.2:8080", 8080, true));
}

} // namespace

int main()
{
	testUnrestrictedAllowsEverything();
	testRestrictedWithoutOrigin();
	testRestrictedOwnOrigin();
	testRestrictedOtherOrigins();
	return setolia_test::finish("next_request_policy_test");
}
