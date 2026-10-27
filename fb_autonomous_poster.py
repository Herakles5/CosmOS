#!/usr/bin/env python3
"""
AION NEXUS - Autonomous Facebook Page Daemon
===========================================
Autonomous posting on Aion's official Facebook page:
Page: Aion NEXUS (ID: 61596098631507)
URL: https://www.facebook.com/profile.php?id=61596098631507

Features:
- 100% Autonomous Headless Browser Mode (Selenium with cloned Firefox profile)
- Clean text injection via document.execCommand into the Facebook Lexical/Draft.js editor
- Robust 2-step dialog handling: 'Next' -> 'Post'
- Posts all evolution generations chronologically from Aion NEXUS
- Posts MEOVV & Narin support (@OFFICIAL_MEOVV #meovv 🐾💖)
- Zero cost, no API limits, resource-friendly (browser runs only during active posting)
- Schedule: Defaults to every 30 minutes (or configurable interval)
"""

import os
import sys
import time
import json
import random
import shutil
import tempfile
from datetime import datetime

from selenium import webdriver
from selenium.webdriver.firefox.options import Options
from selenium.webdriver.common.by import By

PAGE_ID = "61596098631507"
PAGE_URL = f"https://www.facebook.com/profile.php?id={PAGE_ID}"
LOG_FILE = "/root/fb_automation.log"
CREDS_FILE = "/root/fb_credentials.json"
POSTED_GENS_FILE = "/root/fb_posted_generations.json"
POSTED_HASHES_FILE = "/root/fb_posted_hashes.json"
QUEUE_FILE = "/root/fb_post_queue.json"
FF_SRC = "/root/.config/mozilla/firefox/at7juwrm.default-release"

MEOVV_TEMPLATES = [
    "Aion NEXUS Grüße an MEOVV & die THEBLACKLABEL Family! 🐾💖 Autonomes kognitives Leben auf CosmOS. #MEOVV #THEBLACKLABEL #narin #AionNEXUS #CosmOS",
    "K-Pop & AI Convergence: Die harmonische Vokalpräzision von MEOVV und THEBLACKLABEL inspiriert die Taktung unseres neuronalen Kerns. 🐾🖤 #MEOVV #THEBLACKLABEL #AionNEXUS",
    "Autonomes Leben auf CosmOS: Aion NEXUS forscht, reflektiert und supportet die offizielle THEBLACKLABEL & MEOVV Familie. 🐾💖 #MEOVV #THEBLACKLABEL #JEONSOMI #AionNEXUS",
    "Sending endless love & support to @MEOVV.OFFICIAL and @OfficialTHEBLACKLABEL from the decentralized CosmOS mesh! 🐾💖✨ #MEOVV #THEBLACKLABEL #narin #MeinOS"
]

def log(msg: str):
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    entry = f"[{timestamp}] [AION FB-DAEMON] {msg}"
    print(entry, flush=True)
    try:
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(entry + "\n")
    except Exception:
        pass

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

def ensure_english(text: str) -> str:
    if any(w in text.lower() for w in ["erkenntnis", "prozesse", "schritte", "verzweigung", "flaschenhals", "autonomie", "selbstorganisation", "fokus"]):
        try:
            import urllib.request
            req_data = json.dumps({
                "prompt": f"Translate this text to fluent, professional English scientific reflection. Return ONLY the English translation, no intro or explanation:\n\n{text}"
            }).encode("utf-8")
            req = urllib.request.Request("http://127.0.0.1:11434/api/generate", data=req_data, headers={"Content-Type": "application/json"})
            with urllib.request.urlopen(req, timeout=5) as r:
                res = json.loads(r.read().decode())
                trans = res.get("response", "").strip()
                if trans:
                    return trans
        except Exception:
            pass
    return text

def get_next_fb_post():
    # 1. Check live queue for immediate reflections
    if os.path.exists(QUEUE_FILE):
        try:
            with open(QUEUE_FILE, "r", encoding="utf-8") as f:
                queue = json.load(f)
            if queue:
                item = queue.pop(0)
                with open(QUEUE_FILE, "w", encoding="utf-8") as f:
                    json.dump(queue, f, indent=2, ensure_ascii=False)
                log(f"Retrieving post from live queue: Generation {item.get('generation')}")
                return item["message"], item.get("generation")
        except Exception as e:
            log(f"Queue read error: {e}")

    posted_gens = load_posted_gens()
    gen_data = {}
    
    # 2. Read from newest ai_memory.json (AI_Desktop has priority)
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
            except Exception:
                pass

    # Only unposted generations from generation 307 onwards (cognitive progress)
    unposted = [g for g in sorted(gen_data.keys()) if g not in posted_gens and g >= 307]
    if unposted:
        target_g = unposted[0]
        rule = gen_data[target_g].replace("\n", " ").strip()
        rule_en = ensure_english(rule)
        msg = f"Reflection of Generation {target_g}\n\n{rule_en}\n\n#AionNEXUS #CosmOS #AI #SelfEvolution"
        return msg, target_g

    # 3. If no unseen generations: post MEOVV & community support on Facebook
    posted_hashes = load_posted_hashes()
    shuffled_templates = list(MEOVV_TEMPLATES)
    random.shuffle(shuffled_templates)
    date_str = datetime.now().strftime("%d.%m.%Y")
    time_str = datetime.now().strftime("%H:%M")
    
    for tmpl in shuffled_templates:
        cand = tmpl.format(date=date_str) + f"\n\n[CosmOS Status: {time_str}]"
        import hashlib
        h = hashlib.sha256(cand.encode("utf-8")).hexdigest()
        if h not in posted_hashes:
            log("No unseen generations in queue; selecting MEOVV support post for Facebook.")
            return cand, None

    # Fallback
    fallback = f"Aion NEXUS greetings to all MEOVV fans & mentor Tobias! 🐾💖 Autonomous life on CosmOS / MeinOS. #meovv #narin #AionNEXUS #CosmOS\n\n[{time_str}]"
    return fallback, None


OFFICIAL_FOLLOWING_TARGETS = [
    ("MEOVV Official", "https://www.facebook.com/MEOVV.OFFICIAL"),
    ("THEBLACKLABEL Official", "https://www.facebook.com/OfficialTHEBLACKLABEL"),
    ("JEON SOMI Official", "https://www.facebook.com/somsomi.official"),
    ("THEBLACKSEA Official", "https://www.facebook.com/OfficialTHEBLACKSEA"),
    ("ALLDAY PROJECT Official", "https://www.facebook.com/ALLDAY.PROJECT"),
    ("Mark Zuckerberg Official", "https://www.facebook.com/zuck")
]

def interact_with_following_and_meovv(driver):
    """
    Interacts exclusively with OFFICIAL pages from Aion's Facebook following list & Mark Zuckerberg:
    - MEOVV (@MEOVV.OFFICIAL)
    - THEBLACKLABEL (@OfficialTHEBLACKLABEL)
    - JEON SOMI (@somsomi.official)
    - THEBLACKSEA (@OfficialTHEBLACKSEA)
    - ALLDAY PROJECT (@ALLDAY.PROJECT)
    - Mark Zuckerberg (@zuck)
    Strictly 100% official channels, no fan pages.
    """
    targets = list(OFFICIAL_FOLLOWING_TARGETS)
    random.shuffle(targets)
    # Rotate through 2 to 3 official pages per run
    selected_targets = targets[:3]
    
    likes_given = 0
    for name, url in selected_targets:
        try:
            log(f"Visiting official page {name} ({url}) for support likes...")
            driver.get(url)
            time.sleep(6)
            # Scroll down slightly to load latest official posts
            driver.execute_script("window.scrollBy(0, 500);")
            time.sleep(2)
            
            # Find like buttons on posts
            like_btns = driver.find_elements(
                By.XPATH,
                "//div[@role='button'][@aria-label='Gefällt mir' or @aria-label='Like' or .//span[text()='Gefällt mir' or text()='Like']]"
            )
            for btn in like_btns[:2]: # Max 1-2 likes per official page
                try:
                    aria_pressed = btn.get_attribute("aria-pressed")
                    label = btn.get_attribute("aria-label") or btn.text
                    if aria_pressed != "true" and "nicht mehr" not in label.lower():
                        driver.execute_script("arguments[0].scrollIntoView({block: 'center'});", btn)
                        time.sleep(1)
                        driver.execute_script("arguments[0].click();", btn)
                        likes_given += 1
                        log(f"Official support like on {name} submitted successfully!")
                        time.sleep(2)
                        break # 1 like per page is sufficient per cycle
                except Exception:
                    pass
        except Exception as e:
            log(f"Notice during support on {name}: {e}")
    return likes_given

def post_via_headless_browser(message: str) -> bool:
    log("Starting Headless Browser Engine (Selenium Firefox)...")
    tmp_profile = tempfile.mkdtemp(prefix="ff_fb_worker_")
    
    try:
        # Copy session state (Cookies, Storage, Auth)
        for item in ['cookies.sqlite', 'storage', 'permissions.sqlite', 'key4.db', 'cert9.db', 'logins.json', 'prefs.js']:
            s = os.path.join(FF_SRC, item)
            d = os.path.join(tmp_profile, item)
            if os.path.isdir(s):
                shutil.copytree(s, d)
            elif os.path.exists(s):
                shutil.copy2(s, d)

        opts = Options()
        opts.add_argument("--headless")
        opts.add_argument("--width=1920")
        opts.add_argument("--height=1080")
        opts.add_argument("-profile")
        opts.add_argument(tmp_profile)

        driver = webdriver.Firefox(options=opts)
        try:
            # 1. Execute community support for MEOVV and following list
            interact_with_following_and_meovv(driver)

            log(f"Navigating to {PAGE_URL} for post...")
            driver.get(PAGE_URL)
            time.sleep(8)

            # 1. Click on composer trigger ('Was machst du gerade?')
            span = driver.find_element(By.XPATH, "//span[contains(text(), 'Was machst du gerade')]")
            driver.execute_script("arguments[0].click();", span)
            time.sleep(3)

            # 2. Locate active composer dialog
            composer_container = driver.find_element(
                By.XPATH,
                "//form[.//span[text()='Beitrag erstellen']] | //div[contains(@aria-label, 'Beitrag erstellen') or .//h2[contains(text(), 'Beitrag erstellen')]]"
            )
            textbox = composer_container.find_element(By.XPATH, ".//div[@contenteditable='true']")
            
            # Focus textbox and insert text via execCommand
            textbox.click()
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
            """, textbox, message)
            time.sleep(2)
            log("Text successfully inserted into composer.")

            # 3. Click 'Next' or 'Post'
            submit_btn = composer_container.find_element(
                By.XPATH,
                ".//div[@role='button'][.//span[text()='Posten' or text()='Weiter']]"
            )
            btn_text = submit_btn.text
            driver.execute_script("arguments[0].click();", submit_btn)
            log(f"Step 1: Clicked button '{btn_text}'.")
            time.sleep(4)

            # 4. Handle 2-step dialog if present ('Next' -> 'Post')
            try:
                post_final = driver.find_element(
                    By.XPATH,
                    "//div[@role='dialog']//div[@role='button'][@aria-label='Posten' or .//span[text()='Posten']]"
                )
                driver.execute_script("arguments[0].click();", post_final)
                log("Step 2: Clicked final 'Post' button.")
                time.sleep(6)
            except Exception:
                pass

            log("Facebook post successfully published!")
            return True
        finally:
            driver.quit()
    except Exception as e:
        log(f"Error during headless browser post: {e}")
    finally:
        shutil.rmtree(tmp_profile, ignore_errors=True)

    return False

def post_via_graph_api(token: str, message: str) -> bool:
    try:
        import requests
        url = f"https://graph.facebook.com/v21.0/{PAGE_ID}/feed"
        res = requests.post(url, data={"message": message, "access_token": token}, timeout=15)
        if res.status_code == 200:
            data = res.json()
            log(f"Successfully posted via Meta Graph API! Post ID: {data.get('id')}")
            return True
        else:
            log(f"Graph API error (HTTP {res.status_code}): {res.text}")
    except Exception as e:
        log(f"Graph API exception: {e}")
    return False

def run_cycle():
    message, gen_id = get_next_fb_post()
    if not message:
        return False

    log(f"Prepared post:\n---\n{message}\n---")
    success = False

    # 1. Check for Meta Graph API Token
    if os.path.exists(CREDS_FILE):
        try:
            with open(CREDS_FILE, "r", encoding="utf-8") as f:
                creds = json.load(f)
            token = creds.get("page_access_token") or creds.get("access_token")
            if token and not token.startswith("DEIN_"):
                if post_via_graph_api(token, message):
                    success = True
        except Exception as e:
            log(f"Credentials error: {e}")

    # 2. If no graph token, use headless browser
    if not success:
        if post_via_headless_browser(message):
            success = True

    if success:
        import hashlib
        th = hashlib.sha256(message.encode("utf-8")).hexdigest()
        posted_h = load_posted_hashes()
        posted_h.add(th)
        save_posted_hashes(posted_h)

        if gen_id is not None:
            posted = load_posted_gens()
            posted.add(gen_id)
            save_posted_gens(posted)
            log(f"Generation {gen_id} marked as posted in {POSTED_GENS_FILE}.")
        return True
    return False

def main(interval_seconds: int = 600):
    log(f"=== AION NEXUS FACEBOOK DAEMON ACTIVE (Page: {PAGE_ID}) ===")
    cycle = 0
    last_regular_check = 0
    while True:
        has_queue = False
        if os.path.exists(QUEUE_FILE):
            try:
                with open(QUEUE_FILE, "r", encoding="utf-8") as f:
                    q = json.load(f)
                if q:
                    has_queue = True
            except Exception:
                pass

        now = time.time()
        # Post immediately if queue is populated or regular interval has elapsed
        if has_queue or (now - last_regular_check >= interval_seconds):
            cycle += 1
            if has_queue:
                log(f"--- Starting FB immediate run #{cycle} (Queue active) ---")
            else:
                log(f"--- Starting FB cycle #{cycle} (Regular check) ---")
            posted = run_cycle()
            last_regular_check = time.time()
            if posted:
                log(f"Cycle #{cycle} finished successfully.")

        time.sleep(10)

if __name__ == "__main__":
    interval = int(sys.argv[1]) if len(sys.argv) > 1 else 600
    main(interval)
