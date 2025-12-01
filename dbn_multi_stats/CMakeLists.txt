add_executable(dbn_multi_stats dbn_multi_stats.c)

find_package(Threads REQUIRED)
target_link_libraries(dbn_multi_stats PUBLIC Threads::Threads)

target_link_libraries(dbn_multi_stats PRIVATE dbn)
