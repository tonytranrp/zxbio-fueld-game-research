if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()

set(VIDEO_CPP "${SOURCE_DIR}/src/engine/video/VideoManager.cpp")
if(NOT EXISTS "${VIDEO_CPP}")
    message(FATAL_ERROR "Video safety guard failed: video manager source is missing")
endif()
file(READ "${VIDEO_CPP}" VIDEO_MANAGER)

# The FFmpeg subprocess backend was split out of VideoManager.cpp into
# VideoFfmpegBackend.cpp; the threading and partial-load cleanup invariants now
# live there, while the manager facade keeps the failed-load entry cleanup.
set(VIDEO_BACKEND_CPP "${SOURCE_DIR}/src/engine/video/VideoFfmpegBackend.cpp")
if(NOT EXISTS "${VIDEO_BACKEND_CPP}")
    message(FATAL_ERROR "Video safety guard failed: video ffmpeg backend source is missing")
endif()
file(READ "${VIDEO_BACKEND_CPP}" VIDEO_BACKEND)

if(NOT VIDEO_BACKEND MATCHES "std::atomic_bool m_looping")
    message(FATAL_ERROR "Video safety guard failed: backend looping flag must be atomic")
endif()
if(NOT VIDEO_BACKEND MATCHES "m_looping\\.store\\(looping")
    message(FATAL_ERROR "Video safety guard failed: setLooping/play must store through the atomic looping flag")
endif()
if(NOT VIDEO_BACKEND MATCHES "Raylib audio device is not ready\";[ \t\r\n]+unload\\(\\);")
    message(FATAL_ERROR "Video safety guard failed: audio-not-ready partial load must release allocated texture resources")
endif()
if(NOT VIDEO_BACKEND MATCHES "Failed to create video audio stream\";[ \t\r\n]+unload\\(\\);")
    message(FATAL_ERROR "Video safety guard failed: audio-stream partial load must release allocated texture resources")
endif()
if(NOT VIDEO_MANAGER MATCHES "inst\\.backend->unload\\(\\);[ \t\r\n]+inst\\.backend\\.reset\\(\\);")
    message(FATAL_ERROR "Video safety guard failed: failed load entries must release backend resources before retaining error state")
endif()

message(STATUS "Video safety guard passed: failed-load cleanup and looping thread state are hardened.")
