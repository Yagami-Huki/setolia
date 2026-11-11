#pragma once

#include <string>

namespace setolia {

// Decides whether POST /next is accepted.
//  - restrict == false: always (the behavior of 1.2.x and earlier).
//  - restrict == true: only requests without an Origin header (curl, most Stream Deck plugins) or from the
//    plugin's own pages, http://127.0.0.1:<port> or http://localhost:<port>. Requests from other web pages
//    (including Origin "null" of file:// pages) are rejected.
// `origin` is the Origin header value, or an empty string when the header is absent.
bool isNextRequestAllowed(const std::string &origin, int port, bool restrict);

} // namespace setolia
