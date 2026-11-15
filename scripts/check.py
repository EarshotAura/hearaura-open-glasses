#!/usr/bin/env python3
"""Checks every recipe and rebuilds recipes/index.json, the one file the
guide at hearaura.app/build-glasses reads.

    python3 scripts/check.py            check, and rewrite recipes/index.json
    python3 scripts/check.py --check    check only; exit 1 if anything is wrong
                                        or index.json is out of date

Run it before sending a recipe upstream. It needs nothing but Python 3.
"""
import json, pathlib, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
STATUS = ["idea", "designed", "partly built", "built"]
SOLDERING = ["none", "last step", "some", "yes"]
GIVES = ["captions", "see-through", "two-eye-display", "head-direction", "voice-direction", "distance", "captions-by-speaker"]
LESSONS = ["parts", "wake", "screen", "cord", "float", "plug", "look", "ears", "eye", "wear",
           "strip", "stripwake", "stripears", "striplook", "stripwear", "row", "rowwear", "rowglasses"]

def main():
    parts = json.loads((ROOT / "parts.json").read_text())
    problems, recipes = [], []
    for part, entry in parts.items():
        if not all(key in entry for key in ("name", "what", "price")):
            problems.append(f"parts.json: {part} needs name, what and price")
        for entry in entry.get("buy", []):
            if not entry.get("shop") or not str(entry.get("url", "")).startswith("https://"):
                problems.append(f"parts.json: {part} has a \"buy\" entry without a shop or an https link")
    for folder in sorted((ROOT / "recipes").iterdir()):
        if not folder.is_dir() or folder.name.startswith("_"):
            continue
        file = folder / "recipe.json"
        if not file.exists():
            problems.append(f"{folder.name}: no recipe.json")
            continue
        try:
            recipe = json.loads(file.read_text())
        except json.JSONDecodeError as error:
            problems.append(f"{folder.name}: recipe.json is not valid JSON ({error})")
            continue
        say = lambda text: problems.append(f"{folder.name}: {text}")
        for key in ("id", "name", "summary", "status", "by", "soldering", "gives", "parts", "optional", "lessons", "notes", "builds"):
            if key not in recipe:
                say(f"missing \"{key}\"")
        if recipe.get("id") != folder.name:
            say(f"\"id\" must be the folder name, {folder.name}")
        if recipe.get("status") not in STATUS:
            say(f"\"status\" must be one of {STATUS}")
        if recipe.get("soldering") not in SOLDERING:
            say(f"\"soldering\" must be one of {SOLDERING}")
        for part in recipe.get("parts", []) + recipe.get("optional", []):
            if part not in parts:
                say(f"part \"{part}\" is not in parts.json (add it there)")
        for give in recipe.get("gives", []):
            if give not in GIVES:
                say(f"\"gives\" has \"{give}\"; known ones are {GIVES}")
        for lesson in recipe.get("lessons", []):
            if lesson not in LESSONS:
                say(f"lesson \"{lesson}\" is not one the guide has: {LESSONS}")
        if recipe.get("status") == "built" and not recipe.get("builds"):
            say("status is \"built\" but \"builds\" is empty: add who built it and when")
        recipes.append(recipe)
    index = json.dumps({"parts": parts, "recipes": recipes}, indent=1, ensure_ascii=False) + "\n"
    target = ROOT / "recipes" / "index.json"
    stale = not target.exists() or target.read_text() != index
    if "--check" in sys.argv:
        if stale:
            problems.append("recipes/index.json is out of date: run python3 scripts/check.py")
    elif not problems:
        target.write_text(index)
    for line in problems:
        print("PROBLEM  " + line)
    print(f"{len(recipes)} recipes, {len(parts)} parts, {len(problems)} problems")
    sys.exit(1 if problems else 0)

if __name__ == "__main__":
    main()
