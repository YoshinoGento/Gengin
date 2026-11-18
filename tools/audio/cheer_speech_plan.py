"""Authoring-only pronunciation input; visible captions remain unchanged."""


def speech_plan(pair, side):
    if side not in ("L", "R"):
        raise ValueError("Unknown spectator side")
    overrides = pair.get("speech", {})
    if not isinstance(overrides, dict) or any(key not in ("L", "R") for key in overrides):
        raise ValueError("Unknown speech override side")
    if side not in overrides:
        return {"segments": ["".join(pair[side])], "pauseSeconds": 0.0}
    override = overrides[side]
    if not isinstance(override, dict) or set(override) != {"segments", "pauseSeconds"}:
        raise ValueError("Invalid speech override")
    segments, pause = override["segments"], override["pauseSeconds"]
    if not isinstance(segments, list) or not 1 <= len(segments) <= 3:
        raise ValueError("One to three performed phrases required")
    if any(not isinstance(s, str) or not s.strip() or len(s) > 80 for s in segments):
        raise ValueError("Invalid performed phrase")
    if isinstance(pause, bool) or not isinstance(pause, (int, float)) or not 0 <= pause <= .5:
        raise ValueError("Invalid phrase pause")
    if len(segments) == 1 and pause != 0:
        raise ValueError("Single phrase cannot have a gap")
    return {"segments": list(segments), "pauseSeconds": float(pause)}
