CMAKE_MINIMUM_REQUIRED(VERSION 3.10)

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED true)

SET(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic")

PROJECT(LIMG VERSION 1.0)

LINK_LIBRARIES(SDL2)

ADD_EXECUTABLE(limg
    src/main.cpp
    src/Image.h
    src/Image.cpp
    src/BmpImage.h
    src/BmpImage.cpp
    src/PnmImage.h
    src/PnmImage.cpp
    src/bitmagic.h
    src/Logger.h
    src/Logger.cpp
)

ADD_CUSTOM_TARGET(run
    DEPENDS limg
    COMMAND limg
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

