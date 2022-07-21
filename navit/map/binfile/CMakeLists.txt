add_library(map_binfile SHARED binfile.c)
target_include_directories(map_binfile PRIVATE "../" "../../")
