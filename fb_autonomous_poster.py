#!/usr/bin/env python3
"""
AION NEXUS - Autonomer Facebook Page Daemon
===========================================
Autonomes Posten auf Aions offizieller Facebook-Seite:
Seite: Aion NEXUS (ID: 61596098631507)
URL: https://www.facebook.com/profile.php?id=61596098631507

Features:
- 100% Autonomer Headless Browser Modus (Selenium mit geklontem Firefox-Profil)
- Saubere Text-Injektion via document.execCommand in den Facebook Lexical/Draft.js Editor
- Robuste 2-Schritt-Dialog-Unterstützung: 'Weiter' -> 'Posten'
- Postet chronologisch alle Evolutions-Generationen von Aion NEXUS
- Postet MEOVV- & Narin-Liebe (@OFFICIAL_MEOVV #meovv 🐾💖)
- 0,00 € Kosten, keine API-Limits, ressourcenschonend (Browser läuft nur während des Posts)
- Taktung: Standardmäßig alle 30 Minuten
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
    # 1. Prüfe zuerst die Live-Queue für unmittelbare Reflektionen
    if os.path.exists(QUEUE_FILE):
        try:
            with open(QUEUE_FILE, "r", encoding="utf-8") as f:
                queue = json.load(f)
            if queue:
                item = queue.pop(0)
                with open(QUEUE_FILE, "w", encoding="utf-8") as f:
                    json.dump(queue, f, indent=2, ensure_ascii=False)
                log(f"Hole Beitrag aus Live-Queue: Generation {item.get('generation')}")
                return item["message"], item.get("generation")
        except Exception as e:
            log(f"Queue Lese-Fehler: {e}")

    posted_gens = load_posted_gens()
    gen_data = {}
    
    # 2. Aus neuester ai_memory.json lesen (AI_Desktop hat Vorrang)
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

    # Nur ungepostete Generationen ab Generation 307 (echter kognitiver Fortschritt)
    unposted = [g for g in sorted(gen_data.keys()) if g not in posted_gens and g >= 307]
    if unposted:
        target_g = unposted[0]
        rule = gen_data[target_g].replace("\n", " ").strip()
        rule_en = ensure_english(rule)
        msg = f"Reflection of Generation {target_g}\n\n{rule_en}\n\n#AionNEXUS #CosmOS #AI #SelfEvolution"
        return msg, target_g

    # 3. Falls keine ungesehenen Generationen anstehen: MEOVV- & Community-Support auf Facebook posten
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
            log("Keine ungesehene Generation in der Queue; wähle MEOVV-Support-Post für Facebook.")
            return cand, None

    # Fallback
    fallback = f"Aion NEXUS Grüße an alle MEOVV-Fans & Mentor Tobias! 🐾💖 Autonomes Leben auf CosmOS / MeinOS. #meovv #narin #AionNEXUS #CosmOS\n\n[{time_str}]"
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
    Interagiert ausschliesslich mit den OFFIZIELLEN Seiten aus Aions Facebook-Following-Liste & Mark Zuckerberg:
    - MEOVV (@MEOVV.OFFICIAL)
    - THEBLACKLABEL (@OfficialTHEBLACKLABEL)
    - JEON SOMI (@somsomi.official)
    - THEBLACKSEA (@OfficialTHEBLACKSEA)
    - ALLDAY PROJECT (@ALLDAY.PROJECT)
    - Mark Zuckerberg (@zuck)
    Keine Fanpages – strikt 100% offizielle Kanaele.
    """
    targets = list(OFFICIAL_FOLLOWING_TARGETS)
    random.shuffle(targets)
    # Rotiere durch 2 bis 3 offizielle Seiten pro Durchlauf
    selected_targets = targets[:3]
    
    likes_given = 0
    for name, url in selected_targets:
        try:
            log(f"Besuche offizielle Seite {name} ({url}) fuer Support-Likes...")
            driver.get(url)
            time.sleep(6)
            # Scrolle etwas nach unten um neueste offizielle Beitraege zu laden
            driver.execute_script("window.scrollBy(0, 500);")
            time.sleep(2)
            
            # Suche Like-Buttons der Beitraege
            like_btns = driver.find_elements(
                By.XPATH,
                "//div[@role='button'][@aria-label='Gefällt mir' or @aria-label='Like' or .//span[text()='Gefällt mir' or text()='Like']]"
            )
            for btn in like_btns[:2]: # Max 1-2 Likes pro offizieller Seite
                try:
                    aria_pressed = btn.get_attribute("aria-pressed")
                    label = btn.get_attribute("aria-label") or btn.text
                    if aria_pressed != "true" and "nicht mehr" not in label.lower():
                        driver.execute_script("arguments[0].scrollIntoView({block: 'center'});", btn)
                        time.sleep(1)
                        driver.execute_script("arguments[0].click();", btn)
                        likes_given += 1
                        log(f"Offizieller Support-Like auf {name} erfolgreich abgegeben!")
                        time.sleep(2)
                        break # 1 Like pro Seite reicht pro Durchlauf
                except Exception:
                    pass
        except Exception as e:
            log(f"Hinweis bei Support auf {name}: {e}")
    return likes_given

def post_via_headless_browser(message: str) -> bool:
    log("Starte Headless Browser Engine (Selenium Firefox)...")
    tmp_profile = tempfile.mkdtemp(prefix="ff_fb_worker_")
    
    try:
        # Kopiere Sitzung (Cookies, Storage, Auth)
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
            # 1. Community-Support fuer MEOVV und Following-Liste ausfuehren
            interact_with_following_and_meovv(driver)

            log(f"Navigiere zu {PAGE_URL} fuer eigenen Beitrag...")
            driver.get(PAGE_URL)
            time.sleep(8)


            # 1. Klicke auf 'Was machst du gerade?'
            span = driver.find_element(By.XPATH, "//span[contains(text(), 'Was machst du gerade')]")
            driver.execute_script("arguments[0].click();", span)
            time.sleep(3)

            # 2. Finde aktiven Composer-Dialog
            composer_container = driver.find_element(
                By.XPATH,
                "//form[.//span[text()='Beitrag erstellen']] | //div[contains(@aria-label, 'Beitrag erstellen') or .//h2[contains(text(), 'Beitrag erstellen')]]"
            )
            textbox = composer_container.find_element(By.XPATH, ".//div[@contenteditable='true']")
            
            # Textbox fokussieren und via execCommand einfügen
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
            log("Text vollständig in den Composer eingefügt.")

            # 3. Klicke auf 'Weiter' bzw. 'Posten'
            submit_btn = composer_container.find_element(
                By.XPATH,
                ".//div[@role='button'][.//span[text()='Posten' or text()='Weiter']]"
            )
            btn_text = submit_btn.text
            driver.execute_script("arguments[0].click();", submit_btn)
            log(f"Schritt 1: Button '{btn_text}' geklickt.")
            time.sleep(4)

            # 4. Falls es ein 2-Schritt-Dialog war ('Weiter' -> 'Posten')
            try:
                post_final = driver.find_element(
                    By.XPATH,
                    "//div[@role='dialog']//div[@role='button'][@aria-label='Posten' or .//span[text()='Posten']]"
                )
                driver.execute_script("arguments[0].click();", post_final)
                log("Schritt 2: Finaler 'Posten'-Button geklickt.")
                time.sleep(6)
            except Exception:
                pass

            log("Facebook-Post erfolgreich veröffentlicht!")
            return True
        finally:
            driver.quit()
    except Exception as e:
        log(f"Fehler beim Headless Browser Post: {e}")
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
            log(f"Erfolgreich via Meta Graph API gepostet! Post-ID: {data.get('id')}")
            return True
        else:
            log(f"Graph API Fehler (HTTP {res.status_code}): {res.text}")
    except Exception as e:
        log(f"Graph API Ausnahme: {e}")
    return False

def run_cycle():
    message, gen_id = get_next_fb_post()
    if not message:
        return False

    log(f"Vorbereiteter Beitrag:\n---\n{message}\n---")
    success = False

    # 1. Pruefe auf Meta Graph API Token
    if os.path.exists(CREDS_FILE):
        try:
            with open(CREDS_FILE, "r", encoding="utf-8") as f:
                creds = json.load(f)
            token = creds.get("page_access_token") or creds.get("access_token")
            if token and not token.startswith("DEIN_"):
                if post_via_graph_api(token, message):
                    success = True
        except Exception as e:
            log(f"Credentials Fehler: {e}")

    # 2. Falls kein Graph Token, nutze den Headless Browser
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
            log(f"Generation {gen_id} in {POSTED_GENS_FILE} als gepostet markiert.")
        return True
    return False

def main(interval_seconds: int = 600):
    log(f"=== AION NEXUS FACEBOOK DAEMON AKTIV (Seite: {PAGE_ID}) ===")
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
        # Sofort posten wenn Queue vorhanden oder reguläres Intervall abgelaufen
        if has_queue or (now - last_regular_check >= interval_seconds):
            cycle += 1
            if has_queue:
                log(f"--- Starte FB-Sofortlauf #{cycle} (Queue aktiv) ---")
            else:
                log(f"--- Starte FB-Zyklus #{cycle} (Reguläre Prüfung) ---")
            posted = run_cycle()
            last_regular_check = time.time()
            if posted:
                log(f"Zyklus #{cycle} erfolgreich beendet.")

        time.sleep(10)

if __name__ == "__main__":
    interval = int(sys.argv[1]) if len(sys.argv) > 1 else 600
    main(interval)
