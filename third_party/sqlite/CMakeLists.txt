cmake_minimum_required(VERSION 3.15 FATAL_ERROR)



add_library(sqlite OBJECT sqlite3.c sqlite3.h)

target_include_directories(sqlite PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET sqlite PROPERTY C_STANDARD 99)
target_compile_definitions(sqlite PRIVATE -DSQLITE_ENABLE_JSON -DSQLITE_THREADSAFE=0 SQLITE_OMIT_DEPRECATED SQLITE_OMIT_SHARED_CACHE SQLITE_USE_ALLOCA)


add_library(sqlite::sqlite ALIAS sqlite)


message(STATUS "executed sqlite ${CMAKE_CURRENT_SOURCE_DIR}")