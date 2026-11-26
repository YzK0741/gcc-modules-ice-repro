module;

#include <thread>

export module utility:log;

namespace utility {

void f()
{
    std::thread t([] {});
    t.detach();
}

}
