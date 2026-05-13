from __future__ import annotations

from dataclasses import dataclass, field
import math
import struct
from typing import Iterable

MAGIC = b"BHTK"
VERSION = 1
MAX_HANDS = 2
LANDMARK_COUNT = 21

HAND_UNKNOWN = 0
HAND_LEFT = 1
HAND_RIGHT = 2

GESTURE_UNKNOWN = 0
GESTURE_NONE = 1
GESTURE_CLOSED_FIST = 2
GESTURE_OPEN_PALM = 3
GESTURE_POINTING_UP = 4
GESTURE_THUMB_DOWN = 5
GESTURE_THUMB_UP = 6
GESTURE_VICTORY = 7
GESTURE_I_LOVE_YOU = 8

HEADER_STRUCT = struct.Struct("<4sHHQQHHBBH")
HAND_HEADER_STRUCT = struct.Struct("<BBBBff")
LANDMARK_STRUCT = struct.Struct("<fff")
PREVIEW_HEADER_STRUCT = struct.Struct("<4sQI")
PACKET_SIZE = HEADER_STRUCT.size + MAX_HANDS * (
    HAND_HEADER_STRUCT.size + LANDMARK_COUNT * 2 * LANDMARK_STRUCT.size
)

GESTURE_MAP = {
    "None": GESTURE_NONE,
    "Closed_Fist": GESTURE_CLOSED_FIST,
    "Open_Palm": GESTURE_OPEN_PALM,
    "Pointing_Up": GESTURE_POINTING_UP,
    "Thumb_Down": GESTURE_THUMB_DOWN,
    "Thumb_Up": GESTURE_THUMB_UP,
    "Victory": GESTURE_VICTORY,
    "ILoveYou": GESTURE_I_LOVE_YOU,
}


@dataclass
class Landmark:
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0


@dataclass
class HandResult:
    valid: bool = False
    handedness: int = HAND_UNKNOWN
    gesture: int = GESTURE_UNKNOWN
    handedness_score: float = 0.0
    gesture_score: float = 0.0
    image_landmarks: list[Landmark] = field(default_factory=list)
    world_landmarks: list[Landmark] = field(default_factory=list)


def handedness_id(name: str) -> int:
    if name == "Left":
        return HAND_LEFT
    if name == "Right":
        return HAND_RIGHT
    return HAND_UNKNOWN


def gesture_id(name: str) -> int:
    return GESTURE_MAP.get(name, GESTURE_UNKNOWN)


def _finite_float(value: object) -> float:
    try:
        result = float(value)
    except (TypeError, ValueError):
        return 0.0
    return result if math.isfinite(result) else 0.0


def _score(value: object) -> float:
    return max(0.0, min(1.0, _finite_float(value)))


def _landmark_list(values: Iterable[object]) -> list[Landmark]:
    result: list[Landmark] = []
    for value in values:
        result.append(Landmark(_finite_float(value.x), _finite_float(value.y), _finite_float(value.z)))
    while len(result) < LANDMARK_COUNT:
        result.append(Landmark())
    return result[:LANDMARK_COUNT]


def hand_from_result(result: object, index: int, gesture_score_threshold: float = 0.0) -> HandResult:
    hand = HandResult()
    hand.valid = True
    handedness_categories = result.handedness[index] if index < len(result.handedness) else []
    gesture_categories = result.gestures[index] if index < len(result.gestures) else []
    if handedness_categories:
        top = handedness_categories[0]
        hand.handedness = handedness_id(top.category_name)
        hand.handedness_score = _score(top.score)
    if gesture_categories:
        top = gesture_categories[0]
        hand.gesture_score = _score(top.score)
        hand.gesture = gesture_id(top.category_name) if hand.gesture_score >= gesture_score_threshold else GESTURE_UNKNOWN
    if index < len(result.hand_landmarks):
        hand.image_landmarks = _landmark_list(result.hand_landmarks[index])
    if index < len(result.hand_world_landmarks):
        hand.world_landmarks = _landmark_list(result.hand_world_landmarks[index])
    return hand


def pack_frame(
    sequence: int,
    timestamp_ms: int,
    camera_width: int,
    camera_height: int,
    hands: list[HandResult],
) -> bytes:
    payload = bytearray(PACKET_SIZE)
    valid_hands = [hand for hand in hands if hand.valid][:MAX_HANDS]
    hand_count = len(valid_hands)
    HEADER_STRUCT.pack_into(
        payload,
        0,
        MAGIC,
        VERSION,
        HEADER_STRUCT.size,
        sequence,
        timestamp_ms,
        camera_width,
        camera_height,
        hand_count,
        0,
        0,
    )
    offset = HEADER_STRUCT.size
    for index in range(MAX_HANDS):
        hand = valid_hands[index] if index < hand_count else HandResult()
        HAND_HEADER_STRUCT.pack_into(
            payload,
            offset,
            1 if hand.valid else 0,
            hand.handedness,
            hand.gesture,
            0,
            _score(hand.handedness_score),
            _score(hand.gesture_score),
        )
        offset += HAND_HEADER_STRUCT.size
        image = _fixed_landmarks(hand.image_landmarks)
        world = _fixed_landmarks(hand.world_landmarks)
        for landmark in image:
            LANDMARK_STRUCT.pack_into(payload, offset, landmark.x, landmark.y, landmark.z)
            offset += LANDMARK_STRUCT.size
        for landmark in world:
            LANDMARK_STRUCT.pack_into(payload, offset, landmark.x, landmark.y, landmark.z)
            offset += LANDMARK_STRUCT.size
    return bytes(payload)


def _fixed_landmarks(values: list[Landmark]) -> list[Landmark]:
    fixed = [
        Landmark(_finite_float(landmark.x), _finite_float(landmark.y), _finite_float(landmark.z))
        for landmark in values[:LANDMARK_COUNT]
    ]
    while len(fixed) < LANDMARK_COUNT:
        fixed.append(Landmark())
    return fixed


def pack_preview(sequence: int, jpeg: bytes) -> bytes:
    return PREVIEW_HEADER_STRUCT.pack(b"MJPG", sequence, len(jpeg)) + jpeg
