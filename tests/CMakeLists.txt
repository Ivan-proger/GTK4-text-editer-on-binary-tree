# tests/CMakeLists.txt
add_executable(test1 test1.cpp ../src/Tree.cpp ../src/BinaryTreeFile.cpp)
target_include_directories(test1 PRIVATE ../src)
# (не нужно линковать GTK для тестов)
