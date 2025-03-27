add_subdirectory(common)
add_subdirectory(ipc)
add_subdirectory(rpc)

set(INCROOT "${PROJECT_SOURCE_DIR}/include")
set(SRCROOT "${PROJECT_SOURCE_DIR}/src")

set(SRC
    ${SRCROOT}/config.cpp
    ${INCROOT}/config.h

    ${SRCROOT}/service.cpp
    ${INCROOT}/service.h

    ${SRCROOT}/service_rpc.cpp
    ${INCROOT}/service_rpc.h

    ${SRCROOT}/assets.cpp
    ${INCROOT}/assets.h

    ${SRCROOT}/main.cpp
)

if (NOT "${SRC}" STREQUAL "")
    message(STATUS "Building main executable...")
    add_executable(main ${SRC})
    target_include_directories(main PUBLIC ${INCROOT})
    target_link_libraries(main PUBLIC common ipc rpc nlohmann_json jinja2cpp)
    set_target_properties(main PROPERTIES LINKER_LANGUAGE CXX)
else()
    message(WARNING "Main executable is not built")
endif()

add_subdirectory(tools)

set(SRC
    ${SRCROOT}/assets.cpp
    ${INCROOT}/assets.h

    ${SRCROOT}/service_rpc.cpp
    ${INCROOT}/service_rpc.h

    ${SRCROOT}/test.cpp
)

if (NOT "${SRC}" STREQUAL "")
    message(STATUS "Building main executable...")
    add_executable(test ${SRC})
    target_include_directories(test PUBLIC ${INCROOT})
    target_link_libraries(test PUBLIC common ipc rpc nlohmann_json jinja2cpp pqxx)
    set_target_properties(test PROPERTIES LINKER_LANGUAGE CXX)
else()
    message(WARNING "Main executable is not built")
endif()

