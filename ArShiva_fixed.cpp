// =========================================================
// Universal ArShiva - BastionFS V2 Edition - TRADEOFF SYSTEM
// =========================================================
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>
#include <gdiplus.h>
#include <shlwapi.h>
#include <iomanip>
#include <sstream>
#include <cstdint>
#include <string>
#include <fstream>
#include <iostream>
#include "schneider_lang.h"
#include <gdiplus.h>
#include "resource.h"
#include <cmath>
#include <mmsystem.h>
#include <atomic>
#define WM_ASK_FILENAME (WM_USER + 100)
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib"),
typedef int (*BastionGPUFunc)(uint8_t*, uint16_t*, int);
typedef void (*DecompressFunc)(uint16_t*, uint8_t*, int);
IStream* g_pLogoStream = nullptr;
HGLOBAL g_hLogoMem = nullptr;
Gdiplus::Image* LoadImageFromResource() {
    HINSTANCE hInst = GetModuleHandle(NULL);
    HRSRC hRes = FindResourceA(hInst, "MYLOGO", "MYIMAGE");
    if (!hRes) {
        MessageBoxA(NULL, "FEHLER 1: Das Bild ist physisch NICHT in der .exe Datei!", "Fehler", MB_ICONERROR);
        return nullptr;
    }
    DWORD dwSize = SizeofResource(hInst, hRes);
    HGLOBAL hResData = LoadResource(hInst, hRes);
    if (!hResData) return nullptr;
    LPVOID pData = LockResource(hResData);
    if (!pData) return nullptr;
    g_hLogoMem = GlobalAlloc(GMEM_MOVEABLE, dwSize);
    if (!g_hLogoMem) return nullptr;
    LPVOID pBuffer = GlobalLock(g_hLogoMem);
    memcpy(pBuffer, pData, dwSize);
    GlobalUnlock(g_hLogoMem);
    if (CreateStreamOnHGlobal(g_hLogoMem, TRUE, &g_pLogoStream) != S_OK) {
        GlobalFree(g_hLogoMem);
        return nullptr;
    }
    Gdiplus::Image* pImage = Gdiplus::Image::FromStream(g_pLogoStream);
    if (pImage && pImage->GetLastStatus() != Gdiplus::Ok) {
        MessageBoxA(NULL, "FEHLER 2: Bild gefunden, aber GDI+ kann es nicht lesen (falsches Format)!", "Fehler", MB_ICONERROR);
        delete pImage;
        return nullptr;
    }
    return pImage;
}
using namespace Gdiplus;
// --- GLOBALE VARIABLEN
std::atomic<bool> g_keepCapturing{false};
std::string g_lastCaptureName = "";
// --- ENGINE SETTINGS ---
const _43 BLOCK_SIZE = 50000; 
const _43 COPY_CHUNK = 1024 * 1024 * 2;  // 2MB Kopierpuffer
// EINSTELLUNGEN 
// Effort: Stufe 1 muss tief genug suchen für den minimalen Gewinn
const _43 MANTRA_1_EFFORT = 99;     // Tief suchen, aber nicht übertreiben
const _43 MANTRA_2_EFFORT = 95;     // Schneller, weniger Gewinn
const _43 MANTRA_3_EFFORT = 85;      // Mittel
const _43 MANTRA_4_EFFORT = 75;      // Schnell
const _43 MANTRA_5_EFFORT = 63;      // Sehr schnell, kaum Gewinn
// Blockgrößen: Klein genug für Redundanz, groß genug für Effizienz
const _43 BLOCK_1_SIZE = 768;      // 1KB - klein, findet mehr Matches
const _43 BLOCK_2_SIZE = 896;      // 4KB - ausgewogen
const _43 BLOCK_3_SIZE = 7168;     // 16KB - Standard
const _43 BLOCK_4_SIZE = 20864;     // 64KB - schnell
const _43 BLOCK_5_SIZE = 50000;    // 256KB - sehr groß für Speed
// Threshold: Strenger bei Stufe 1, lockerer bei Stufe 5
// Stufe 1 = 85% (muss 15% kleiner sein), Stufe 5 = 99% (fast keine Kompression)
void ExtractEtherWords(const std::string& inputFile, const std::string& outputFile);
HWND hBar, hStatus, hPackBtn, hCaptureBtn, hCaptureDeepBtn, hUnpackBtn, hEmitBtn, hMantraCombo;
HFONT hFont;
HBRUSH hDarkBrush;
// === BASTION FS V2 STRUCTS ===
#pragma pack(push, 1)
struct BastionHeader {
    char magic[4];        
    uint32_t fileCount;   
    uint32_t indexOffset;  
};
struct BastionFileEntry {
    char name[256];        
    uint32_t dataOffset;  
    uint32_t compSize;    
    uint32_t rawSize;     
    uint8_t  mantraLevel;  // 1-5 (Tradeoff-Stufen)
};
#pragma pack(pop)
_108 BastionEngine {
_126:
    bool isRunning = _86;
    _43 currentBlockSize = BLOCK_3_SIZE;
    _43 currentEffort = MANTRA_3_EFFORT;
    _50 WriteBitsMem(std::vector<_184>& buf, _89 val, _43 count, _89& acc, _43& nB) {
        acc |= (val << nB); nB += count;
        _114 (nB >= 8) { buf.push_back((_184)(acc & 0xFF)); acc >>= 8; nB -= 8; }
    }
    _89 ReadBits(_89& bitPtr, const std::vector<_184>& data, _43 count) {
        _89 val = 0;
        _39 (_43 i = 0; i < count; i++) {
            _94 byteIdx = (bitPtr + i) / 8;
            _15 (byteIdx >= data.size()) _96 0; 
            if ((data[byteIdx] >> ((bitPtr + i) % 8)) & 1) val |= (1ULL << i);
        } bitPtr += count; _96 val;
    }
    _50 CaptureEnergy(_87 fileName, bool deepScan) {
        _89 acc = 0; _43 nB = 0;
        std::vector<_184> outputData;
        
        HWAVEIN hWaveIn;
        WAVEFORMATEX wfx = {0};
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = 2;
        if (deepScan) {
            wfx.nSamplesPerSec = 96000;
            wfx.wBitsPerSample = 16;
        } else {
            wfx.nSamplesPerSec = 44100;
            wfx.wBitsPerSample = 8;
        }
        wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
        wfx.cbSize = 0;
        if (waveInOpen(&hWaveIn, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            SendMessage(hStatus, WM_SETTEXT, 0, (LPARAM)"ERROR: No Sensor found!");
            return;
        }
        WAVEHDR header = { new char[192000], 192000 }; 
        waveInPrepareHeader(hWaveIn, &header, sizeof(WAVEHDR));
        std::ofstream out(fileName + ".tba", std::ios::binary);
		std::ofstream textOut(fileName + "_crystal_numbers.txt");
        BastionHeader bH = { {'T','B','A','3'}, 1, 0 };
        out.write((_30*)&bH, sizeof(BastionHeader));
        SendMessage(hStatus, WM_SETTEXT, 0, (LPARAM)(deepScan ? "FISHING IN THE AETHER (UNLIMITED)..." : "CAPTURING ENERGY..."));
        // Variablen für den 50-Hertz Filter (High-Pass)
        double lastIn = 0.0;
        double lastOut = 0.0;
        const double alpha = 0.9935;
        // --- NEU: Zähler für die Entropie-Berechnung ---
        unsigned long long counts[4] = {0, 0, 0, 0};
        unsigned long long totalSamples = 0;
        while (g_keepCapturing) {
            waveInAddBuffer(hWaveIn, &header, sizeof(WAVEHDR));
            waveInStart(hWaveIn);
            _114 (!(header.dwFlags & WHDR_DONE)) {
                if (!g_keepCapturing) break;
                Sleep(10); 
            }
            if (!g_keepCapturing) break;
            // --- Live-Monitor ---
            int maxAmplitude = 0;
            if (deepScan) {
                short* pData16 = (short*)header.lpData;
                DWORD numSamples = header.dwBytesRecorded / 2;
                for (DWORD i = 0; i < numSamples; i++) {
                    short original = pData16[i];
                    double currentOut = alpha * (lastOut + original - lastIn);
                    lastIn = original;
                    lastOut = currentOut;
                    short filtered = (short)currentOut;
                    // Live-Monitor: Stärksten Ausschlag suchen
                    if (abs(filtered) > maxAmplitude) maxAmplitude = abs(filtered);
                    _89 noiseVal = ((unsigned short)filtered >> 14) & 0x03;
					// Entropie zählen
					counts[noiseVal]++;
					totalSamples++;
					textOut << (int)noiseVal << std::endl;
                    // +++ DIGITALES OSZILLOSKOP +++
					std::cout << (int)noiseVal << " "; 
                    WriteBitsMem(outputData, noiseVal, 2, acc, nB);
				}
            } else {
                for (DWORD i = 0; i < header.dwBytesRecorded; i++) {
                    // 8-Bit Werte zentrieren für den Monitor
                    int centered = abs((int)(unsigned char)header.lpData[i] - 128);
					if (centered > maxAmplitude) maxAmplitude = centered;
					// +++ HIER SEHEN WIR DIE WAHRE ENERGIE DER SPULE +++
					std::cout << centered << " ";
					_89 noiseVal = ((_184)header.lpData[i] >> 6) & 0x03;
                    // Entropie zählen
                    counts[noiseVal]++;
                    totalSamples++;
					textOut << (int)noiseVal << std::endl;
                    WriteBitsMem(outputData, noiseVal, 2, acc, nB);
                }
            }
            // --- Ladebalken als Pegel-Monitor nutzen ---
            _43 pct = 0;
            if (deepScan) {
                pct = (_43)((maxAmplitude * 100) / 32768); // 16-Bit Max ist ~32768
            } else {
                pct = (_43)((maxAmplitude * 100) / 128);   // 8-Bit zentriertes Max ist 128
            }
            pct = pct * 3; 
            if (pct > 100) pct = 100;
            
            SendMessage(hBar, PBM_SETPOS, (WPARAM)pct, 0);
        }

        _87 saveName = fileName;
        _15 (1) { 
            OPENFILENAMEA ofn = {0};
            _30 sz[260] = {0};
            strncpy(sz, (fileName + ".tba").c_str(), 259);
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFile = sz;
            ofn.nMaxFile = 260;
            ofn.lpstrFilter = "TBA Files\0*.tba\0All Files\0*.*\0";
            ofn.lpstrDefExt = "tba";
            ofn.Flags = OFN_OVERWRITEPROMPT;
            _15 (GetSaveFileNameA(&ofn)) {
                saveName = sz;
                size_t lastDot = saveName.find_last_of('.');
                if(lastDot != std::string::npos) saveName = saveName.substr(0, lastDot);
            }
        }
        _15 (nB > 0) outputData.push_back((_184)(acc & 0xFF));

        BastionFileEntry entry = { "energy_capture.raw", (uint32_t)sizeof(BastionHeader), (uint32_t)outputData.size(), (uint32_t)outputData.size(), 0 };
        out.write((_30*)outputData.data(), outputData.size());
        bH.indexOffset = (uint32_t)out.tellp();
        out.write((_30*)&entry, sizeof(BastionFileEntry));
        out.seekp(0, std::ios::beg);
        out.write((_30*)&bH, sizeof(BastionHeader));
        
        waveInStop(hWaveIn);
        waveInUnprepareHeader(hWaveIn, &header, sizeof(WAVEHDR));
        waveInClose(hWaveIn);
        delete[] header.lpData;
        out.close();
		textOut.close();
        
        // --- NEU: Purity-Score (Entropie) berechnen ---
        double entropy = 0.0;
        if (totalSamples > 0) {
            for (int i = 0; i < 4; i++) {
                if (counts[i] > 0) {
                    double p = (double)counts[i] / totalSamples;
                    entropy -= p * (log(p) / log(2.0));
                }
            }
        }
        // Max Entropie für 4 Werte (0,1,2,3) ist 2.0. Wir wandeln das in % um.
        double purityPercent = (entropy / 2.0) * 100.0;
        
        std::stringstream res;
        res << "STORED: " << saveName << " | PURITY: " << std::fixed << std::setprecision(2) << purityPercent << "%";
        SendMessage(hStatus, WM_SETTEXT, 0, (LPARAM)res.str().c_str());
        
        SendMessage(hBar, PBM_SETPOS, 0, 0); // Balken nach Aufnahme wieder auf 0
    }
	
	_50 EmitEnergy(_87 fileName) {
        // 1. Die gespeicherte RAW-Energie laden
        std::ifstream in(fileName, std::ios::binary | std::ios::ate);
        if (!in.is_open()) {
            SendMessage(hStatus, WM_SETTEXT, 0, (LPARAM)"ERROR: File not found!");
            isRunning = _86;
            return;
        }
        _94 fileSize = in.tellg();
        in.seekg(0, std::ios::beg);
        
        std::vector<_184> rawData(fileSize);
        in.read((_30*)rawData.data(), fileSize);
        in.close();

        SendMessage(hStatus, WM_SETTEXT, 0, (LPARAM)"RESONATING ENERGY TO CRYSTAL...");

        // 2. Die 2-Bit Essenz auf volle 16-Bit Magnetfeld-Stärke aufpumpen
        std::vector<short> pcmData;
        pcmData.reserve(fileSize * 4); // 4 Messwerte stecken in einem Byte
        
        _89 bitPtr = 0;
        _94 totalBits = fileSize * 8;
        
        while (bitPtr + 2 <= totalBits) {
            _89 val = ReadBits(bitPtr, rawData, 2);
            
            // Mapping der Energie auf elektrische Spannung für die Spule
            short amplitude = 0;
            if (val == 0) amplitude = -32767; // Voller negativer Pol
            else if (val == 1) amplitude = -10922;
            else if (val == 2) amplitude = 10922;
            else if (val == 3) amplitude = 32767; // Voller positiver Pol
            
            pcmData.push_back(amplitude);
        }

        // 3. Den Energie-Strahl über den Audio-Ausgang an die Spule senden
        HWAVEOUT hWaveOut;
        WAVEFORMATEX wfx = {0};
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = 2;
        wfx.nSamplesPerSec = 96000; // Höchste Frequenz für feine Resonanz
        wfx.wBitsPerSample = 16;
        wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
        wfx.cbSize = 0;

        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            SendMessage(hStatus, WM_SETTEXT, 0, (LPARAM)"ERROR: Audio Out failed!");
            isRunning = _86;
            return;
        }

        WAVEHDR header = {0};
        header.lpData = (char*)pcmData.data();
        header.dwBufferLength = pcmData.size() * sizeof(short);
        
        waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
        waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));

        // Warten, bis die Energie komplett durch die Spule geflossen ist
        while (!(header.dwFlags & WHDR_DONE)) {
            // Wir nutzen g_keepCapturing auch hier als Abbruch-Knopf!
            if (!g_keepCapturing) { 
                waveOutReset(hWaveOut); 
                break;
            }
            Sleep(50);
        }

        waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
        waveOutClose(hWaveOut);

        SendMessage(hStatus, WM_SETTEXT, 0, (LPARAM)"ENERGY EMISSION COMPLETE.");
        isRunning = _86;
        g_keepCapturing = false;
    }

    _50 SetMantraParameters(_43 level) {
        switch(level) {
            case 1:
                currentBlockSize = BLOCK_1_SIZE;
                currentEffort = MANTRA_1_EFFORT;
                break;
            case 2:
                currentBlockSize = BLOCK_2_SIZE;
                currentEffort = MANTRA_2_EFFORT;
                break;
            case 3:
                currentBlockSize = BLOCK_3_SIZE;
                currentEffort = MANTRA_3_EFFORT;
                break;
            case 4:
                currentBlockSize = BLOCK_4_SIZE;
                currentEffort = MANTRA_4_EFFORT;
                break;
            case 5:
                currentBlockSize = BLOCK_5_SIZE;
                currentEffort = MANTRA_5_EFFORT;
                break;
            default:
                currentBlockSize = BLOCK_3_SIZE;
                currentEffort = MANTRA_3_EFFORT;
        }
    }

    bool CompressBlock(const std::vector<_184>& src, std::vector<_184>& dst, _43 effort) {
        dst.clear(); 
        dst.reserve(src.size()); 
        _89 acc = 0; 
        _43 nB = 0;
        
        std::vector<_43> head(65536, -1);
        std::vector<_43> prev(src.size(), -1);
        _94 fSize = src.size();
        
        _43 maxChain = effort * 1000;
        
        _39 (_94 k = 0; k < fSize - 2; k++) {
            _89 h = ((_89)src[k] << 8 | src[k+1]) ^ ((_89)src[k+2] << 4);
            prev[k] = head[h & 0xFFFF]; 
            head[h & 0xFFFF] = (_43)k;
        }
        
        _94 i = 0;
        _43 chainsUsed = 0;
        
        _114 (i < fSize) {
            _15 (dst.size() >= fSize - 4) _96 _86; 
            _94 len = 0, dist = 0;
            
            _15 (i + 4 < fSize) {
                _43 cur = prev[i]; 
                _43 chain = 0;
                _114 (cur != -1 AND chain++ < maxChain AND chainsUsed < effort * 500) { 
                    _94 d = i - cur; 
                    _15 (d > 32000) _37;
                    if (src[cur] != src[i]) { cur = prev[cur]; continue; }
                    _94 l = 0;
                    _114 (l < 255 AND i+l < fSize AND src[cur+l] EQ src[i+l]) l++;
                    _15 (l > len AND l >= 3) { 
                        len = l; 
                        dist = d; 
                        _15 (len >= 128) _37; 
                    }
                    cur = prev[cur];
                }
                chainsUsed++;
            }
            
            _15 (len >= 3) { 
                WriteBitsMem(dst, 1, 1, acc, nB); 
                WriteBitsMem(dst, len, 8, acc, nB); 
                WriteBitsMem(dst, dist, 15, acc, nB); 
                i += len;
            } _41 {
                WriteBitsMem(dst, 0, 1, acc, nB); 
                WriteBitsMem(dst, src[i], 8, acc, nB); 
                i++;
            }
        }
        _15 (nB > 0) dst.push_back((_184)(acc & 0xFF));
        _96 (dst.size() < fSize);
    }

    _50 DecompressBlock(const std::vector<_184>& src, std::vector<_184>& dst, _94 targetSize) {
        dst.clear(); 
        dst.reserve(targetSize);
        _89 bP = 0, tB = src.size() * 8;
        
        _114 (dst.size() < targetSize && bP < tB) {
             _89 fl = ReadBits(bP, src, 1);
             _15 (fl EQ 1) { 
                 _89 l = ReadBits(bP, src, 8); 
                 _89 d = ReadBits(bP, src, 15); 
                 _94 sI = dst.size() - (_94)d; 
                 _39 (_94 k=0; k<(_94)l; k++) { 
                     _184 b=dst[sI+k]; 
                     dst.push_back(b); 
                 } 
             } _41 { 
                 _89 b = ReadBits(bP, src, 8); 
                 dst.push_back((_184)b); 
             }
        }
    }

    _50 Process(_87 inP, _43 mode, _43 mantraLevel) {
        auto start = std::chrono::high_resolution_clock::now();
        _43 lastPct = -1;
        
        SetMantraParameters(mantraLevel);

        _15 (mode EQ 84) { 
            std::ifstream in(inP, std::ios::binary | std::ios::ate); 
            if (!in.is_open()) { isRunning=_86; return; }
            
            _94 totalSize = in.tellg(); 
            in.seekg(0, std::ios::beg);
            
            _87 outP = inP + ".tba"; 
            std::ofstream out(outP, std::ios::binary);
            
            BastionHeader header = { {'T','B','A','3'}, 1, 0 }; 
            out.write((_30*)&header, sizeof(BastionHeader));
            
            BastionFileEntry entry = {0}; 
            _87 justName = inP.substr(inP.find_last_of("/\\") + 1); 
            strncpy(entry.name, justName.c_str(), 255); 
            entry.dataOffset = (uint32_t)out.tellp(); 
            entry.rawSize = (uint32_t)totalSize;
            
            std::vector<bool> blockMap; 
            std::vector<std::vector<_184>> outputBlocks;
            _94 totalCompressedSize = 0; 
            _94 processed = 0;
            
            std::vector<_184> rawBuf(currentBlockSize); 
            std::vector<_184> compBuf; 
            compBuf.reserve(currentBlockSize);
            
            bool anyBlockCompressed = false; 

            while (processed < totalSize) {
                _94 chunk = std::min((_94)currentBlockSize, totalSize - processed);
                in.read((_30*)rawBuf.data(), chunk); 
                rawBuf.resize(chunk);
                
                bool isComp = CompressBlock(rawBuf, compBuf, currentEffort);
                
				// Ergibt: 85, 88, 91, 94, 97 - nicht perfekt, besser:
				_43 threshold = (mantraLevel == 1) ? 95 : 
								(mantraLevel == 2) ? 96 :
								(mantraLevel == 3) ? 100 :
								(mantraLevel == 4) ? 110 : 120;
                
                if (isComp && (compBuf.size() * 100 < rawBuf.size() * threshold)) {
                    blockMap.push_back(_128); 
                    outputBlocks.push_back(compBuf); 
                    totalCompressedSize += (compBuf.size() + 4); 
                    anyBlockCompressed = true;
                } else { 
                    blockMap.push_back(_86); 
                    outputBlocks.push_back(rawBuf); 
                    totalCompressedSize += chunk; 
                }
                
                processed += chunk;
                _43 curPct = (_43)((processed * 100) / totalSize);
                _15 (curPct != lastPct) { 
                    SendMessage(hBar, PBM_SETPOS, (WPARAM)curPct, 0); 
                    SendMessage(hStatus, WM_SETTEXT, 0, (LPARAM)"COMPRESSING..."); 
                    lastPct = curPct; 
                }
            }
            
            _94 mapBytes = (blockMap.size() + 7) / 8; 
            _94 headerOverhead = 1 + 4 + mapBytes; 
            _94 finalDataSize = totalCompressedSize + headerOverhead;
            
            if (anyBlockCompressed && finalDataSize < totalSize) {
                entry.mantraLevel = (uint8_t)mantraLevel;
                entry.compSize = (uint32_t)finalDataSize;
                _184 fl = (1<<7); 
                out.put((_30)fl); 
                _43 bCnt = (_43)blockMap.size(); 
                out.write((_30*)&bCnt, 4);
                
                std::vector<_184> mapData(mapBytes, 0); 
                for (size_t i = 0; i < blockMap.size(); i++) 
                    if (blockMap[i]) 
                        mapData[i/8] |= (1 << (i%8)); 
                out.write((_30*)mapData.data(), mapBytes);
                
                _39 (_94 i = 0; i < outputBlocks.size(); i++) {
                    _15 (blockMap[i]) { 
                        _43 sz = (_43)outputBlocks[i].size(); 
                        out.write((_30*)&sz, 4); 
                        out.write((_30*)outputBlocks[i].data(), sz); 
                    } _41 { 
                        out.write((_30*)outputBlocks[i].data(), outputBlocks[i].size()); 
                    }
                    if (i % 10 == 0) out.flush();
                }
            } else {
                entry.mantraLevel = 0;
                entry.compSize = (uint32_t)totalSize; 
                _184 fl = 0; 
                out.put((_30)fl);
                in.clear(); 
                in.seekg(0, std::ios::beg); 
                std::vector<_30> copyBuf(COPY_CHUNK); 
                _94 copied = 0;
                while (copied < totalSize) { 
                    _94 chk = std::min((_94)COPY_CHUNK, totalSize - copied); 
                    in.read(copyBuf.data(), chk); 
                    out.write(copyBuf.data(), chk); 
                    copied += chk; 
                }
            }
            
            header.indexOffset = (uint32_t)out.tellp(); 
            out.write((_30*)&entry, sizeof(BastionFileEntry)); 
            out.seekp(0, std::ios::beg); 
            out.write((_30*)&header, sizeof(BastionHeader)); 
            in.close(); 
            out.close();
        } 
        _42 (mode EQ 85) { 
            std::ifstream in(inP, std::ios::binary | std::ios::ate); 
            if (!in.is_open()) { isRunning=_86; return; }
            _94 tbaSz = in.tellg(); 
            in.seekg(0, std::ios::beg);
            
            BastionHeader header; 
            in.read((_30*)&header, sizeof(BastionHeader));
            
            bool isTBA2 = (strncmp(header.magic, "TBA2", 4) == 0);
            bool isTBA3 = (strncmp(header.magic, "TBA3", 4) == 0);
            
            if (!isTBA2 && !isTBA3) { 
                isRunning = _86; 
                MessageBoxA(NULL, "Falsches Format!", "Error", MB_ICONERROR); 
                return; 
            }
            
            in.seekg(header.indexOffset, std::ios::beg); 
            BastionFileEntry entry; 
            in.read((_30*)&entry, sizeof(BastionFileEntry));
            
            _43 decompBlockSize;
            if (isTBA2) {
                decompBlockSize = BLOCK_3_SIZE;
            } else {
                switch(entry.mantraLevel) {
                    case 1: decompBlockSize = BLOCK_1_SIZE; break;
                    case 2: decompBlockSize = BLOCK_2_SIZE; break;
                    case 3: decompBlockSize = BLOCK_3_SIZE; break;
                    case 4: decompBlockSize = BLOCK_4_SIZE; break;
                    case 5: decompBlockSize = BLOCK_5_SIZE; break;
                    default: decompBlockSize = BLOCK_3_SIZE;
                }
            }
            
            _30 dirPath[MAX_PATH]; 
            lstrcpyA(dirPath, inP.c_str()); 
            PathRemoveFileSpecA(dirPath); 
            PathAppendA(dirPath, entry.name); 
            std::ofstream out(dirPath, std::ios::binary);
            
            in.seekg(entry.dataOffset, std::ios::beg); 
            _30 fb; 
            in.get(fb);
            _94 currentWritten = 0; 
            _94 effSz = entry.rawSize;
            
            if (entry.mantraLevel > 0) {
                _43 blkCount; 
                in.read((_30*)&blkCount, 4); 
                _94 mapBytes = (blkCount + 7) / 8;
                std::vector<_184> mapData(mapBytes); 
                in.read((_30*)mapData.data(), mapBytes);
                
                for (_43 i = 0; i < blkCount; i++) {
                    bool isComp = (mapData[i/8] >> (i%8)) & 1;
                    if (isComp) {
                        _43 sz; 
                        in.read((_30*)&sz, 4); 
                        std::vector<_184> cD(sz), rD; 
                        in.read((_30*)cD.data(), sz);
                        DecompressBlock(cD, rD, std::min((_94)decompBlockSize, effSz - currentWritten)); 
                        out.write((_30*)rD.data(), rD.size()); 
                        currentWritten += rD.size();
                    } else {
                        _94 toRead = std::min((_94)decompBlockSize, effSz - currentWritten); 
                        std::vector<_30> r(toRead); 
                        in.read(r.data(), toRead); 
                        out.write(r.data(), in.gcount()); 
                        currentWritten += in.gcount();
                    }
                }
            } else { 
                std::vector<_30> buf(COPY_CHUNK);
                while(currentWritten < effSz) { 
                    _94 chk = std::min((_94)COPY_CHUNK, effSz - currentWritten); 
                    in.read(buf.data(), chk); 
                    out.write(buf.data(), chk); 
                    currentWritten += chk; 
                }
            }
            in.close(); 
            out.close();
        }

        auto end = std::chrono::high_resolution_clock::now(); 
        std::chrono::duration<double> diff = end - start; 
        isRunning = _86; 
        SendMessage(hBar, PBM_SETPOS, 100, 0); 
        std::stringstream res; 
        res << "Universal ArShiva COMPLETE.\nTIME: " << std::fixed << std::setprecision(6) << diff.count() << "s";
        SendMessage(hStatus, WM_SETTEXT, 0, (LPARAM)res.str().c_str()); 
        MessageBoxA(NULL, res.str().c_str(), "Universal ArShiva", MB_OK);
    }
};

BastionEngine engine; 
Image* img = NULL;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    _82 (msg) {
        _28 WM_ASK_FILENAME: { 
            OPENFILENAMEA ofn = {0}; 
            _30 sz[260] = "capture.tba"; 
            ofn.lStructSize = sizeof(ofn); 
            ofn.lpstrFile = sz; 
            ofn.nMaxFile = 260; 
            ofn.lpstrFilter = "TBA Files\0*.tba\0"; 
            ofn.Flags = OFN_OVERWRITEPROMPT; 
            _15 (GetSaveFileNameA(&ofn)) { g_lastCaptureName = sz; } 
            _96 0; 
        }
        _28 WM_CREATE: {
            hFont = CreateFont(16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS, "Segoe UI"); 
            hDarkBrush = CreateSolidBrush(RGB(30, 30, 30));
            hStatus = CreateWindowA("STATIC", "Universal ArShiva READY", WS_VISIBLE|WS_CHILD|SS_CENTER, 20, 270, 285, 20, hwnd, NULL, NULL, NULL); 
            SendMessage(hStatus, WM_SETFONT, (WPARAM)hFont, TRUE);
            hBar = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_CHILD|WS_VISIBLE, 20, 295, 285, 10, hwnd, NULL, NULL, NULL);
            hPackBtn = CreateWindowA("BUTTON", "PACK", WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON|BS_FLAT, 15, 315, 140, 45, hwnd, (HMENU)1, NULL, NULL); 
            SendMessage(hPackBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
            hUnpackBtn = CreateWindowA("BUTTON", "UNPACK", WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON|BS_FLAT, 165, 315, 140, 45, hwnd, (HMENU)2, NULL, NULL); 
            SendMessage(hUnpackBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
            hCaptureBtn = CreateWindowA("BUTTON", "CAPTURE NOISE", WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON|BS_FLAT, 15, 365, 140, 30, hwnd, (HMENU)3, NULL, NULL); 
            SendMessage(hCaptureBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
			hCaptureDeepBtn = CreateWindowA("BUTTON", "ETHER SCAN (16-BIT)", WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON|BS_FLAT, 15, 405, 140, 30, hwnd, (HMENU)4, NULL, NULL); 
            SendMessage(hCaptureDeepBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
            
            hMantraCombo = CreateWindowA("COMBOBOX", NULL, WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST, 165, 365, 140, 200, hwnd, NULL, NULL, NULL); 
            SendMessage(hMantraCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hMantraCombo, CB_ADDSTRING, 0, (LPARAM)"1 MANTRA");
            SendMessage(hMantraCombo, CB_ADDSTRING, 0, (LPARAM)"2 MANTRA");
            SendMessage(hMantraCombo, CB_ADDSTRING, 0, (LPARAM)"3 MANTRA");
            SendMessage(hMantraCombo, CB_ADDSTRING, 0, (LPARAM)"4 MANTRA");
            SendMessage(hMantraCombo, CB_ADDSTRING, 0, (LPARAM)"5 MANTRA");
            SendMessage(hMantraCombo, CB_SETCURSEL, 2, 0);
			
			hEmitBtn = CreateWindowA("BUTTON", "RESONATE (EMIT)", WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON|BS_FLAT, 165, 405, 140, 30, hwnd, (HMENU)5, NULL, NULL); 
			SendMessage(hEmitBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
			// Eigenen Namen (hMythBtn) verwenden!
			HWND hMythBtn = CreateWindowA("BUTTON", "MYTH WORDS", WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON|BS_FLAT, 165, 437, 140, 30, hwnd, (HMENU)6, NULL, NULL); 
			SendMessage(hMythBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
			
			img = LoadImageFromResource();
			InvalidateRect(hwnd, NULL, TRUE); 
			_96 0;
        }
        _28 WM_COMMAND: {
            _43 id = LOWORD(wp);
            
            // --- ID 1 & 2 (PACK / UNPACK) ---
            _15 ((id == 1 || id == 2) && !engine.isRunning) {
                OPENFILENAMEA ofn = {0}; 
                _30 sz[260] = {0}; 
                ofn.lStructSize = sizeof(ofn); 
                ofn.lpstrFile = sz; 
                ofn.nMaxFile = 260; 
                ofn.Flags = OFN_FILEMUSTEXIST;
                if (GetOpenFileNameA(&ofn)) {
                    engine.isRunning = _128; 
                    _43 cM = (id == 1 ? 84 : 85); 
                    _43 selectedMantra = SendMessage(hMantraCombo, CB_GETCURSEL, 0, 0) + 1;
                    std::thread t(&BastionEngine::Process, &engine, (_87)sz, cM, selectedMantra); 
                    t.detach();
                }
            } 
            // --- ID 3 & 4 (CAPTURE NOISE / DEEP SCAN) ---
            _42 (id == 3 || id == 4) { 
                if (!g_keepCapturing && !engine.isRunning) { 
                    g_keepCapturing = true; 
                    engine.isRunning = _128; 
                    
                    bool isDeepScan = (id == 4);
                    
                    if (isDeepScan) {
                        SetWindowTextA(hCaptureDeepBtn, "STOP SCAN"); 
                    } else {
                        SetWindowTextA(hCaptureBtn, "STOP SCAN"); 
                    }
                    
                    std::thread t([&, isDeepScan]() { 
                        if (isDeepScan) {
                            engine.CaptureEnergy("Aether", true); 
                        } else {
                            engine.CaptureEnergy("Umgebung", false); 
                        }
                        
                        engine.isRunning = _86; 
                        g_keepCapturing = false; 
                        
                        SetWindowTextA(hCaptureBtn, "CAPTURE NOISE");
                        SetWindowTextA(hCaptureDeepBtn, "DEEP SCAN (16-BIT)");
                    }); 
                    t.detach(); 
                } else { 
                    g_keepCapturing = false; 
                    SendMessage(hStatus, WM_SETTEXT, 0, (LPARAM)"STOPPING SCAN..."); 
                } 
            }
            // --- ID 5 (RESONATE / EMIT) ---
            // ======== BUTTON 5 (RESONATE) ========
            _42 (id == 5) {
                if (!g_keepCapturing && !engine.isRunning) {
                    OPENFILENAMEA ofn = {0}; 
                    _30 sz[260] = {0}; 
                    ofn.lStructSize = sizeof(ofn); 
                    ofn.lpstrFile = sz; 
                    ofn.nMaxFile = 260; 
                    ofn.lpstrFilter = "RAW Energy Files\0*.raw\0"; 
                    ofn.Flags = OFN_FILEMUSTEXIST;
                    
                    if (GetOpenFileNameA(&ofn)) {
                        engine.isRunning = _128; 
                        g_keepCapturing = true; 
                        SetWindowTextA(hEmitBtn, "STOP RESONANCE");
                        
                        std::string safeFileName(sz);
                        std::thread t([&, safeFileName]() { 
                            engine.EmitEnergy(safeFileName); 
                            SetWindowTextA(hEmitBtn, "RESONATE (EMIT)");
                        }); 
                        t.detach();
                    }
                } else if (engine.isRunning) {
                    g_keepCapturing = false;
                    SendMessage(hStatus, WM_SETTEXT, 0, (LPARAM)"ABORTING EMISSION...");
                }
            } // <--- HIER ENDET BUTTON 5 (Die magische fehlende Klammer!)


            // ======== BUTTON 6 (MYTH WORDS) ========
            _42 (id == 6) { 
                OPENFILENAMEA ofn;       
                _30 szFile[260] = {0};  

                ZeroMemory(&ofn, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd;
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = sizeof(szFile);
                
                // ACHTUNG: Hier KEIN "Ä" benutzen! Win32 hasst Umlaute an dieser Stelle.
                ofn.lpstrFilter = "Aether Files (*.raw;*.txt)\0*.raw;*.txt\0All Files\0*.*\0";
                ofn.nFilterIndex = 1;
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                if (GetOpenFileNameA(&ofn)) {
                    std::string inputPath = szFile;
                    std::string outputPath = inputPath + "_oracle.txt";
                    ExtractEtherWords(inputPath, outputPath);
                }
            }

            break;

            _96 0;
        }
        _28 WM_PAINT: { 
            PAINTSTRUCT ps; 
            HDC hdc = BeginPaint(hwnd, &ps); 
            RECT rc; 
            GetClientRect(hwnd, &rc); 
            FillRect(hdc, &rc, hDarkBrush); 
            Graphics g(hdc); 
            if (img) { 
                _54 sc = 0.75f; 
                g.DrawImage(img, (rc.right - (_43)(img->GetWidth()*sc))/2, 20, (_43)(img->GetWidth()*sc), (_43)(img->GetHeight()*sc)); 
            } 
            EndPaint(hwnd, &ps); 
            _96 0; 
        }
        _28 WM_DESTROY: { 
            DeleteObject(hFont); 
            DeleteObject(hDarkBrush); 
            PostQuitMessage(0); 
            _96 0; 
        }
    }
    _96 DefWindowProcA(hwnd, msg, wp, lp); 
}
void ExtractEtherWords(const std::string& inputFile, const std::string& outputFile) {
    std::ifstream in(inputFile, std::ios::binary);
    std::ofstream number("crystal_numbers.txt", std::ios::app);  // Richtiger Name!
    
    if (!in) {
        MessageBoxA(NULL, "Konnte Input-Datei nicht öffnen!", "Fehler", MB_OK | MB_ICONERROR);
        return;
    }
    if (!number) {
        MessageBoxA(NULL, "Konnte crystal_numbers.txt nicht erstellen!", "Fehler", MB_OK | MB_ICONERROR);
        return;
    }
    
    std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    
    std::ofstream out(outputFile, std::ios::binary);
    if (!out) return;
    
    // UTF-16LE BOM
    unsigned char bom[] = { 0xFF, 0xFE };
    out.write((char*)bom, 2);
    
    for (size_t i = 0; i + 1 < buffer.size(); i += 2) {
        // Korrekte UTF-16LE Dekodierung (Little Endian!)
        uint16_t character = buffer[i] | (buffer[i + 1] << 8);
        
        // === STUFE 1: 32-BIT ANOMALIEN (Surrogate Pairs) ===
        if (character >= 0xD800 && character <= 0xDBFF) {
            if (i + 3 < buffer.size()) {
                uint16_t lowSurrogate = buffer[i + 2] | (buffer[i + 3] << 8);
                
                if (lowSurrogate >= 0xDC00 && lowSurrogate <= 0xDFFF) {
                    // 32-Bit Zeichen gefunden - Original-Bytes schreiben (nicht verändert!)
                    out.write((char*)&buffer[i], 4);
                    number << "UTF32: 0x" << std::hex 
                           << ((((uint32_t)character - 0xD800) << 10) + (lowSurrogate - 0xDC00) + 0x10000)
                           << std::dec << "\n";
                    
                    // Leerzeichen (UTF-16LE)
                    unsigned char space[] = { 0x20, 0x00 };
                    out.write((char*)space, 2);
                    
                    i += 2;
                    continue;
                }
            }
        }
        
        // === STUFE 2: 16-BIT ANOMALIEN ===
        // WICHTIG: Original character verwenden, NICHT buffer verändern!
        // KEIN: character = character >> 2;
        // KEIN: buffer[i] = character;
        
        bool isChinese  = (character >= 0x4E00 && character <= 0x9FFF);
        bool isJapanese = (character >= 0x3040 && character <= 0x30FF) ||  // Hiragana/Katakana
                          (character >= 0x31F0 && character <= 0x31FF) ||  // Katakana Erweiterung
                          (character >= 0xFF65 && character <= 0xFF9F);    // Halfwidth Katakana
        bool isKorean   = (character >= 0xAC00 && character <= 0xD7AF) ||  // Hangul Syllables
                          (character >= 0x1100 && character <= 0x11FF) ||  // Hangul Jamo
                          (character >= 0x3130 && character <= 0x318F);    // Hangul Compatibility
        bool isTibetan  = (character >= 0x0F00 && character <= 0x0FFF);
        bool isRunic    = (character >= 0x16A0 && character <= 0x16FF);
        bool isCeltic   = (character >= 0x1680 && character <= 0x169F);
        bool isMystic   = (character >= 0x2600 && character <= 0x26FF);
        bool isCherokee = (character >= 0x13A0 && character <= 0x13FF);
        bool isBraille  = (character >= 0x2800 && character <= 0x28FF);
        bool isDingbats = (character >= 0x2700 && character <= 0x27BF);
        
        // Zusätzliche asiatische Bereiche
        bool isCJKExtA  = (character >= 0x3400 && character <= 0x4DBF);  // CJK Extension A
        bool isCJKExtB  = (character >= 0x20000 && character <= 0x2A6DF); // CJK Extension B (32-bit)
        bool isBopomofo = (character >= 0x3100 && character <= 0x312F);  // Zhuyin
        
        if (isChinese || isJapanese || isKorean || isTibetan || 
            isRunic || isCeltic || isMystic || isCherokee || 
            isBraille || isDingbats || isCJKExtA || isBopomofo) {
            
            // Original-Bytes aus dem Buffer schreiben (unverändert!)
            out.write((char*)&buffer[i], 2);
            
            // In crystal_numbers.txt protokollieren
            number << "U+" << std::hex << std::setw(4) << std::setfill('0') << character 
                   << std::dec << " | Bytes: 0x" << std::hex 
                   << (int)buffer[i] << " 0x" << (int)buffer[i+1] << std::dec;
            
            // Zeichen-Name/Kategorie
            if (isChinese) number << " [Chinese]";
            else if (isJapanese) number << " [Japanese]";
            else if (isKorean) number << " [Korean]";
            else if (isTibetan) number << " [Tibetan]";
            else if (isRunic) number << " [Runic]";
            else if (isCeltic) number << " [Celtic]";
            else if (isMystic) number << " [Mystic]";
            else if (isCherokee) number << " [Cherokee]";
            else if (isBraille) number << " [Braille]";
            else if (isDingbats) number << " [Dingbats]";
            
            number << "\n";
            
            // Leerzeichen (UTF-16LE)
            unsigned char space[] = { 0x20, 0x00 };
            out.write((char*)space, 2);
        }
    }
    
    out.close();
    number.close();
    
    MessageBoxA(NULL, ("Extracted to: " + outputFile + "\nLogged to: crystal_numbers.txt").c_str(), 
                "ArShiva Oracle", MB_OK | MB_ICONINFORMATION);
}
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    InitCommonControls(); 
    GdiplusStartupInput gsi; 
    ULONG_PTR gst; 
    GdiplusStartup(&gst, &gsi, NULL);
    WNDCLASSA wc = {0}; 
    wc.lpfnWndProc = WndProc; 
    wc.hInstance = hInstance; 
    wc.hbrBackground = NULL; 
    wc.lpszClassName = "Universal ArShiva"; 
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(100)); 
    RegisterClassA(&wc);
    CreateWindowA("Universal ArShiva", "Universal ArShiva", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 325, 510, NULL, NULL, hInstance, NULL);
    MSG m; 
    while (GetMessage(&m, NULL, 0, 0)) { 
        TranslateMessage(&m); 
        DispatchMessage(&m); 
    } 
    GdiplusShutdown(gst); 
    return (int)m.wParam;
}