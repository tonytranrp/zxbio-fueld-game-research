if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()

set(HAND_TRACKING_CPP "${SOURCE_DIR}/src/engine/vision/hand_tracking/HandTrackingService.cpp")
if(NOT EXISTS "${HAND_TRACKING_CPP}")
    message(FATAL_ERROR "Hand-tracking safety guard failed: service source is missing")
endif()
file(READ "${HAND_TRACKING_CPP}" HAND_TRACKING)

if(NOT HAND_TRACKING MATCHES "address_v4::loopback\\(\\)")
    message(FATAL_ERROR "Hand-tracking safety guard failed: UDP receiver must bind to loopback")
endif()
if(NOT HAND_TRACKING MATCHES "sender\\.address\\(\\)\\.is_loopback\\(\\)")
    message(FATAL_ERROR "Hand-tracking safety guard failed: UDP receiver must reject non-loopback senders")
endif()

string(REGEX MATCHALL "std::scoped_lock lock\\(m_mutex\\);[^}]*Events::publish" LOCKED_PUBLISHES "${HAND_TRACKING}")
if(LOCKED_PUBLISHES)
    message(FATAL_ERROR "Hand-tracking safety guard failed: Events::publish appears in a direct m_mutex scoped-lock block")
endif()

message(STATUS "Hand-tracking safety guard passed: publishes are outside direct mutex blocks and UDP is loopback-restricted.")
