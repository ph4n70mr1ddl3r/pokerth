# PokerTH Unit Tests

set(POKERTH_TEST_SRC
    pokerth_tests.cpp
    pokerth_unittest.cpp
    pokerth_test_framework.h
)

add_executable(pokerth_tests ${POKERTH_TEST_SRC})

target_include_directories(pokerth_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/third_party/protobuf
    ${Boost_INCLUDE_DIRS}
    ${PROTOBUF_INCLUDE_DIR}
)

target_link_libraries(pokerth_tests PRIVATE
    pokerth_lib
    pokerth_protocol
    ${Boost_LIBRARIES}
    ${PROTOBUF_LIBRARIES}
)

target_compile_definitions(pokerth_tests PRIVATE
    ENABLE_IPV6
    HAVE_OPENSSL
)

set_target_properties(pokerth_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME PokerTH_UnitTests COMMAND pokerth_tests)

# PokerTH Additional Standalone Tests
set(POKERTH_ADDITIONAL_STANDALONE_TEST_SRC
    pokerth_additional_standalone.cpp
)

add_executable(pokerth_additional_standalone ${POKERTH_ADDITIONAL_STANDALONE_TEST_SRC})

target_include_directories(pokerth_additional_standalone PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(pokerth_additional_standalone PRIVATE
    pokerth_lib
    ${Boost_LIBRARIES}
)

set_target_properties(pokerth_additional_standalone PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME PokerTH_AdditionalStandaloneTests COMMAND pokerth_additional_standalone)

message(STATUS "PokerTH unit tests target created")
