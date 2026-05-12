from __future__ import annotations

from dataclasses import dataclass, field
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


def _landmark_list(values: Iterable[object]) -> list[Landmark]:
    result: list[Landmark] = []
    for value in values:
        result.append(Landmark(float(value.x), float(value.y), float(value.z)))
    while len(result) < LANDMARK_COUNT:
        result.append(Landmark())
    return result[:LANDMARK_COUNT]


def hand_from_result(result: object, index: int) -> HandResult:
    hand = HandResult()
    hand.valid = True
    handedness_categories = result.handedness[index] if index < len(result.handedness) else []
    gesture_categories = result.gestures[index] if index < len(result.gestures) else []
    if handedness_categories:
        top = handedness_categories[0]
        hand.handedness = handedness_id(top.category_name)
        hand.handedness_score = float(top.score)
    if gesture_categories:
        top = gesture_categories[0]
        hand.gesture = gesture_id(top.category_name)
        hand.gesture_score = float(top.score)
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
    hand_count = min(len(hands), MAX_HANDS)
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
        hand = hands[index] if index < hand_count else HandResult()
        HAND_HEADER_STRUCT.pack_into(
            payload,
            offset,
            1 if hand.valid else 0,
            hand.handedness,
            hand.gesture,
            0,
            hand.handedness_score,
            hand.gesture_score,
        )
        offset += HAND_HEADER_STRUCT.size
        image = hand.image_landmarks or [Landmark() for _ in range(LANDMARK_COUNT)]
        world = hand.world_landmarks or [Landmark() for _ in range(LANDMARK_COUNT)]
        for landmark in image[:LANDMARK_COUNT]:
            LANDMARK_STRUCT.pack_into(payload, offset, landmark.x, landmark.y, landmark.z)
            offset += LANDMARK_STRUCT.size
        for landmark in world[:LANDMARK_COUNT]:
            LANDMARK_STRUCT.pack_into(payload, offset, landmark.x, landmark.y, landmark.z)
            offset += LANDMARK_STRUCT.size
    return bytes(payload)


def pack_preview(sequence: int, jpeg: bytes) -> bytes:
    return PREVIEW_HEADER_STRUCT.pack(b"MJPG", sequence, len(jpeg)) + jpeg
