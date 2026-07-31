cmake_minimum_required(VERSION 3.22)
project(balance_ball_host_tests C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
enable_testing()

add_executable(test_balance
    test_balance.c
    ../App/Src/balance_actuator.c
    ../App/Src/balance_loop.c
    ../App/Src/balance_controller.c
    ../App/Src/balance_measurement.c
    ../App/Src/balance_observer.c
    ../App/Src/balance_supervisor.c
    ../App/Src/balance_target.c
)
target_include_directories(test_balance PRIVATE ../App/Inc)
target_compile_options(test_balance PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_balance PRIVATE m)
add_test(NAME balance_core COMMAND test_balance)

add_executable(test_emm_v5_protocol
    test_emm_v5_protocol.c
    ../App/Src/emm_v5_protocol.c
)
target_include_directories(test_emm_v5_protocol PRIVATE ../App/Inc)
target_compile_options(test_emm_v5_protocol PRIVATE -Wall -Wextra -Werror)
add_test(NAME emm_v5_protocol COMMAND test_emm_v5_protocol)

add_executable(test_balance_motor
    test_balance_motor.c
    ../App/Src/balance_motor.c
    ../App/Src/emm_v5_protocol.c
)
target_include_directories(test_balance_motor PRIVATE ../App/Inc)
target_compile_options(test_balance_motor PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_balance_motor PRIVATE m)
add_test(NAME balance_motor COMMAND test_balance_motor)
