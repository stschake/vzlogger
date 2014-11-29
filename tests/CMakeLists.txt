include_directories(
    ${GTEST_INCLUDE_DIRS}
)

# all *.cpp files here will be used. No main() required as we link with gtest_main
file(GLOB test_sources *.cpp)

SET(CMAKE_CXX_FLAGS="-g -O0 -Wall -fprofile-arcs -ftest-coverage")
 
SET(CMAKE_C_FLAGS="-g -O0 -Wall -fprofile-arcs -ftest-coverage")
 
#SET(CMAKE_SHARED_LINKER_FLAGS="-fprofile-arcs -ftest-coverage")
SET(CMAKE_EXE_LINKER_FLAGS="-fprofile-arcs -ftest-coverage")

add_executable(vzlogger_unit_tests ${test_sources})

target_link_libraries(vzlogger_unit_tests
    ${GTEST_LIBS_DIR}/libgtest.a
    ${GTEST_LIBS_DIR}/libgtest_main.a
    ${JSON_LIBRARY}
    pthread)
