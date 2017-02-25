project(omaha)

add_executable(ps-omaha main.cpp)
#add_definitions ("-ansi -Wall -std=c++0x")
add_definitions ("-std=c++0x")

target_link_libraries(ps-omaha
        peval
        penum
        ${Boost_LIBRARIES}
)
