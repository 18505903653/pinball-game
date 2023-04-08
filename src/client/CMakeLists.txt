aux_source_directory(. source_dir)

ADD_EXECUTABLE(client ${source_dir})
target_link_libraries(client pthread curses)