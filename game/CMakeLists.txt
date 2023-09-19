
add_executable(netmud netmud.cpp version.cpp)
target_link_libraries(netmud ugly crypt pcre ncurses)

