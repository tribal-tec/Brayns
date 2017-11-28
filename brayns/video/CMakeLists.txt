
set(BRAYNSVIDEO_SOURCES VideoEncoder.cpp)

set(BRAYNSVIDEO_PUBLIC_HEADERS VideoEncoder.h)

set(BRAYNSVIDEO_LINK_LIBRARIES PRIVATE ${FFMPEG_LIBRARIES} Lunchbox)

common_library(braynsVideo)
