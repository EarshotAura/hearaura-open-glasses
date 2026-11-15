#!/usr/bin/env python3
"""Looks up every part in parts.json at the shops it lists, and writes back
what it finds: today's price, whether it is in stock, and the date.

    python3 scripts/find_parts.py          look up, update parts.json and the index
    python3 scripts/find_parts.py --dry    look up and print; change nothing

Each part has a "buy" list, best shop first. An entry is one of two kinds:

  a product page   {"shop": "Adafruit", "url": "https://www.adafruit.com/product/4754"}
                   The script reads its price and stock.
  a search         {"shop": "Amazon", "url": "https://www.amazon.com/s?k=...", "search": true}
                   For parts with no single maker (a lens, a breadboard).
                   The script leaves these alone: those shops turn scripts away,
                   and a search has no one price.

Shops it can read: Adafruit (their public product API) and Seeed (the
price a product page publishes for search engines). To teach it another
shop, add a function to READERS.

A part's "price" becomes the price at the first shop that has it in stock,
rounded up to a dollar. If a lookup fails, the old numbers stay and the
script says so; it never guesses. It needs nothing but Python 3.
"""
import datetime, json, math, pathlib, re, subprocess, sys, time, urllib.error, urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
AGENT = "Mozilla/5.0 (compatible; open-glasses-parts-check)"

def fetch(url, tries=2):
    for attempt in range(tries):
        try:
            request = urllib.request.Request(url, headers={"User-Agent": AGENT, "Accept": "text/html,application/json"})
            with urllib.request.urlopen(request, timeout=30) as reply:
                return reply.read().decode("utf-8", "replace")
        except urllib.error.HTTPError as error:
            if error.code == 429 and attempt + 1 < tries:
                time.sleep(45)                  # asked to slow down
                continue
            raise

def adafruit(url):
    number = re.search(r"/product/(\d+)", url).group(1)
    product = json.loads(fetch(f"https://www.adafruit.com/api/product/{number}"))
    return float(product["product_price"]), str(product.get("product_stock", "")).strip().lower() not in ("0", "", "out of stock")

def seeed(url, again=True):
    time.sleep(12)                              # Seeed turns away quick repeat visits
    page = fetch(url)
    if "application/ld+json" not in page and again:
        time.sleep(45)                          # a "please wait" page, not the product
        return seeed(url, again=False)
    for block in re.findall(r"<script[^>]*application/ld\+json[^>]*>(.*?)</script>", page, re.S):
        try:
            data = json.loads(block, strict=False)   # some pages carry stray line breaks
        except json.JSONDecodeError:
            continue
        for item in data if isinstance(data, list) else [data]:
            if isinstance(item, dict) and item.get("@type") == "Product":
                offer = item.get("offers") or {}
                # The high price is for one piece; the low one needs a bulk order.
                price = offer.get("highPrice") or offer.get("price")
                singles = offer.get("offers") or [offer]
                stock = any("InStock" in str(one.get("availability", "")) for one in singles)
                if price:
                    return float(price), stock
    raise ValueError("no price found on the page")

READERS = {"www.adafruit.com": adafruit, "www.seeedstudio.com": seeed}

def main():
    dry = "--dry" in sys.argv
    file = ROOT / "parts.json"
    parts = json.loads(file.read_text())
    today = datetime.date.today().isoformat()
    failed = 0
    for name, part in parts.items():
        for entry in part.get("buy", []):
            if entry.get("search"):
                continue
            host = entry["url"].split("/")[2]
            reader = READERS.get(host)
            if not reader:
                print(f"  {name}: no reader for {host}; left alone")
                continue
            try:
                price, stock = reader(entry["url"])
            except Exception as error:
                failed += 1
                print(f"  {name}: could not read {entry['shop']} ({error}); kept the old numbers")
                continue
            entry.update(price=price, stock=stock, checked=today)
            print(f"  {name}: {entry['shop']} ${price:.2f}, {'in stock' if stock else 'OUT OF STOCK'}")
        known = [e for e in part.get("buy", []) if "price" in e]
        best = next((e for e in known if e.get("stock")), known[0] if known else None)
        if best:
            part["price"] = math.ceil(best["price"])
    if dry:
        print("dry run: nothing written")
    else:
        file.write_text(json.dumps(parts, indent=1, ensure_ascii=False) + "\n")
        subprocess.run([sys.executable, str(ROOT / "scripts" / "check.py")], check=False)
    print(f"{failed} lookups failed" if failed else "every lookup worked")
    sys.exit(1 if failed else 0)

if __name__ == "__main__":
    main()
