"""Prepare a new exhibition farm from the public harvest fixture, never a player save."""
import copy
import json
from pathlib import Path
import sys

base = Path(__file__).resolve().parents[2]
target = (base / sys.argv[1]).resolve()
if not target.is_relative_to((base / "generated").resolve()) or target.exists():
    raise SystemExit("Expected a new directory under generated/")
d = json.loads((base/"project/Resources/farm/experience/harvest.json").read_text(encoding="utf-8"))
old = d["tiles"]
tiles = [copy.deepcopy(old[0]) for _ in old]
for dest, source in [(10,6),(11,7),(14,8)]:
    tiles[dest] = copy.deepcopy(old[source])
    tiles[dest].update(height=0, moisture=.34, irrigationEnabled=False)
tiles[5].update(feature="WaterSource",waterAmount=1)
for i in (7,8,13,18):
    tiles[i].update(feature="Canal",waterAmount=0)
for i in (17,19):
    tiles[i].update(state="Tilled",moisture=.05,irrigationEnabled=True)
d["tiles"] = tiles
d["grid"]["selectedIndex"] = 2
d["economy"]["seedCounts"] = [3,6,6,6]
d["document"].update(id="farm_1795500000000_0",displayName="パーティクル体験畑")
(target/"saves").mkdir(parents=True)
for path, obj in [(target/"saves"/(d["document"]["id"]+".json"), d),
                  (target/"farm_documents.json", {"activeDocumentId":d["document"]["id"],"schemaVersion":1})]:
    path.write_text(json.dumps(obj,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
