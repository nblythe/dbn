add_library(dbn STATIC dbn.c)


# Use PkgConfig to find libsodium and liburing.
#
find_package(PkgConfig REQUIRED)

pkg_check_modules(sodium_PKG REQUIRED libsodium)
target_include_directories(dbn PRIVATE ${sodium_PKG_INCLUDE_DIRS})
target_link_libraries(dbn PRIVATE ${sodium_PKG_LIBRARIES})

pkg_check_modules(liburing REQUIRED liburing)
target_include_directories(dbn PRIVATE ${liburing_INCLUDE_DIRS})
target_link_libraries(dbn PRIVATE ${liburing_LIBRARIES})


# Users of this library can #include <dbn.h>
#
target_include_directories(dbn PUBLIC .)
