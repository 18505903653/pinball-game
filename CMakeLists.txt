project(BallGame)

include_directories(include)

add_subdirectory(src/server bin)
add_subdirectory(src/client bin)
add_subdirectory(src/test bin)