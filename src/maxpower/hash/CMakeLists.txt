set(CMAKE_C_FLAGS "-g -O3 -fPIC -std=c99 -Wall -Wextra -Werror -Wno-missing-field-initializers")

include_directories($ENV{MAXCOMPILERDIR}/include/slic)
include_directories(include)

link_directories($ENV{MAXCOMPILERDIR}/lib $ENV{MAXELEROSDIR}/lib)

add_library(maxhash
	maxhash.c
	maxhash_slic.c
)
target_link_libraries(maxhash
	slic maxeleros m pthread
)

install(
	TARGETS maxhash
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
)
install(
	FILES include/maxhash.h
	DESTINATION include
)