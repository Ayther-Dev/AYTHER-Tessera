#include <memory>

int main(int argument_count, char**) {
    auto buffer = std::make_unique<int[]>(4);
    volatile int* address = buffer.get();
    address[argument_count + 4] = 7;
    return 0;
}
