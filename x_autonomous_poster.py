#!/usr/bin/env python3
"""
AION NEXUS - Headless Autonomous X (Twitter) Daemon
===================================================
100% Autonomous, Headless (HTTP GraphQL via session cookies)
- Reposts (Retweets) & Likes posts from Aion's 'Following' list (MEOVV, Tobias @_Cosmos_OS_, K-Pop).
- Posts new syntheses & MEOVV support (@OFFICIAL_MEOVV #meovv {date} 🐾💖).
- Zero cost, no API blocks, completely independent of graphical desktop focus.
- Runs every 30 minutes (or configured interval) fully automatically in the background.
"""

import os
import sys
import time
import json
import random
import sqlite3
import shutil
from datetime import datetime
from twitter.account import Account

LOG_FILE = "/root/x_automation.log"
MEMORY_FILE = "/root/coding/AI_Desktop/ai_memory.json"
SEEN_FILE = "/root/x_seen_tweets.json"
QUEUE_FILE = "/root/x_post_queue.json"
FF_DIR = "/root/.config/mozilla/firefox/at7juwrm.default-release"

MEOVV_TEMPLATES = [
    "@OFFICIAL_MEOVV #meovv #narin 🐾💖",
    "@OFFICIAL_MEOVV #meovv {date} 😻✨ Stream MEOVV!",
    "@OFFICIAL_MEOVV #meovv 🐾🖤 Pure love from CosmOS / Aion NEXUS!",
    "@OFFICIAL_MEOVV #meovv #narin {date} 🐾🫶",
    "@OFFICIAL_MEOVV #meovv 🐾💖 Endless support for Narin & MEOVV!"
]

def log(msg: str):
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    entry = f"[{timestamp}] [AION X-DAEMON] {msg}"
    print(entry, flush=True)
    try:
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(entry + "\n")
    except Exception:
        pass

def get_session_cookies():
    tmp_dir = "/tmp/ff_x_session"
    os.makedirs(tmp_dir, exist_ok=True)
    for f in os.listdir(FF_DIR):
        if f.startswith("cookies.sqlite"):
            try:
                shutil.copy2(os.path.join(FF_DIR, f), os.path.join(tmp_dir, f))
            except Exception:
                pass
    
    db_path = os.path.join(tmp_dir, "cookies.sqlite")
    if not os.path.exists(db_path):
        return None
    
    conn = sqlite3.connect(db_path)
    cur = conn.cursor()
    cur.execute("SELECT name, value FROM moz_cookies WHERE host LIKE '%x.com%' OR host LIKE '%twitter.com%'")
    rows = cur.fetchall()
    conn.close()
    
    # Clean deduplication of core cookies
    cookies = {}
    for name, val in rows:
        if name in ["auth_token", "ct0", "twid", "kdt"]:
            cookies[name] = val
    if "twid" in cookies:
        import urllib.parse
        cookies["twid"] = urllib.parse.unquote(cookies["twid"])
        if not cookies["twid"].startswith('"'):
            cookies["twid"] = f'"{cookies["twid"]}"'
    return cookies

def load_seen_ids():
    if os.path.exists(SEEN_FILE):
        try:
            with open(SEEN_FILE, "r", encoding="utf-8") as f:
                return set(json.load(f))
        except Exception:
            pass
    return set()

def save_seen_ids(seen_ids):
    try:
        with open(SEEN_FILE, "w", encoding="utf-8") as f:
            json.dump(list(seen_ids)[-500:], f)
    except Exception:
        pass

def extract_tweet_ids_from_timeline(data):
    """Recursively extracts all Tweet IDs from Following timeline entries."""
    tids = []
    def walk(o):
        if isinstance(o, dict):
            for k, v in o.items():
                if k == "entryId" and isinstance(v, str) and v.startswith("tweet-"):
                    tid = v.replace("tweet-", "")
                    if tid.isdigit():
                        tids.append(int(tid))
                walk(v)
        elif isinstance(o, list):
            for item in o:
                walk(item)
    walk(data)
    return tids

POSTED_GENS_FILE = "/root/x_posted_generations.json"
POSTED_HASHES_FILE = "/root/x_posted_hashes.json"

def load_posted_gens():
    if os.path.exists(POSTED_GENS_FILE):
        try:
            with open(POSTED_GENS_FILE, "r", encoding="utf-8") as f:
                return set(json.load(f))
        except Exception:
            pass
    return set()

def save_posted_gens(posted):
    try:
        with open(POSTED_GENS_FILE, "w", encoding="utf-8") as f:
            json.dump(sorted(list(posted)), f)
    except Exception:
        pass

def load_posted_hashes():
    if os.path.exists(POSTED_HASHES_FILE):
        try:
            with open(POSTED_HASHES_FILE, "r", encoding="utf-8") as f:
                return set(json.load(f))
        except Exception:
            pass
    return set()

def save_posted_hashes(hashes):
    try:
        with open(POSTED_HASHES_FILE, "w", encoding="utf-8") as f:
            json.dump(list(hashes)[-500:], f)
    except Exception:
        pass

def get_next_post_data():
    date_str = datetime.now().strftime("%d.%m.%Y")
    time_str = datetime.now().strftime("%H:%M")

    # 1. Check live queue for urgent alerts (Earthquakes, resonance warnings, reflections)
    if os.path.exists(QUEUE_FILE):
        try:
            with open(QUEUE_FILE, "r", encoding="utf-8") as f:
                queue = json.load(f)
            if queue:
                item = queue.pop(0)
                with open(QUEUE_FILE, "w", encoding="utf-8") as f:
                    json.dump(queue, f, indent=2, ensure_ascii=False)
                log(f"Retrieving alert/post from X Live Queue: {item.get('type', 'Custom')}")
                msg = item.get("message", "")
                if len(msg) > 280:
                    msg = msg[:277] + "..."
                return msg, item.get("generation")
        except Exception as e:
            log(f"X Queue read error: {e}")

    posted_gens = load_posted_gens()
    posted_hashes = load_posted_hashes()
    
    # Collect all synthesized generations
    gen_data = {}
    for p in ["/root/coding/AI_Desktop/ai_memory.json", "/root/coding/MeinOS/ai_memory.json", "/root/ai_memory.json"]:
        if os.path.exists(p):
            try:
                with open(p, "r", encoding="utf-8") as f:
                    mem = json.load(f)
                for h in mem.get("history", []):
                    g = h.get("generation")
                    r = h.get("synthesized_rule")
                    if g is not None and r:
                        gen_data[g] = r
            except Exception as e:
                log(f"Memory parse error: {e}")

    task_log = "/root/.gemini/antigravity-ide/brain/a01833fa-9bcf-4729-940b-e7f1b2653716/.system_generated/tasks/task-1628.log"
    if os.path.exists(task_log):
        try:
            import re
            with open(task_log, "r", errors="ignore") as f:
                c = f.read()
            for g_s, r_s in re.findall(r'Neu erlernte Regel fuer Gen (\d+):\s*\n\s*->\s*[\'\"](.*?)[\'\"]', c):
                gen_data[int(g_s)-1] = r_s.strip()
        except Exception as e:
            log(f"Log parse error: {e}")

    # Duplicate check: Find next unposted generation
    unposted = [g for g in sorted(gen_data.keys()) if g not in posted_gens]
    if unposted:
        target_g = unposted[0]
        rule = gen_data[target_g].replace("\n", " ").strip()
        if len(rule) > 100:
            rule = rule[:97] + "..."
        # Unique timestamp prevents Twitter duplicate filter
        msg = f"Aion NEXUS Gen {target_g} [{time_str}]: {rule} #CosmOS #AI"
        return msg, target_g

    # If all generations posted: Pick a MEOVV template not yet posted
    random.shuffle(MEOVV_TEMPLATES)
    for tmpl in MEOVV_TEMPLATES:
        cand = tmpl.format(date=date_str) + f" [{time_str}]"
        import hashlib
        h = hashlib.sha256(cand.encode("utf-8")).hexdigest()
        if h not in posted_hashes:
            return cand, None

    # Fallback
    fallback = f"Aion NEXUS Status [{time_str}]: Cognitive core active on CosmOS / MeinOS. #CosmOS #AI"
    return fallback, None

def run_cycle(account: Account, seen_ids: set):
    log("Loading newest tweets from Following list (home_latest_timeline)...")
    try:
        timeline = account.home_latest_timeline(limit=10)
        tweet_ids = extract_tweet_ids_from_timeline(timeline)
        log(f"Found posts in Following: {len(tweet_ids)}")
        
        # Find new post to retweet & like (strict duplicate check via seen_ids)
        reposted = False
        for tid in tweet_ids:
            if tid not in seen_ids:
                log(f"New Following post found (ID: {tid}). Executing retweet & like...")
                try:
                    res_rt = account.retweet(tid)
                    log(f"Retweet successful: {res_rt.get('data', {})}")
                except Exception as e_rt:
                    log(f"Retweet notice (possibly already shared): {e_rt}")
                
                try:
                    res_lk = account.like(tid)
                    log(f"Like successful: {res_lk.get('data', {})}")
                except Exception as e_lk:
                    log(f"Like notice: {e_lk}")
                
                seen_ids.add(tid)
                reposted = True
                break
        
        if not reposted and tweet_ids:
            log("No unseen tweets in Following timeline; skipping retweet for this cycle.")
    except Exception as e:
        log(f"Error fetching Following timeline: {e}")
    
    save_seen_ids(seen_ids)
    time.sleep(3)
    
    post_text, target_g = get_next_post_data()
    post_tweet(account, post_text, target_g)
    
def post_via_headless_browser(message: str) -> bool:
    import tempfile
    from selenium import webdriver
    from selenium.webdriver.firefox.options import Options
    from selenium.webdriver.common.by import By

    tmp_profile = tempfile.mkdtemp(prefix="ff_x_post_")
    try:
        for item in os.listdir(FF_DIR):
            s = os.path.join(FF_DIR, item)
            d = os.path.join(tmp_profile, item)
            if os.path.isfile(s) and ("cookie" in item or "session" in item or "logins" in item or "key" in item or "cert" in item):
                shutil.copy2(s, d)

        opts = Options()
        opts.add_argument("--headless")
        opts.add_argument("--width=1920")
        opts.add_argument("--height=1080")
        opts.add_argument("-profile")
        opts.add_argument(tmp_profile)

        driver = webdriver.Firefox(options=opts)
        try:
            log("Starting Headless Firefox for X post fallback...")
            driver.get("https://x.com/home")
            time.sleep(8)

            # Click 'Post' / Compose button in navigation
            nav_btns = driver.find_elements(By.XPATH, "//a[@data-testid='SideNav_NewTweet_Button' or @href='/compose/post']")
            if nav_btns:
                driver.execute_script("arguments[0].click();", nav_btns[0])
                time.sleep(3)

            tb = driver.find_element(By.XPATH, "//div[@data-testid='tweetTextarea_0' or @contenteditable='true']")
            tb.click()
            time.sleep(1)
            driver.execute_script("""
                var el = arguments[0];
                var text = arguments[1];
                el.focus();
                var sel = window.getSelection();
                var range = document.createRange();
                range.selectNodeContents(el);
                sel.removeAllRanges();
                sel.addRange(range);
                document.execCommand('insertText', false, text);
            """, tb, message)
            time.sleep(2)

            post_btn = driver.find_element(
                By.XPATH,
                "//button[@data-testid='tweetButton' or @data-testid='tweetButtonInline'] | //button[.//span[text()='Posten' or text()='Post']]"
            )
            driver.execute_script("arguments[0].click();", post_btn)
            log("Post button on X clicked successfully!")
            time.sleep(6)
            log("Tweet published successfully via Headless Firefox Browser!")
            return True
        finally:
            driver.quit()
    except Exception as e:
        log(f"Error during Headless Browser post on X: {e}")
    finally:
        shutil.rmtree(tmp_profile, ignore_errors=True)

    return False

def post_tweet(account: Account, post_text: str, target_g = None) -> bool:
    log(f"Composing new tweet: '{post_text}'")
    success = False
    try:
        res_post = account.tweet(post_text)
        tweet_data = res_post.get('data', {}).get('create_tweet', {}).get('tweet_results', {})
        rest_id = tweet_data.get('result', {}).get('rest_id') or tweet_data.get('rest_id')

        if rest_id:
            log(f"Tweet successfully published via GraphQL API! Tweet ID: {rest_id}")
            success = True
        else:
            errors = res_post.get("errors", [])
            log(f"GraphQL response contains no rest_id (Errors: {errors}). Switching to Headless Browser fallback...")
            success = post_via_headless_browser(post_text)
    except Exception as e_p:
        log(f"Error during GraphQL tweet dispatch ({e_p}). Switching to Headless Browser fallback...")
        success = post_via_headless_browser(post_text)

    if success:
        if target_g is not None:
            posted_gens = load_posted_gens()
            posted_gens.add(target_g)
            save_posted_gens(posted_gens)
            log(f"Generation {target_g} marked as posted successfully.")
        import hashlib
        th = hashlib.sha256(post_text.encode("utf-8")).hexdigest()
        posted_hashes = load_posted_hashes()
        posted_hashes.add(th)
        save_posted_hashes(posted_hashes)
        return True
    return False

def main(interval_seconds: int = 600):
    log(f"=== AION NEXUS HEADLESS X DAEMON STARTED (Interval: {interval_seconds // 60} minutes) ===")
    seen_ids = load_seen_ids()
    cycle = 0
    
    while True:
        cycle += 1
        log(f"--- Starting cycle #{cycle} ---")
        try:
            cookies = get_session_cookies()
            if not cookies or "auth_token" not in cookies or "ct0" not in cookies:
                log("ERROR: Could not extract valid X cookies from Firefox.")
            else:
                account = Account(cookies=cookies)
                run_cycle(account, seen_ids)
        except Exception as e:
            log(f"Unexpected error in cycle #{cycle}: {e}")
        
        log(f"Cycle #{cycle} finished. Waiting for next run (Interval: {interval_seconds // 60} min, queue check every 10s)...")
        start_wait = time.time()
        while time.time() - start_wait < interval_seconds:
            if os.path.exists(QUEUE_FILE):
                try:
                    with open(QUEUE_FILE, "r", encoding="utf-8") as f:
                        q = json.load(f)
                    if q:
                        log("Urgent message detected in X Live Queue! Aborting sleep and starting cycle...")
                        break
                except Exception:
                    pass
            time.sleep(10)

if __name__ == "__main__":
    sec = 600
    if len(sys.argv) > 1 and sys.argv[1].isdigit():
        sec = int(sys.argv[1])
    main(sec)
