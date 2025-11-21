#ifndef AFD4177B_6535_4E69_BA23_15F3FDD64FED
#define AFD4177B_6535_4E69_BA23_15F3FDD64FED

#include <boost/stacktrace.hpp>
#include <cstdlib>
#include <iostream>

inline void entt_assert_handler(bool condition, const char* msg, const char* expr, const char* file,
                                int line) {
    if (condition) return;

    std::cerr << "ENTT_ASSERT failed: " << msg << "\n"
              << "  expr : " << expr << "\n"
              << "  file : " << file << ":" << line << "\n"
              << "Stacktrace:\n"
              << boost::stacktrace::stacktrace() << std::endl;

    std::abort();
}

#define ENTT_ASSERT(condition, msg) \
    ::entt_assert_handler((condition), (msg), #condition, __FILE__, __LINE__)

#endif /* AFD4177B_6535_4E69_BA23_15F3FDD64FED */
