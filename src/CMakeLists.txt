IF(UNIX)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -std=c++11")
ENDIF()

ADD_EXECUTABLE(${PROJECT_NAME} db.cpp mmap.cpp photo.cpp sha1.cpp timestamp.cpp sqlite3.c photodb.cpp)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} exiv2 pthread)
