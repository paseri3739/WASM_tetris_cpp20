module;

#include <iostream>  // import declaration

export module hello_world;  // module declaration
export void hello() {       // export declaration
    std::cout << "Hello world!\n";
}
export void hello2() {  // export declaration
    std::cout << "Hello world 2!\n";
}
