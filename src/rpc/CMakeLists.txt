set(INCROOT "${PROJECT_SOURCE_DIR}/include/rpc")
set(SRCROOT "${PROJECT_SOURCE_DIR}/src/rpc")

set(SRC
    ${SRCROOT}/http_server.cpp
    ${SRCROOT}/service_rpc.cpp

)

add_library(rpc STATIC ${SRC})

target_include_directories(rpc PUBLIC 
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(rpc PUBLIC common nlohmann_json jinja2cpp)

set_target_properties(rpc PROPERTIES LINKER_LANGUAGE CXX)
