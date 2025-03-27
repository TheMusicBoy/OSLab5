include(FetchContent)

# # -- Tests --

# FetchContent_Declare(googletest
#     URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip)

# FetchContent_MakeAvailable(googletest)
# FetchContent_GetProperties(googletest)

# if (NOT googletest_POPULATED)
#     FetchContent_Populate(googletest)
#     add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
# endif()

# -- JSON --

FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2  # Specify a release tag
)

FetchContent_MakeAvailable(json)
FetchContent_GetProperties(json)

if (NOT json_POPULATED)
    FetchContent_Populate(json)
    add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR})
endif()

# -- Jinja2Cpp --

FetchContent_Declare(jinja
    GIT_REPOSITORY https://github.com/jinja2cpp/Jinja2Cpp.git
    GIT_TAG 1.2.1
)

FetchContent_MakeAvailable(jinja)
FetchContent_GetProperties(jinja)

if (NOT jinja_POPULATED)
    FetchContent_Populate(jinja)
    add_subdirectory(${jinja_SOURCE_DIR} ${jinja_BINARY_DIR})
endif()

# -- Libpqxx --

FetchContent_Declare(libpqxx
    GIT_REPOSITORY https://github.com/jtv/libpqxx.git
    GIT_TAG 7.10.0
)

FetchContent_MakeAvailable(libpqxx)
FetchContent_GetProperties(libpqxx)

if (NOT libpqxx_POPULATED)
    FetchContent_Populate(libpqxx)
    set(BUILD_TEST OFF CACHE BOOL "Disable tests")
    add_subdirectory(${libpqxx_SOURCE_DIR} ${libpqxx_BINARY_DIR})
endif()
