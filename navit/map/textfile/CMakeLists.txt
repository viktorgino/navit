add_library(map_textfile SHARED textfile.c)
target_include_directories(map_textfile PRIVATE "../" "../../")

set_target_properties(map_textfile PROPERTIES COMPILE_DEFINITIONS "MODULE=map_textfile")