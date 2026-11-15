# Adding to this project

There are three useful things to send, from least effort to most.

## 1. Tell us you built one

Open a "Build report" issue: which recipe, what worked, what did not, and a
photo if you can. A recipe moves from "designed" to "built" on the first
report, and that is worth more than any new code.

## 2. Add a recipe

A recipe is a folder in `recipes/` with one file, `recipe.json`. Used
different parts, a different display, a different board? That is a recipe.

1. Copy `recipes/_template` to `recipes/your-recipe-name` (lower case, with
   hyphens).
2. Fill in `recipe.json`:

   | Field | What to put |
   |---|---|
   | `id` | The folder name |
   | `name`, `summary` | What a beginner would call it, and one or two sentences |
   | `status` | `idea`, `designed`, `partly built` or `built`. Be honest: "built" needs an entry in `builds` |
   | `by` | Your name or handle |
   | `soldering` | `none`, `last step`, `some` or `yes` |
   | `gives` | What a builder ends up with, from the list in `scripts/check.py`. The guide's chooser sorts recipes by these and by `soldering`, so be exact: `ahead-behind` only if it can tell a voice in front from one behind |
   | `works_with` | Optional. Leave it out if any host that speaks the link will do. List hosts only when the recipe truly needs one, as bought display glasses need HearAura on iPhone to draw on them |
   | `parts`, `optional` | Part ids from `parts.json`. Add new parts there, with a plain-words "what", a rough price, and a `buy` list (see below) |
   | `lessons` | Which of the guide's lessons apply, in order. Leave empty if none fit yet |
   | `notes` | What is different, what was hard, what is untested |
   | `builds` | `{"who": "...", "date": "2026-11-14", "result": "..."}` for each person who built it |

   A part's `buy` list says where to get it, best shop first. Give the exact
   product page when one maker sells it, and a search link when any brand
   will do:

   ```json
   "buy": [
    {"shop": "Adafruit", "url": "https://www.adafruit.com/product/4754"},
    {"shop": "Amazon", "url": "https://www.amazon.com/s?k=BNO085", "search": true}
   ]
   ```

   `python3 scripts/find_parts.py` then reads the price and stock from
   Adafruit and Seeed product pages and fills them in. The guide shows a
   buy button for each part and picks the first shop that has it. No
   affiliate or tracking links, please.

3. Run `python3 scripts/check.py`. It checks your recipe and rebuilds
   `recipes/index.json`. Commit both.
4. If your recipe needs the firmware changed, keep it working for the other
   recipes: the firmware finds its parts at start-up, so add a new part the
   same way instead of removing an old one. Run `cd firmware && pio run`.
5. Open a pull request. Say what you built and what you tested.

Photos, drawings and 3D-print files for your recipe go in its folder.

## 3. Keep your own version, and send back the parts worth sharing

You do not have to send everything back. If you fork this repository and
change it for your own glasses, the awkward part is staying current: every
release here would mean redoing your changes by hand.
[Graft](https://graftfork.com) does that part.

1. Fork this repository on GitHub and make your changes there. In each
   commit message, say why: Graft carries each change forward with its
   reason.
2. At https://graftfork.com/app, install Graft's GitHub app on your fork
   and add the fork. Graft works out that it comes from this repository.
3. Each time a release is tagged here, Graft moves your changes onto it.
   If every change lands, you get the moved copy. If one does not, your
   fork stays exactly where it was and Graft tells you which change
   clashed and why.
4. When one of your changes is worth sharing, such as a new recipe or a
   fix to the firmware, ask Graft to send it upstream. It prepares that one
   change on its own, on the current release, with the pull request text
   written from your reason. You open the pull request from your own
   account.

You can do all of this without Graft, with ordinary forks and pull
requests. Graft is for people who want to keep a version of their own for
a long time.

## Releases

This repository tags a release (`v0.1.0`, `v0.2.0`, ...) whenever the
firmware, the link or the recipe format changes in a way a fork would
notice. The guide at hearaura.app/build-glasses reads `recipes/index.json`
from the main branch, so a merged recipe appears there within minutes.

## Ground rules

- Write for a beginner. Plain words, short sentences, say what a part is.
- Say what you tested and what you did not. "Not tested" is a fine answer.
- No pictures or recordings of people who have not agreed to it.
- The glasses must never store or send camera pictures or sound.
