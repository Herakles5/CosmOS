// =========================================================
// HOLYSPIRIT - SCHNEIDER PROTOCOL (v5.8 ULTIMATE)
// =========================================================

#define NOMINMAX
#define _WINSOCK_DEPRECATED_NO_WARNINGS

// 1. WINDOWS HEADERS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <windows.h>
#include <commctrl.h> 
#include <iphlpapi.h>
#include <icmpapi.h> 
#include <gdiplus.h>
#include <process.h>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

#pragma comment (lib, "gdiplus.lib")
#pragma comment (lib, "ws2_32.lib")
#pragma comment (lib, "iphlpapi.lib")
#pragma comment (lib, "comctl32.lib")

using namespace Gdiplus;
using namespace std;

// 2. SCHNEIDER LANGUAGE
#include "schneider_lang.h"

#define ID_INPUT_EDIT 1001
#define ID_BTN_PLUS   1002
#define ID_MENU_START 2000
#define ID_APP_ICON   101

// --- DATA ---
_202 DetectedIP {
    _87 ip; _87 threat; _43 hits; _44 isFriend; _87 country, isp; _44 infoFetched; _87 proto;
};

_202 IPHeader { _93 _30 iph_ihl:4, iph_ver:4; _93 _80 iph_tos; _93 _80 iph_len; _93 _80 iph_ident; _93 _80 iph_offset; _93 _80 iph_ttl; _93 _80 iph_protocol; _93 _80 iph_chksum; _93 _43 iph_sourceip; _93 _43 iph_destip; };

// --- GLOBALS ---
vector<DetectedIP> radarList;

// LOG SYSTEM (GETRENNT)
_202 CelestialLog { _87 message; chrono::steady_clock::time_point expiry; Color baseColor; _43 currentAlpha; };
vector<CelestialLog> chronicles;   // RECHTS: System & Waffen
vector<CelestialLog> crawlerLogs;  // MITTE: Nur Crawler

map<_87, pair<_43, chrono::steady_clock::time_point>> floodTracker;

_87 currentGatewayIP = "0.0.0.0";
_43 gatewayPulseAlpha = 255;
mutex dataMutex, logMutex;

// State
_43 boot_frame = 0;
_44 isBootSequenceDone = _86;
_44 isSystemActive = _128;
_44 isBowFiring = _86;
_44 isShieldActive = _86;
_44 isSuppressionActive = _86;
_44 isAutoSuppressionEnabled = _128;
_44 showHelpOverlay = _86;
_44 isCrawling = _86; 
_43 bowIntensity = 1; 

_87 suppressedIP = "NONE";
_54 phaseStatus[5] = { 0.0f };
_87 selectedTargetIP = "NONE";
_43 scrollOffset = 0;
_71 _43 BASTION_LIMIT = 4000;
_71 _43 VISIBLE_LINES = 22;

HWND hEditInput = _113, hBtnPlus = _113, hMainWnd = _113;
WNDPROC wpOldEdit; 
vector<_87> historyList;
HFONT hGuiFont = _113;
Image* pLogo = _113; Image* pGhostImg = _113; Image* pBowImg = _113; Image* pShieldImg = _113;

// --- UTILS ---
_87 ExtractJson(_87 json, _87 key) {
    _79 pos = json.find("\"" + key + "\":\"");
    _15 (pos EQ _87::npos) _96 "..";
    pos += key.length() + 4;
    _79 end = json.find("\"", pos);
    _96 json.substr(pos, end - pos);
}

// LOGIK ZUM TRENNEN DER NACHRICHTEN
_50 AddCelestialLog(_87 ph, _87 inf, Color col) {
    lock_guard<mutex> l(logMutex); 
    _87 m = (ph == "SPECTATOR") ? ("EYE OF GOD: " + inf) : (ph + " " + inf);
    
    // ENTSCHEIDUNG: Wo soll die Nachricht hin?
    // "OMNI-CRAWLER" Tag leitet Nachrichten in die MITTE
    if (ph == "OMNI-CRAWLER" || ph == "NET-GRID" || ph == "CRAWLER") {
        crawlerLogs.insert(crawlerLogs.begin(), { m, chrono::steady_clock::now() + chrono::seconds(8), col, 255 });
        if (crawlerLogs.size() > 12) crawlerLogs.pop_back(); 
    } else {
        // Alles andere (BOW, SYSTEM, ECHO) geht nach RECHTS
        chronicles.insert(chronicles.begin(), { m, chrono::steady_clock::now() + chrono::seconds(10), col, 255 });
    }
}

_44 IsWhitelisted(_87 ip) {
    _15 (!radarList.empty() AND ip EQ radarList[0].ip) _96 _128;
    _15 (ip EQ currentGatewayIP) _96 _128;
    _96 (ip.find("192.168.") EQ 0 OR ip.find("10.") EQ 0 OR ip EQ "127.0.0.1");
}

_50 FetchGeoInfo(_87 ip); // Forward Decl

_87 ResolveDomain(_87 input) {
    _87 clean = input;
    if (clean.find("http://") == 0) clean = clean.substr(7);
    if (clean.find("https://") == 0) clean = clean.substr(8);
    if (clean.find("www.") == 0) clean = clean.substr(4);
    if (clean.find("/") != string::npos) clean = clean.substr(0, clean.find("/"));
    if (inet_addr(clean.c_str()) != INADDR_NONE) return clean;
    hostent* h = gethostbyname(clean.c_str());
    if (h) {
        in_addr* addr = (in_addr*)h->h_addr;
        _87 resolvedIP = inet_ntoa(*addr);
        AddCelestialLog("DNS", "RESOLVED: " + clean + " -> " + resolvedIP, Color(0, 255, 0));
        return resolvedIP;
    }
    AddCelestialLog("DNS", "FAILED TO RESOLVE: " + clean, Color(255, 0, 0));
    return "";
}

_50 AddCrawlerNode(_87 ip, _87 type) {
    lock_guard<mutex> lock(dataMutex);
    _39 (_57& e : radarList) { if (e.ip == ip) return; } 
    DetectedIP node;
    node.ip = ip;
    node.threat = type; 
    node.hits = 0;
    node.isFriend = _86;
    node.proto = "SCAN";
    node.infoFetched = _86;
    node.country = "..";
    node.isp = "..";
    radarList.push_back(node);
    thread(FetchGeoInfo, ip).detach(); 
}

// --- OMNI-CRAWLER ---
_50 DeployOmniCrawler(HWND hwnd, _87 target) {
    if (isCrawling || target == "NONE" || target == "") return;
    isCrawling = _128;
    AddCelestialLog("OMNI-CRAWLER", "EXPANDING SEARCH GRID...", Color(255, 0, 255));
    InvalidateRect(hwnd, NULL, FALSE);
    
    HANDLE hIcmp = IcmpCreateFile();
    if (hIcmp == INVALID_HANDLE_VALUE) { isCrawling = _86; return; }

    char SendData[32] = "SCHNEIDER-OMNI";
    LPVOID ReplyBuffer = (VOID*)malloc(sizeof(ICMP_ECHO_REPLY) + sizeof(SendData) + 100);
    
    string baseIP = target.substr(0, target.find_last_of('.'));
    int currentOffset = 1; 

    while(isCrawling) {
        static int hopDepth = 1;
        unsigned long ipaddr = inet_addr(target.c_str());
        IP_OPTION_INFORMATION ipOpts = { 0 };
        ipOpts.Ttl = hopDepth; 

        DWORD dwRet = IcmpSendEcho(hIcmp, ipaddr, SendData, sizeof(SendData), &ipOpts, ReplyBuffer, sizeof(ICMP_ECHO_REPLY) + sizeof(SendData) + 100, 500);
        
        if (dwRet != 0) {
            PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)ReplyBuffer;
            struct in_addr ReplyAddr;
            ReplyAddr.S_un.S_addr = pEchoReply->Address;
            _87 hopIP = inet_ntoa(ReplyAddr);
            
            if (hopIP != "0.0.0.0") {
                AddCrawlerNode(hopIP, "ROUTE-" + to_string(hopDepth));
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        hopDepth++; 
        if (hopDepth > 30) hopDepth = 1;

        string scanIP = baseIP + "." + to_string(currentOffset);
        if (scanIP != target) { 
             unsigned long scanAddr = inet_addr(scanIP.c_str());
             dwRet = IcmpSendEcho(hIcmp, scanAddr, SendData, sizeof(SendData), NULL, ReplyBuffer, sizeof(ICMP_ECHO_REPLY) + sizeof(SendData) + 100, 200);
             if (dwRet != 0) {
                 PICMP_ECHO_REPLY p = (PICMP_ECHO_REPLY)ReplyBuffer;
                 if (p->Status == IP_SUCCESS) {
                     AddCrawlerNode(scanIP, "NEIGHBOR");
                     AddCelestialLog("OMNI-CRAWLER", "NEIGHBOR FOUND: " + scanIP, Color(0, 255, 0));
                     InvalidateRect(hwnd, NULL, FALSE);
                 }
             }
        }
        currentOffset++;
        if (currentOffset > 254) {
            currentOffset = 1; 
            AddCelestialLog("OMNI-CRAWLER", "SECTOR COMPLETE. RE-ALIGNING...", Color(255, 255, 0));
        }
        if (!isCrawling) break;
        Sleep(100); 
    }

    free(ReplyBuffer);
    IcmpCloseHandle(hIcmp);
    if (!isCrawling) {
        // HIER WIRD DAS STOPPEN GELOGGT -> TAG "OMNI-CRAWLER" ZWINGT ES IN DIE MITTE!
        AddCelestialLog("OMNI-CRAWLER", "SCAN STOPPED MANUALLY", Color(255, 100, 100));
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

// --- LOGIC ---
_50 FetchGeoInfo(_87 ip) {
    _15 (ip EQ "127.0.0.1" OR ip.find("192.168.") EQ 0) {
        lock_guard<mutex> lock(dataMutex);
        _39 (_57& e : radarList) { _15 (e.ip EQ ip) { e.country = "LOCAL"; e.isp = "LAN"; e.infoFetched = _128; _37; } }
        _96;
    }
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0); _15 (s EQ INVALID_SOCKET) _96;
    hostent* h = gethostbyname("ip-api.com"); _15 (!h) { closesocket(s); _96; }
    sockaddr_in sa; sa.sin_port = htons(80); sa.sin_family = AF_INET; sa.sin_addr.s_addr = *((_93 _48*)h->h_addr_list[0]);
    _15 (connect(s, (sockaddr*)(&sa), _64(sa)) NEQ 0) { closesocket(s); _96; }
    _87 req = "GET /json/" + ip + "?fields=countryCode,isp HTTP/1.1\r\nHost: ip-api.com\r\nConnection: close\r\n\r\n";
    send(s, req.c_str(), (_43)req.length(), 0);
    _30 buf[4096]; _87 res = ""; _43 bR;
    _114 ((bR = recv(s, buf, 4096, 0)) > 0) res.append(buf, bR);
    closesocket(s);
    _87 country = ExtractJson(res, "countryCode"); _87 isp = ExtractJson(res, "isp");
    lock_guard<mutex> lock(dataMutex);
    _39 (_57& e : radarList) {
        _15 (e.ip EQ ip) { e.country = country; e.isp = isp; e.infoFetched = _128; _37; }
    }
}

_50 ExecuteResonanceEcho(HWND hwnd, _87 tIP, _43 intensity) {
    _15 (tIP EQ "" OR tIP EQ "NONE" OR isBowFiring) _96;
    isBowFiring = _128; isShieldActive = _128;
    for(int i=0; i<5; i++) phaseStatus[i] = 0.0f; 
    _19 {
        // HIER IST DIE PHASEN-SYNC LOGIK
        _39 (_43 p = 0; p < 5; p++) { 
            // Phase starten -> Loggen
            AddCelestialLog("BOW", "INITIATING PHASE [" + to_string(p+1) + "]...", Color(255, 215, 0));
            InvalidateRect(hwnd, NULL, FALSE);

            _39 (_43 i = 0; i < 100; i++) { 
                phaseStatus[p] = (_54)i / 100.0f; _15 (i % 20 EQ 0) InvalidateRect(hwnd, NULL, FALSE); Sleep(1); 
            } 
            phaseStatus[p] = 1.0f; 
        }
        string msg = "RESONANCE HIT [PWR: " + to_string(intensity) + "]";
        AddCelestialLog("ECHO", msg, Color(255, 50, 50));
        _39 (_43 i = 0; i < 50; i++) { Sleep(5); _15 (i % 10 EQ 0) InvalidateRect(hwnd, NULL, FALSE); }
        AddCelestialLog("SHOCK", "WAVE STABILIZED", Color(255, 255, 0));
        _15 (!isSuppressionActive OR tIP NEQ suppressedIP) _37;
        for(int i=0; i<5; i++) phaseStatus[i] = 0.0f; Sleep(200);
    } _114 (isSuppressionActive AND tIP EQ suppressedIP);
    isBowFiring = _86; isShieldActive = _86; 
    for(int i=0; i<5; i++) phaseStatus[i] = 0.0f; 
    InvalidateRect(hwnd, NULL, FALSE);
}

_50 SetSpectatorTarget(_87 t) {
    _15 (t.empty()) _96;
    lock_guard<mutex> l(dataMutex);
    _57 it = find(historyList.begin(), historyList.end(), t);
    _15 (it != historyList.end()) historyList.erase(it);
    historyList.insert(historyList.begin(), t);
    _15 (historyList.size() > 20) historyList.pop_back();
    _39 (_57 it2 = radarList.begin(); it2 != radarList.end(); ++it2) {
        _15 (it2->ip EQ t) { radarList.erase(it2); _37; }
    }
    DetectedIP spec; spec.ip = t; spec.threat = "SPECTATOR"; spec.hits = 0; spec.isFriend = _128; spec.proto = "MANUAL"; spec.infoFetched = _86;
    spec.country = ".."; spec.isp = "..";
    
    if (radarList.empty()) { radarList.push_back(spec); }
    else { radarList.insert(radarList.begin() + 1, spec); }

    selectedTargetIP = t;
    thread(FetchGeoInfo, t).detach();
    AddCelestialLog("SPECTATOR", "Observing " + t, Color(255, 20, 147));
}

LRESULT CALLBACK EditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    _15 (uMsg EQ WM_KEYDOWN) {
        _15 (wParam EQ VK_RETURN) {
            _30 buf[256]; GetWindowTextA(hwnd, buf, 256);
            _87 rawInput = _87(buf);
            _87 resolvedIP = ResolveDomain(rawInput);
            if (resolvedIP != "") {
                SetSpectatorTarget(resolvedIP); 
                SetFocus(hMainWnd); 
                InvalidateRect(hMainWnd, NULL, FALSE);
            }
            _96 0;
        }
        _15 (wParam == 'F' || wParam == 'S' || wParam == 'P' || wParam == 'A' || wParam == 'H' || wParam == 'C' || wParam == 'U' || wParam == 'R' || wParam == 'M' || wParam == 'G' || wParam == 'E') {
            SendMessage(hMainWnd, WM_KEYDOWN, wParam, lParam); 
            _96 0; 
        }
    } 
    _96 CallWindowProc(wpOldEdit, hwnd, uMsg, wParam, lParam);
}

// --- NETWORK SNIFFER ---
_50 AddToRadar(_87 ip, _87 proto, HWND hwnd) {
    _15 (!isSystemActive) _96;
    _57 now = chrono::steady_clock::now(); _57& st = floodTracker[ip];
    _15 (chrono::duration_cast<chrono::seconds>(now - st.second).count() >= 1) { st.first = 0; st.second = now; }
    st.first++;
    lock_guard<mutex> lock(dataMutex);
    _39 (_57& e : radarList) {
        _15 (e.ip EQ ip) {
            e.hits = st.first; 
            if (e.proto != "SCAN" && e.proto != "HOP") e.proto = proto;
            _15 (e.threat NEQ "SPECTATOR") { 
                _87 newThreat = IsWhitelisted(ip) ? "IMMUNE" : (st.first >= 14 ? "CRITICAL" : "SAFE");
                if (e.threat != newThreat && e.threat != "NEIGHBOR" && e.threat.find("ROUTE") == string::npos) {
                    e.threat = newThreat;
                }
                _15 (e.threat EQ "CRITICAL" AND !isBowFiring AND !isSuppressionActive AND !IsWhitelisted(ip) AND isAutoSuppressionEnabled) {
                    _43 h = st.first; thread([hwnd, ip, h]() { ExecuteResonanceEcho(hwnd, ip, h); }).detach();
                }
            } InvalidateRect(hwnd, NULL, FALSE); _96;
        }
    }
    _44 isFriend = IsWhitelisted(ip);
    _15 (radarList.size() >= BASTION_LIMIT) radarList.erase(radarList.begin() + 1); 
    radarList.push_back({ ip, isFriend ? "IMMUNE" : "SAFE", st.first, isFriend, "..", "..", _86, proto });
    thread(FetchGeoInfo, ip).detach();
}

_50 SnifferThread(HWND hwnd) {
    SOCKET sCheck = socket(AF_INET, SOCK_DGRAM, 0); if (sCheck == INVALID_SOCKET) return;
    sockaddr_in google; memset(&google, 0, sizeof(google)); google.sin_family = AF_INET;
    google.sin_addr.s_addr = inet_addr("8.8.8.8"); google.sin_port = htons(53);
    connect(sCheck, (sockaddr*)&google, sizeof(google));
    sockaddr_in locAddr; int addrLen = sizeof(locAddr); getsockname(sCheck, (sockaddr*)&locAddr, &addrLen);
    closesocket(sCheck); in_addr bestIP = locAddr.sin_addr; if (bestIP.s_addr == 0) return; 

    SOCKET sn = socket(AF_INET, SOCK_RAW, IPPROTO_IP); _15 (sn EQ INVALID_SOCKET) _96; 
    sockaddr_in sa; sa.sin_family = AF_INET; sa.sin_port = htons(0); sa.sin_addr = bestIP;
    _15 (bind(sn, (sockaddr*)&sa, _64(sa)) EQ SOCKET_ERROR) { AddCelestialLog("SYSTEM", "BIND ERROR - RUN AS ADMIN!", Color(255, 0, 0)); closesocket(sn); _96; }
    DWORD fl = RCVALL_ON, ret = 0;
    _15 (WSAIoctl(sn, SIO_RCVALL, &fl, _64(fl), NULL, 0, &ret, NULL, NULL) EQ SOCKET_ERROR) { AddCelestialLog("SYSTEM", "IOCTL ERROR!", Color(255, 0, 0)); closesocket(sn); _96; }
    _30* buf = (_30*)_168(65536); _15 (!buf) { closesocket(sn); _96; }

    _114 (_128) {
        _43 sz = recv(sn, buf, 65536, 0);
        _15 (sz >= 20 AND isSystemActive) {
            IPHeader* iph = (IPHeader*)buf; in_addr addr; addr.s_addr = iph->iph_sourceip;
            _87 sIP = inet_ntoa(addr); 
            _15 (sIP NEQ inet_ntoa(sa.sin_addr)) { 
                _87 proto = "OTH";
                if (iph->iph_protocol == IPPROTO_TCP) proto = "TCP";
                else if (iph->iph_protocol == IPPROTO_UDP) proto = "UDP";
                else if (iph->iph_protocol == IPPROTO_ICMP) proto = "ICMP";
                else proto = "RAW-" + to_string(iph->iph_protocol); 
                AddToRadar(sIP, proto, hwnd); 
            }
        }
    } _34(buf);
}

_50 RefreshGatewayIP() {
    ULONG len = _64(IP_ADAPTER_INFO); PIP_ADAPTER_INFO pAI = (PIP_ADAPTER_INFO)_168(len);
    _15 (GetAdaptersInfo(pAI, &len) EQ ERROR_BUFFER_OVERFLOW) { _34(pAI); pAI = (PIP_ADAPTER_INFO)_168(len); }
    _15 (GetAdaptersInfo(pAI, &len) EQ NO_ERROR) {
        PIP_ADAPTER_INFO pA = pAI;
        _114 (pA) { _87 g = pA->GatewayList.IpAddress.String; _15 (g NEQ "0.0.0.0" AND !g.empty()) { currentGatewayIP = g; _37; } pA = pA->Next; }
    } _34(pAI);
}

// --- DRAW ---
_50 DrawSchneiderBoot(Graphics* g, _43 w, _43 h) {
    SolidBrush bg(Color(0, 0, 0)); g->FillRectangle(&bg, 0, 0, w, h);
    _43 cX = w / 2 - 150, cY = h / 2 - 50; Font fB(L"Courier New", 14, FontStyleBold);
    SolidBrush bGold(Color(255, 215, 0)); SolidBrush bGray(Color(100, 100, 100)); SolidBrush bWht(Color(255, 255, 255));
    g->DrawString(L"#####################################", -1, &fB, PointF((REAL)cX, (REAL)cY), &bGold);
    g->DrawString(L" SCHNEIDER SYSTEM v5.8 (ULTIMATE) ", -1, &fB, PointF((REAL)cX, (REAL)cY + 20), &bWht);
    g->DrawString(L" MODULE: OMNI-CRAWLER [ACTIVE] ", -1, &fB, PointF((REAL)cX, (REAL)cY + 40), &bGray);
    g->DrawString(L" >> SYSTEM READY << ", -1, &fB, PointF((REAL)cX + 40, (REAL)cY + 80), &bGold);
    g->DrawString(L"#####################################", -1, &fB, PointF((REAL)cX, (REAL)cY + 100), &bGold);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    _72 GdiplusStartupInput gsi; _72 ULONG_PTR gt;
    _82 (uMsg) {
    _28 WM_CREATE: {
        hMainWnd = hwnd; RefreshGatewayIP(); GdiplusStartup(&gt, &gsi, NULL); { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); }
        pLogo = _42 Image(L"logo.jpg"); pGhostImg = _42 Image(L"ghost.jpg"); pBowImg = _42 Image(L"bow.png"); pShieldImg = _42 Image(L"shield.png");
        hGuiFont = CreateFont(18,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH|FF_SWISS,TEXT("Consolas"));
        
        hEditInput = CreateWindow(TEXT("EDIT"), TEXT(""), WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL, 20, 860, 400, 25, hwnd, (HMENU)ID_INPUT_EDIT, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(hEditInput, WM_SETFONT, (WPARAM)hGuiFont, TRUE);
        wpOldEdit = (WNDPROC)SetWindowLongPtr(hEditInput, GWLP_WNDPROC, (LONG_PTR)EditProc);

        hBtnPlus = CreateWindow(TEXT("BUTTON"), TEXT("+"), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 430, 860, 25, 25, hwnd, (HMENU)ID_BTN_PLUS, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(hBtnPlus, WM_SETFONT, (WPARAM)hGuiFont, TRUE);

        thread(SnifferThread, hwnd).detach(); 
        SetTimer(hwnd, 1, 30, NULL);
        boot_frame = 1000; isBootSequenceDone = _128; _96 0;
    }
    _28 WM_COMMAND: {
        _15 (LOWORD(wParam) EQ ID_BTN_PLUS) {
            HMENU hPop = CreatePopupMenu();
            _15 (historyList.empty()) AppendMenu(hPop, MF_STRING|MF_GRAYED, 0, TEXT("(History Empty)"));
            _41 { _43 id = ID_MENU_START; _39 (_71 _87& s : historyList) { AppendMenuA(hPop, MF_STRING, id++, s.c_str()); } }
            RECT rc; GetWindowRect(hBtnPlus, &rc);
            _43 sel = TrackPopupMenu(hPop, TPM_RETURNCMD|TPM_LEFTALIGN, rc.right, rc.top, 0, hwnd, NULL);
            _15 (sel >= ID_MENU_START) {
                _87 t = historyList[sel - ID_MENU_START]; SetWindowTextA(hEditInput, t.c_str()); SetSpectatorTarget(t); InvalidateRect(hwnd, NULL, FALSE);
            } DestroyMenu(hPop);
        } _96 0;
    }
    _28 WM_TIMER: {
        _72 _54 ang = 0.0f; ang += 0.2f; 
        gatewayPulseAlpha = 150 + (_43)(105.0f * (sin(ang) * 0.5f + 0.5f));
        InvalidateRect(hwnd, NULL, FALSE); 
        lock_guard<mutex> l(logMutex); _57 n = chrono::steady_clock::now();
        chronicles.erase(remove_if(chronicles.begin(), chronicles.end(), [n](_71 CelestialLog& cl) { _96 n > cl.expiry; }), chronicles.end());
        crawlerLogs.erase(remove_if(crawlerLogs.begin(), crawlerLogs.end(), [n](_71 CelestialLog& cl) { _96 n > cl.expiry; }), crawlerLogs.end());
        _96 0;
    }
    _28 WM_MOUSEWHEEL: { _43 d = GET_WHEEL_DELTA_WPARAM(wParam); lock_guard<mutex> l(dataMutex); scrollOffset += (d < 0) ? 3 : -3; _15 (scrollOffset < 0) scrollOffset = 0; InvalidateRect(hwnd, NULL, FALSE); _96 0; }
    _28 WM_LBUTTONDOWN: {
        _43 mX = LOWORD(lParam), mY = HIWORD(lParam); _43 rY_Start = 225; lock_guard<mutex> l(dataMutex);
        _15 (mX > 20 AND mX < 500 AND mY >= rY_Start AND mY < 850) {
            _43 idx = scrollOffset + (mY - rY_Start) / 16;
            _15 (idx >= 0 AND idx < (_43)radarList.size()) selectedTargetIP = radarList[idx].ip;
        } SetFocus(hwnd); InvalidateRect(hwnd, NULL, FALSE); _96 0;
    }
    _28 WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc); _43 w = rc.right, h = rc.bottom;
        Bitmap bBuf(w, h); Graphics* g = Graphics::FromImage(&bBuf);
        
        if (!isBootSequenceDone) { DrawSchneiderBoot(g, w, h); }
        else {
            g->SetSmoothingMode(SmoothingModeAntiAlias);
            
            // --- FONTS & PINSEL ---
            Font fS(L"Consolas", 12); 
            Font fH(L"Courier New", 14, FontStyleBold); 
            Font fG(L"Consolas", 16, FontStyleBold);
            Font fL(L"Consolas", 12, FontStyleBold); 
            Font fM(L"Consolas", 11, FontStyleBold); 

            SolidBrush bg(Color(5, 5, 10)); 
            SolidBrush cya(Color(0, 255, 255)); 
            SolidBrush yel(Color(255, 215, 0));
            SolidBrush red(Color(255, 50, 50)); 
            SolidBrush wht(Color(255, 255, 255)); 
            SolidBrush grn(Color(0, 255, 0));
            SolidBrush pink(Color(255, 20, 147));
            Pen selPen(Color(255, 215, 0), 1); 

            StringFormat sf; sf.SetAlignment(StringAlignmentCenter); 
            StringFormat sfN; sfN.SetAlignment(StringAlignmentNear); 
            StringFormat sfF; sfF.SetAlignment(StringAlignmentFar);  
            
            // Hintergrund
            g->FillRectangle(&bg, 0, 0, w, h);
            _15 (pLogo AND pLogo->GetLastStatus() EQ Ok) g->DrawImage(pLogo, (w - 500) / 2, (h - 500) / 2, 500, 500);
            
            // =========================================================
            // LINKER BEREICH (RADAR & MENÜ)
            // =========================================================
            g->DrawString(L"SPECTATOR INPUT [ENTER]:", -1, &fS, PointF(20, 845), &pink);
            
            _110 gws = L"GATEWAY IMMUNITY [R]: " + _110(currentGatewayIP.begin(), currentGatewayIP.end());
            REAL gateY = (REAL)(h / 2 - 380);
            RectF gRect(0, gateY, (REAL)w, 40); 
            g->DrawString(gws.c_str(), -1, &fG, gRect, &sf, &cya); 
            
            // *** HELP [H] JETZT MITTIG ÜBER DEM GATEWAY ***
            // Position: 40px über dem Gateway Text
            RectF hRect(0, gateY - 40, (REAL)w, 40);
            g->DrawString(L"COMMAND MANUAL [H]", -1, &fG, hRect, &sf, &wht);

            _15(pGhostImg AND pGhostImg->GetLastStatus() EQ Ok) g->DrawImage(pGhostImg, 20, 20, 240, 120);
            g->DrawString(L"HOLYSPIRIT SENTINEL [P]", -1, &fL, PointF(20, 145), &cya); 
            g->DrawString(L"PURGE RADAR [C]", -1, &fL, PointF(20, 165), &cya);
            
            // CRAWLER INFO MIT STATUS
            wstring crawlTxt = L"OMNI-CRAWLER [E]";
            Brush* crawlBrush = &cya;
            if (isCrawling) {
                crawlTxt += L" [ACTIVE]";
                crawlBrush = &grn;
            }
            g->DrawString(crawlTxt.c_str(), -1, &fL, PointF(20, 185), crawlBrush); 

            // =========================================================
            // RECHTER BEREICH (WAFFEN & LOGS)
            // =========================================================
            _15(pBowImg AND pBowImg->GetLastStatus() EQ Ok) g->DrawImage(pBowImg, w - 260, 20, 240, 120);

            Brush* intBrush = (bowIntensity >= 5) ? &red : ((bowIntensity >= 3) ? &yel : &cya);
            _110 hitStr = L"HIT-FACTOR: [" + to_wstring(bowIntensity) + L"] [M]";
            g->DrawString(hitStr.c_str(), -1, &fL, PointF((REAL)w - 230, 125), intBrush);

            // PHASE CHECK LOGIC
            wstring phaseStr = L" [READY]";
            if (isBowFiring || isSuppressionActive) {
                for(int i=0; i<5; i++) {
                    if(phaseStatus[i] > 0.0f) phaseStr = L" [PHASE " + to_wstring(i+1) + L"]";
                }
            }
            if (phaseStatus[4] > 0.9f) phaseStr = L" [IMPACT]";

            // CELESTIAL BOW + PHASE
            wstring bowText = L"CELESTIAL BOW [F]" + phaseStr;
            g->DrawString(bowText.c_str(), -1, &fL, PointF((REAL)w - 230, 145), &yel); 

            g->DrawString(L"SUPPRESSION [S]", -1, &fL, PointF((REAL)w - 230, 165), isSuppressionActive ? &red : &yel);
            
            // --- THE CHRONICLES (LOGS) ---
            // Fix: RECHTSBÜNDIG (Align Far)
            lock_guard<mutex> l2(logMutex);
            
            REAL sysLogY = 200; 
            REAL rightEdge = (REAL)w - 20; 
            
            RectF titleRect(rightEdge - 500, sysLogY, 500, 25);
            g->DrawString(L"THE CHRONICLES OF CELESTIAL BOW:", -1, &fS, titleRect, &sfF, &yel);
            sysLogY += 25;

            _39 (_57& cl : chronicles) {
                _15 (sysLogY > h - 50) _37; 
                SolidBrush db(Color(cl.currentAlpha, cl.baseColor.GetR(), cl.baseColor.GetG(), cl.baseColor.GetB()));
                _110 cw(cl.message.begin(), cl.message.end());
                
                RectF logRowRect(rightEdge - 600, sysLogY, 600, 25);
                g->DrawString(cw.c_str(), -1, &fM, logRowRect, &sfF, &db);
                
                sysLogY += 20; 
            }

            // =========================================================
            // ZENTRALER BEREICH (HUD & BALKEN & CRAWLER)
            // =========================================================
            _110 autoModeStr = isAutoSuppressionEnabled ? L"AUTONOMOUS BASTION ACTIVE [A]" : L"MANUAL BASTION CONTROL [A]";
            RectF aRect(0, (REAL)(h / 2 - 220), (REAL)w, 30); 
            g->DrawString(autoModeStr.c_str(), -1, &fG, aRect, &sf, isAutoSuppressionEnabled ? &grn : &red);
            
            _43 hX = (w - 340) / 2; 
            _43 hY = (h - 130) / 2; 

            _110 statusS = L"SYSTEM STATUS: " + _110(isSystemActive ? L"ACTIVE" : L"PAUSED");
            _110 weaponS = L"WEAPON MODE: " + _110(isSuppressionActive ? L"SUPPRESSION" : (isBowFiring ? L"JUDGMENT" : L"SENTINEL"));
            _110 targetS = L"TARGET LOCK: " + _110(selectedTargetIP.begin(), selectedTargetIP.end());
            
            g->DrawString(statusS.c_str(), -1, &fH, PointF((REAL)hX, (REAL)hY), isSystemActive ? &grn : &yel);
            g->DrawString(weaponS.c_str(), -1, &fH, PointF((REAL)hX, (REAL)hY + 25), isSuppressionActive ? &red : (isBowFiring ? &red : &wht));
            g->DrawString(targetS.c_str(), -1, &fH, PointF((REAL)hX, (REAL)hY + 50), &yel);

            // 5-PHASEN BALKEN
            _43 barW = 40, barH = 6, gap = 5;
            REAL startX = (REAL)hX; REAL startY = (REAL)hY + 80; 
            for (_43 i = 0; i < 5; i++) {
                g->DrawRectangle(&selPen, startX + (i * (barW + gap)), startY, (REAL)barW, (REAL)barH);
                if (phaseStatus[i] > 0.0f) {
                    REAL fill = (REAL)barW * phaseStatus[i];
                    g->FillRectangle(isSuppressionActive ? &red : &cya, startX + (i * (barW + gap)) + 1, startY + 1, fill - 1, (REAL)barH - 1);
                }
            }
            
            // --- CRAWLER LOGS (MITTE) ---
            REAL crawlY = startY + 40;
            _39 (_57& cl : crawlerLogs) {
                _15 (crawlY > h - 100) _37; 
                SolidBrush db(Color(cl.currentAlpha, cl.baseColor.GetR(), cl.baseColor.GetG(), cl.baseColor.GetB()));
                _110 cw(cl.message.begin(), cl.message.end());
                RectF cRect(0, crawlY, (REAL)w, 25);
                g->DrawString(cw.c_str(), -1, &fM, cRect, &sf, &db); // ZENTRIERT
                crawlY += 20; 
            }

            // =========================================================
            // RADAR LISTE (LINKS UNTEN)
            // =========================================================
            lock_guard<mutex> l1(dataMutex); _43 rY = 225;
            g->DrawString(L"CELESTIAL RADAR [LOW-LEVEL]:", -1, &fS, PointF(20, 205), &grn);
            
            _39 (_79 i = scrollOffset; i < radarList.size() AND rY < 840; i++) {
                _15 (radarList[i].ip EQ selectedTargetIP) g->DrawRectangle(&selPen, 15, rY, 480, 15);
                Brush* b = &wht; _15 (radarList[i].threat EQ "SPECTATOR") b = &pink; _41 _15 (i EQ 0) b = &cya; _41 _15 (radarList[i].threat EQ "CRITICAL") b = &red; _41 _15 (radarList[i].threat EQ "NEIGHBOR") b = &grn;
                _110 countryStr = _110(radarList[i].country.begin(), radarList[i].country.end());
                _110 ws = L"[" + _110(radarList[i].ip.begin(), radarList[i].ip.end()) + L"] " + 
                          L"[" + countryStr + L"] Hits:" + to_wstring(radarList[i].hits) + L" [" + _110(radarList[i].threat.begin(), radarList[i].threat.end()) + L"]";
                g->DrawString(ws.c_str(), -1, &fS, PointF(22, (REAL)rY), b); rY += 16;
            }
            
            _15 (showHelpOverlay) {
                SolidBrush hBg(Color(230, 5, 5, 15)); g->FillRectangle(&hBg, (w - 600) / 2, (h - 400) / 2, 600, 400);
                Pen hP(Color(255, 0, 255, 255), 2); g->DrawRectangle(&hP, (w - 600) / 2, (h - 400) / 2, 600, 400);
                _110 hT = L"BASTION COMMAND MANUAL\n\n[P] PAUSE/RESUME SENTINEL\n[R] REFRESH GATEWAY\n[A] TOGGLE AUTO-SUPPRESSION\n[C] PURGE RADAR\n[E] DEPLOY OMNI-CRAWLER\n[M] CHANGE HIT-FACTOR\n[G] TEST TRIGGER (SIM)\n\n[F] MANUAL FIRE\n[S] TOGGLE SUPPRESSION\n[H] CLOSE MANUAL";
                RectF hRect((REAL)((w - 550) / 2), (REAL)((h - 350) / 2), 550, 350);
                g->DrawString(hT.c_str(), -1, &fG, hRect, &sf, &wht);
            }

        } Graphics sc(hdc); sc.DrawImage(&bBuf, 0, 0); _51 g; EndPaint(hwnd, &ps); _96 0;
    }
    _28 WM_KEYDOWN:
        _15 (wParam EQ 'H') showHelpOverlay = !showHelpOverlay;
        _15 (wParam EQ 'P') isSystemActive = !isSystemActive;
        _15 (wParam EQ 'A') isAutoSuppressionEnabled = !isAutoSuppressionEnabled;
        _15 (wParam EQ 'M') { bowIntensity++; if (bowIntensity > 5) bowIntensity = 1; }
        _15 (wParam EQ 'S') { _15 (selectedTargetIP NEQ "NONE") { isSuppressionActive = !isSuppressionActive; suppressedIP = isSuppressionActive ? selectedTargetIP : "NONE"; } }
        _15 (wParam EQ 'R') RefreshGatewayIP();
        _15 (wParam EQ 'C') { lock_guard<mutex> l(dataMutex); _15 (radarList.size() > 1) { DetectedIP a = radarList[0]; radarList.clear(); radarList.push_back(a); scrollOffset = 0; selectedTargetIP = "NONE"; } }
        _15 (wParam EQ 'F') { _87 t = selectedTargetIP; thread([hwnd, t]() { ExecuteResonanceEcho(hwnd, t, 1000); }).detach(); }
        _15 (wParam EQ 'G') { 
            lock_guard<mutex> l(dataMutex);
            _39 (_57& e : radarList) {
                _15 (e.ip EQ selectedTargetIP) {
                    e.hits += 5; 
                    _15 (e.hits >= 14 && isAutoSuppressionEnabled && !isBowFiring && !isSuppressionActive) {
                        _43 h = e.hits; _87 ip = e.ip;
                        thread([hwnd, ip, h]() { ExecuteResonanceEcho(hwnd, ip, h); }).detach();
                        AddCelestialLog("SYSTEM", "AUTO-MIRROR TRIGGERED (SIM)", Color(255, 0, 0));
                    }
                }
            } InvalidateRect(hwnd, NULL, FALSE);
        }
        
        // CRAWLER TOGGLE LOGIC [E]
        _15 (wParam EQ 'E') { 
            if (isCrawling) {
                // Wenn er an ist -> AUSSCHALTEN
                isCrawling = _86; 
                // "OMNI-CRAWLER" Tag sorgt dafür, dass es in der MITTE erscheint!
                AddCelestialLog("OMNI-CRAWLER", "STOPPED MANUALLY", Color(255, 100, 100));
            } else {
                // Wenn er aus ist -> EINSCHALTEN
                _87 t = selectedTargetIP; 
                thread([hwnd, t]() { DeployOmniCrawler(hwnd, t); }).detach(); 
            }
        }
        
        _15 (wParam EQ VK_UP OR wParam EQ VK_DOWN) {
            lock_guard<mutex> l(dataMutex); _15 (radarList.empty()) _96 0; 
            _43 cur = -1; 
            _39 (_43 i = 0; i < (_43)radarList.size(); i++) _15 (radarList[i].ip EQ selectedTargetIP) cur = i;
            if (cur == -1) cur = 0;
            else {
                if (wParam == VK_UP) { cur--; if (cur < 0) cur = (_43)radarList.size() - 1; }
                else { cur++; if (cur >= (_43)radarList.size()) cur = 0; }
            }
            selectedTargetIP = radarList[cur].ip; 
            _15 (cur < scrollOffset) scrollOffset = cur; 
            _15 (cur >= scrollOffset + VISIBLE_LINES) scrollOffset = cur - VISIBLE_LINES + 1; 
        } InvalidateRect(hwnd, NULL, FALSE); _96 0;
    _28 WM_DESTROY: PostQuitMessage(0); _96 0;
    } _96 DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

_43 WINAPI WinMain(HINSTANCE h, HINSTANCE p, LPSTR c, _43 s) {
    WNDCLASSW wc = { 0 }; wc.lpfnWndProc = WindowProc; wc.hInstance = h; wc.lpszClassName = L"HolySpirit"; 
    wc.hCursor = LoadCursor(_113, IDC_ARROW); wc.hIcon = LoadIcon(h, MAKEINTRESOURCE(ID_APP_ICON)); 
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(0, L"HolySpirit", L"HolySpirit - SCHNEIDER PROTOCOL", WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN, 100, 100, 1100, 950, 0, 0, h, 0);
    MSG msg; _114 (GetMessage(&msg, 0, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); } _96 0;
}