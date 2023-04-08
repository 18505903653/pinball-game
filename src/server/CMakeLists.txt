aux_source_directory(. source_dir)

ADD_EXECUTABLE(server ${source_dir})

target_link_libraries(server pthread rt)