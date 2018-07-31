
include_directories("../lolita/include")

add_executable(SRC "Source.cpp" "testheader.h")

target_link_libraries(SRC LolitaLib)