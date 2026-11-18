"""Validate authoring input separately from audible pronunciation acceptance."""
import copy
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/audio"))
from cheer_speech_plan import speech_plan


class SpeechPlanTest(unittest.TestCase):
    def test_default_and_copy(self):
        pair = {"L":["one", "two"], "R":["response"]}
        self.assertEqual(speech_plan(pair,"L"), {"segments":["onetwo"],"pauseSeconds":0})
        pair["speech"] = {"R":{"segments":["a","b"],"pauseSeconds":.24}}
        result = speech_plan(pair,"R")
        result["segments"][0] = "changed"
        self.assertEqual(pair["speech"]["R"]["segments"][0], "a")

    def test_invalid(self):
        for segments, pause in [([],0),(["a"]*4,.2),([""],0),(["a"],.2),(["a","b"],-.1),
                                (["a","b"],.6),(["a","b"],float("nan")),(["a","b"],True)]:
            with self.assertRaises(ValueError):
                speech_plan({"speech":{"L":{"segments":segments,"pauseSeconds":pause}}},"L")
        with self.assertRaises(ValueError):
            speech_plan({},"X")
        with self.assertRaises(ValueError):
            speech_plan({"speech":{"wrong":{}}},"L")
        with self.assertRaises(ValueError):
            speech_plan({"speech":[]},"L")
        with self.assertRaises(ValueError):
            speech_plan({"speech":{"L":None}},"L")

    def test_authored_readings(self):
        data = json.loads((ROOT / "tools/audio/contest_cheers.json").read_text(encoding="utf-8"))
        before = copy.deepcopy(data)
        pairs = {p["id"]:p for p in data["pairs"]}
        self.assertIn("はたけ", "".join(speech_plan(pairs["Perfect"],"R")["segments"]))
        self.assertIn("ひとたば", "".join(speech_plan(pairs["CarrotBundle"],"L")["segments"]))
        self.assertIn("だて", "".join(speech_plan(pairs["TurnipStar"],"R")["segments"]))
        soil = speech_plan(pairs["NutrientSoil"],"R")
        self.assertEqual(soil["segments"][0], "そのつち！")
        self.assertEqual(soil["pauseSeconds"], .24)
        self.assertNotIn("speech",pairs["PumpkinPot"])
        for pair in data["pairs"]:
            for side in ("L","R"):
                speech_plan(pair,side)
        self.assertEqual(data,before)


if __name__ == "__main__":
    unittest.main()
