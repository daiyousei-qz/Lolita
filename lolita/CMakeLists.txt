
file(GLOB_RECURSE SOURCE_FILES "./*.cpp")
file(GLOB_RECURSE HEADER_FILES "./include/*.h")

include_directories("./include")
add_library(LolitaLib STATIC ${SOURCE_FILES} ${HEADER_FILES})