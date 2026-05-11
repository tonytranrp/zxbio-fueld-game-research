# Data/event/video

Video playback events fired by `VideoManager`.

| Event | Meaning |
| --- | --- |
| `VideoStartedEvent` | `play()` accepted and playback began |
| `VideoCompletedEvent` | Video reaches end-of-file for non-looping playback |
| `VideoErrorEvent` | Decoder, file, or stream setup failed |

All events carry `videoName` (`std::string_view`); `VideoErrorEvent` also carries `errorMessage`.
