module;

#include <iostream>  // import declaration
#include <tl/expected.hpp>

export module hello;  // module declaration
/**
 * @brief "Hello world!"を表示する関数
 */
export void hello3() {  // export declaration
    std::cout << "Hello world 3!\n";
}

export tl::expected<std::string, std::string> greet(bool succeed) {
    if (succeed) {
        return tl::expected<std::string, std::string>("Hello from greet!");
    } else {
        return tl::unexpected<std::string>("Greeting failed.");
    }
}
