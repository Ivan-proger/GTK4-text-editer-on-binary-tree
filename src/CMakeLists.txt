# src/CMakeLists.txt
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-4.0)

add_executable(editor
    main.cpp
    Tree.cpp
    BinaryTreeFile.cpp
)

# include dirs from pkg-config
target_include_directories(editor PRIVATE ${GTKMM_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
# compile flags (other flags, e.g. -pthread etc.)
target_compile_options(editor PRIVATE ${GTKMM_CFLAGS_OTHER})
# link libraries provided by pkg-config
target_link_libraries(editor PRIVATE ${GTKMM_LIBRARIES})
