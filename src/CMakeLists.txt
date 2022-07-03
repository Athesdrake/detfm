add_subdirectory(detfm)
add_subdirectory(match)
target_sources(detfm PRIVATE detfm.cpp main.cpp renamer.cpp utils.cpp)
