SET(TEST_SOURCES
    IntersectionDifferenceTest.cpp
    GraphTest.cpp
)

add_executable(GazerAdtTest ${TEST_SOURCES})
target_link_libraries(GazerAdtTest gtest_main GazerSupport)
add_test(GazerAdtTest GazerAdtTest)