add_library(dbnopra STATIC osi.c dbn_opra_discover.c)

target_link_libraries(dbnopra PUBLIC dbn)


# Users of this library can #include <dbnopra.h>
#
target_include_directories(dbnopra PUBLIC .)
