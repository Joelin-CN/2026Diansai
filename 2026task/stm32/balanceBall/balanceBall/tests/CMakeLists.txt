cmake_minimum_required(VERSION 3.22)
project(balance_ball_host_tests C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
enable_testing()

add_executable(test_balance
    test_balance.c
    ../App/Src/balance_observer.c
    ../App/Src/balance_target.c
)
target_include_directories(test_balance PRIVATE ../App/Inc)
target_compile_options(test_balance PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_balance PRIVATE m)
add_test(NAME balance_core COMMAND test_balance)
