aux_source_directory(. source_dir)

ADD_EXECUTABLE(presureTest ${source_dir})

target_link_libraries(presureTest pthread rt)