#include "core/CompanionTimeRequest.h"
#include <assert.h>
#include <string>
#include <iostream>

int main() {
    const char *session = "e072a1b421a8-0123456789abcdef";
    auto accepts = [&](const std::string &request, int64_t expected) {
        int64_t value = -1;
        bool ok = CompanionTimeRequest::parse(request.data(), request.size(), session, value);
        assert(ok == (expected >= 0));
        if (ok) assert(value == expected);
        else assert(value == -1);
    };
    const std::string prefix = std::string("PocketPDA-Time-1\n") + session + "\n";
    accepts(prefix + "1791636000\n", 1791636000); // UTC passes through without local offset.
    accepts(prefix + "1577836800\n", 1577836800);
    accepts(prefix + "4102444799\n", 4102444799);
    for (const char *bad : {"0\n", "-1791636000\n", "1791636000", "1791636000\nextra",
                            "1791636000\n\n", "179163600x\n", "4102444800\n",
                            "1577836799\n", "99999999999999999999\n"}) accepts(prefix + bad, -1);
    accepts("PocketPDA-Time-1\nwrong-session\n1791636000\n", -1);
    accepts("PocketPDA-Time-2\n" + std::string(session) + "\n1791636000\n", -1);
    auto embeddedNull = prefix + "1791636000\n";
    embeddedNull.push_back(0);
    accepts(embeddedNull, -1);
    int64_t value = 0;
    assert(!CompanionTimeRequest::parse(prefix.data(), prefix.size(), "", value));
    for (size_t n = 0; n < prefix.size(); ++n)
        assert(!CompanionTimeRequest::parse(prefix.data(), n, session, value));
    std::cout << "Companion time request tests passed\n";
}
