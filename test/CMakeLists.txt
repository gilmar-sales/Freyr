project(Test)

FetchContent_Declare(
  gtest
  GIT_REPOSITORY "https://github.com/google/googletest"
  GIT_TAG        "main"
)
FetchContent_MakeAvailable(gtest)

enable_testing()
include(GoogleTest)

include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

file(GLOB_RECURSE TESTS_SRC
        "src/**.cpp"
)

add_executable(Tests_run ${TESTS_SRC})

add_test(NAME Tests_run COMMAND Tests_run)

target_compile_definitions(Tests_run PUBLIC -DFREYR_BUILDING_TESTS)
target_link_libraries(Tests_run gtest gtest_main freyr::freyr)
target_include_directories(Tests_run PUBLIC ${gtest_SOURCE_DIR}/include ../include)

gtest_discover_tests(Tests_run)
