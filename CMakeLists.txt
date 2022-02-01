cmake_minimum_required(VERSION 3.0.0)
project(raytracer)

set(CMAKE_CPP_STANDARD 17)

find_package(glm CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2-image CONFIG REQUIRED)

add_executable(${PROJECT_NAME}
    src/main.cpp
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        glm::glm
        SDL2::SDL2 SDL2::SDL2main
        SDL2::SDL2_image
)
