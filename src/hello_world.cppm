module;

#include <iostream>  // import declaration

export module hello_world;  // module declaration
/**
 * @brief "Hello world!"を表示する関数
 */
export void hello() {  // export declaration
    std::cout << "Hello world!\n";
}
export void hello2() {  // export declaration
    std::cout << "Hello world 2!\n";
}

/**
 * @brief 2つの整数を加算する関数
 * @param a 加算する整数1
 * @param b 加算する整数2
 * @return 加算結果
 */
export int add(int a, int b) { return a + b; }
