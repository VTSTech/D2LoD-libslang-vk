// platform/linux/d2_linux_shim_registry.cpp — [d2linux M1] the registry shim:
// the Win32 registry API over a file-backed store (PLAN.md §6 M1:
// "Registry shim: file-backed store under ~/.config/d2linux/").
//
// The store is a plain ini-shaped file, one section per registry key path
// (e.g. [HKLM\SOFTWARE\Blizzard Entertainment\Diablo II]), one line per
// value:  name=type:hexdata   (REG_DWORD as 8 hex digits, REG_SZ as quoted
// UTF-8, REG_BINARY as hex). The known first readers are the video-mode keys
// (D2Lang/strtable's language probe and the boot migration in Fog) and
// Storm's SReg wrapper. Flush-through: every write rewrites the file, the
// same durability the PE's RegFlushKey callers assume.
//
// HKEY roots map to fixed section prefixes; opened keys are paths into that
// namespace. "Keys" are implicit (a key exists when its path prefixes a
// section); the API surface the slice uses is value-level, which keeps the
// store honest without inventing a hierarchy database.
//
// PE safety: this file compiles only in the linux build (build.cmake).

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "platform/linux/d2_linux_compat.h"
#include "platform/linux/d2_linux_api.h"
// The shim's shared TSD (defined in d2_linux_shim_win32.cpp).
extern thread_local DWORD tls_lastError;

extern "C" {

// ---------------------------------------------------------------------------
// The store. One process-wide map; the file is the backing, re-read lazily.
// ---------------------------------------------------------------------------
struct D2RegValue {
    DWORD dwType;
    uint8_t* pbData;
    DWORD cbData;
};

static const size_t kMaxValues = 512;
static D2RegValue g_values[kMaxValues];
static char g_szKeys[kMaxValues][512];
static char g_szNames[kMaxValues][256];
static size_t g_nCount = 0;
static pthread_mutex_t g_csReg = PTHREAD_MUTEX_INITIALIZER;
static bool g_bLoaded = false;
static char g_szStorePath[512] = "";

static const char* kRootNames[] = {
    "HKEY_CLASSES_ROOT", "HKEY_CURRENT_USER", "HKEY_LOCAL_MACHINE",
    "HKEY_USERS", "", "HKEY_CURRENT_CONFIG",
};

static void d2_reg_store_path() {
    if (g_szStorePath[0]) return;
    const char* home = getenv("HOME");
    if (!home || !home[0]) home = "/tmp";
    const char* xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        snprintf(g_szStorePath, sizeof(g_szStorePath), "%s/d2linux/registry.ini", xdg);
    } else {
        snprintf(g_szStorePath, sizeof(g_szStorePath), "%s/.config/d2linux/registry.ini", home);
    }
}

static void d2_reg_hex(uint8_t* pb, DWORD cb, char* szOut, size_t nOut) {
    size_t n = 0;
    for (DWORD i = 0; i < cb && n + 2 < nOut; ++i) {
        static const char hex[] = "0123456789abcdef";
        szOut[n++] = hex[pb[i] >> 4];
        szOut[n++] = hex[pb[i] & 0xF];
    }
    szOut[n] = '\0';
}

static int d2_reg_unhex(const char* szHex, uint8_t* pbOut, DWORD cbMax) {
    DWORD n = 0;
    for (const char* p = szHex; p[0] && p[1] && n < cbMax; p += 2) {
        int hi = (p[0] >= '0' && p[0] <= '9') ? p[0] - '0' : (p[0] | 0x20) - 'a' + 10;
        int lo = (p[1] >= '0' && p[1] <= '9') ? p[1] - '0' : (p[1] | 0x20) - 'a' + 10;
        if (hi < 0 || hi > 15 || lo < 0 || lo > 15) return -1;
        pbOut[n++] = (uint8_t)((hi << 4) | lo);
    }
    return (int)n;
}

static void d2_reg_load() {
    if (g_bLoaded) return;
    g_bLoaded = true;
    d2_reg_store_path();
    FILE* f = fopen(g_szStorePath, "r");
    if (!f) return;
    char szLine[1600];
    char szSection[512] = "";
    while (fgets(szLine, sizeof(szLine), f)) {
        size_t nLen = strlen(szLine);
        while (nLen && (szLine[nLen - 1] == '\n' || szLine[nLen - 1] == '\r')) szLine[--nLen] = '\0';
        if (szLine[0] == '[') {
            char* close = strchr(szLine, ']');
            if (close) { *close = '\0'; snprintf(szSection, sizeof(szSection), "%s", szLine + 1); }
            continue;
        }
        char* eq = strchr(szLine, '=');
        if (!eq || !szSection[0]) continue;
        *eq = '\0';
        char* szName = szLine;
        char* szVal = eq + 1;
        if (g_nCount >= kMaxValues) break;
        D2RegValue* v = &g_values[g_nCount];
        memset(v, 0, sizeof(*v));
        if (strncmp(szVal, "dword:", 6) == 0) {
            v->dwType = REG_DWORD;
            v->cbData = 4;
            v->pbData = (uint8_t*)malloc(4);
            uint32_t nDw = (uint32_t)strtoul(szVal + 6, NULL, 16);
            memcpy(v->pbData, &nDw, 4);
        } else if (strncmp(szVal, "sz:", 3) == 0) {
            v->dwType = REG_SZ;
            v->cbData = (DWORD)strlen(szVal + 3) + 1;
            v->pbData = (uint8_t*)malloc(v->cbData);
            memcpy(v->pbData, szVal + 3, v->cbData);
        } else if (strncmp(szVal, "bin:", 4) == 0) {
            v->dwType = REG_BINARY;
            DWORD cbMax = (DWORD)strlen(szVal + 4) / 2;
            v->pbData = (uint8_t*)malloc(cbMax ? cbMax : 1);
            v->cbData = (DWORD)d2_reg_unhex(szVal + 4, v->pbData, cbMax);
        } else {
            continue;
        }
        snprintf(g_szKeys[g_nCount], 512, "%s", szSection);
        snprintf(g_szNames[g_nCount], 256, "%s", szName);
        g_nCount++;
    }
    fclose(f);
}

static void d2_reg_flush() {
    d2_reg_store_path();
    char szDir[512];
    snprintf(szDir, sizeof(szDir), "%s", g_szStorePath);
    char* slash = strrchr(szDir, '/');
    if (slash) { *slash = '\0'; mkdir(szDir, 0755); }
    FILE* f = fopen(g_szStorePath, "w");
    if (!f) return;
    for (size_t i = 0; i < g_nCount; ++i) {
        fprintf(f, "[%s]\n", g_szKeys[i]);
    }
    // (values grouped after their sections on the next pass below)
    for (size_t i = 0; i < g_nCount; ++i) {
        char szHex[1024];
        switch (g_values[i].dwType) {
            case REG_DWORD: {
                uint32_t nDw = 0;
                if (g_values[i].pbData && g_values[i].cbData >= 4) memcpy(&nDw, g_values[i].pbData, 4);
                snprintf(szHex, sizeof(szHex), "%08x", nDw);
                fprintf(f, "[%s]\n%s=dword:%s\n", g_szKeys[i], g_szNames[i], szHex);
                break;
            }
            case REG_SZ:
                fprintf(f, "[%s]\n%s=sz:%s\n", g_szKeys[i], g_szNames[i],
                        g_values[i].pbData ? (char*)g_values[i].pbData : "");
                break;
            default: {
                d2_reg_hex(g_values[i].pbData, g_values[i].cbData, szHex, sizeof(szHex));
                fprintf(f, "[%s]\n%s=bin:%s\n", g_szKeys[i], g_szNames[i], szHex);
                break;
            }
        }
    }
    fclose(f);
}

static int d2_reg_find(HKEY hKey, const char* szValue) {
    const char* szPath = (const char*)hKey;
    if (!szPath) return -1;
    for (size_t i = 0; i < g_nCount; ++i) {
        if (strcmp(g_szKeys[i], szPath) == 0 && strcmp(g_szNames[i], szValue) == 0) return (int)i;
    }
    return -1;
}

static const char* d2_root_name(HKEY hRoot) {
    uintptr_t n = (uintptr_t)hRoot;
    if (n >= 0x80000000u && n <= 0x80000005u) return kRootNames[n - 0x80000000u];
    return "HKEY_UNKNOWN";
}

// ---------------------------------------------------------------------------
// The API. Handles are the full key-path strings (malloc'd); value ops take
// the path + value name.
// ---------------------------------------------------------------------------
static LSTATUS __stdcall d2_reg_open(HKEY hRoot, const char* szSubKey, PHKEY phkResult) {
    if (!phkResult) return 87 /* ERROR_INVALID_PARAMETER */;
    char szPath[512];
    snprintf(szPath, sizeof(szPath), "%s%s%s", d2_root_name(hRoot),
             szSubKey && szSubKey[0] ? "\\" : "", szSubKey ? szSubKey : "");
    *phkResult = (HKEY)strdup(szPath);
    return ERROR_SUCCESS;
}

LSTATUS RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD, REGSAM, PHKEY phkResult) {
    pthread_mutex_lock(&g_csReg);
    d2_reg_load();
    LSTATUS rc = d2_reg_open(hKey, lpSubKey, phkResult);
    pthread_mutex_unlock(&g_csReg);
    return rc;
}

LSTATUS RegCreateKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD, LPSTR, DWORD, REGSAM,
                        const SECURITY_ATTRIBUTES*, PHKEY phkResult, LPDWORD lpdwDisposition) {
    pthread_mutex_lock(&g_csReg);
    d2_reg_load();
    LSTATUS rc = d2_reg_open(hKey, lpSubKey, phkResult);
    if (rc == ERROR_SUCCESS && lpdwDisposition) *lpdwDisposition = 2 /* REG_CREATED_NEW_KEY (honest: implicit) */;
    pthread_mutex_unlock(&g_csReg);
    return rc;
}

LSTATUS RegOpenKeyA(HKEY hKey, LPCSTR lpSubKey, PHKEY phkResult) {
    return RegOpenKeyExA(hKey, lpSubKey, 0, 0, phkResult);
}

LSTATUS RegCreateKeyA(HKEY hKey, LPCSTR lpSubKey, PHKEY phkResult) {
    return RegCreateKeyExA(hKey, lpSubKey, 0, NULL, 0, 0, NULL, phkResult, NULL);
}

LSTATUS RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType,
                         LPBYTE lpData, LPDWORD lpcbData) {
    (void)lpReserved;
    if (!lpValueName) return 87;
    pthread_mutex_lock(&g_csReg);
    d2_reg_load();
    int i = d2_reg_find(hKey, lpValueName);
    if (i < 0) {
        pthread_mutex_unlock(&g_csReg);
        tls_lastError = 2;
        return 2 /* ERROR_FILE_NOT_FOUND */;
    }
    D2RegValue* v = &g_values[i];
    if (lpType) *lpType = v->dwType;
    if (lpData && lpcbData) {
        DWORD cbCopy = *lpcbData < v->cbData ? *lpcbData : v->cbData;
        memcpy(lpData, v->pbData, cbCopy);
        *lpcbData = v->cbData;
        if (cbCopy < v->cbData) {
            pthread_mutex_unlock(&g_csReg);
            tls_lastError = 15100;
            return 234 /* ERROR_MORE_DATA */;
        }
    } else if (lpcbData) {
        *lpcbData = v->cbData;
    }
    pthread_mutex_unlock(&g_csReg);
    return ERROR_SUCCESS;
}

LSTATUS RegSetValueExA(HKEY hKey, LPCSTR lpValueName, DWORD, DWORD dwType,
                       const BYTE* lpData, DWORD cbData) {
    if (!lpValueName) return 87;
    pthread_mutex_lock(&g_csReg);
    d2_reg_load();
    int i = d2_reg_find(hKey, lpValueName);
    if (i < 0) {
        if (g_nCount >= kMaxValues) { pthread_mutex_unlock(&g_csReg); return 14 /* ERROR_OUTOFMEMORY */; }
        i = (int)g_nCount++;
        snprintf(g_szKeys[i], 512, "%s", (const char*)hKey);
        snprintf(g_szNames[i], 256, "%s", lpValueName);
        g_values[i].pbData = NULL;
        g_values[i].cbData = 0;
    }
    D2RegValue* v = &g_values[i];
    free(v->pbData);
    v->dwType = dwType;
    v->cbData = cbData;
    v->pbData = (uint8_t*)malloc(cbData ? cbData : 1);
    if (cbData) memcpy(v->pbData, lpData, cbData);
    d2_reg_flush();
    pthread_mutex_unlock(&g_csReg);
    return ERROR_SUCCESS;
}

LSTATUS RegDeleteValueA(HKEY hKey, LPCSTR lpValueName) {
    pthread_mutex_lock(&g_csReg);
    d2_reg_load();
    int i = d2_reg_find(hKey, lpValueName);
    if (i < 0) { pthread_mutex_unlock(&g_csReg); tls_lastError = 2; return 2; }
    free(g_values[i].pbData);
    g_values[i] = g_values[g_nCount - 1];
    memcpy(g_szKeys[i], g_szKeys[g_nCount - 1], 512);
    memcpy(g_szNames[i], g_szNames[g_nCount - 1], 256);
    g_nCount--;
    d2_reg_flush();
    pthread_mutex_unlock(&g_csReg);
    return ERROR_SUCCESS;
}

LSTATUS RegDeleteKeyA(HKEY hKey, LPCSTR lpSubKey) {
    // Keys are implicit; deleting a key drops the values under its path.
    pthread_mutex_lock(&g_csReg);
    d2_reg_load();
    char szPath[512];
    snprintf(szPath, sizeof(szPath), "%s\\%s", (const char*)hKey, lpSubKey ? lpSubKey : "");
    size_t nPath = strlen(szPath);
    size_t n = 0;
    for (size_t i = 0; i < g_nCount; ++i) {
        if (strncmp(g_szKeys[i], szPath, nPath) == 0) {
            free(g_values[i].pbData);
            continue;
        }
        if ((int)i != (int)n) {
            g_values[n] = g_values[i];
            memcpy(g_szKeys[n], g_szKeys[i], 512);
            memcpy(g_szNames[n], g_szNames[i], 256);
        }
        n++;
    }
    g_nCount = n;
    d2_reg_flush();
    pthread_mutex_unlock(&g_csReg);
    return ERROR_SUCCESS;
}

LSTATUS RegCloseKey(HKEY hKey) {
    free((void*)hKey);
    return ERROR_SUCCESS;
}

LSTATUS RegFlushKey(HKEY hKey) {
    (void)hKey;
    pthread_mutex_lock(&g_csReg);
    d2_reg_flush();
    pthread_mutex_unlock(&g_csReg);
    return ERROR_SUCCESS;
}

LSTATUS RegEnumValueA(HKEY hKey, DWORD dwIndex, LPSTR lpValueName, LPDWORD lpcchValueName,
                      LPDWORD, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    pthread_mutex_lock(&g_csReg);
    d2_reg_load();
    const char* szPath = (const char*)hKey;
    DWORD nSeen = 0;
    for (size_t i = 0; i < g_nCount; ++i) {
        if (strcmp(g_szKeys[i], szPath) != 0) continue;
        if (nSeen++ != dwIndex) continue;
        size_t nName = strlen(g_szNames[i]);
        if (lpValueName && lpcchValueName) {
            if (*lpcchValueName < nName + 1) { pthread_mutex_unlock(&g_csReg); return 234; }
            memcpy(lpValueName, g_szNames[i], nName + 1);
            *lpcchValueName = (DWORD)nName;
        }
        if (lpType) *lpType = g_values[i].dwType;
        if (lpData && lpcbData) {
            DWORD cbCopy = *lpcbData < g_values[i].cbData ? *lpcbData : g_values[i].cbData;
            memcpy(lpData, g_values[i].pbData, cbCopy);
            *lpcbData = g_values[i].cbData;
        }
        pthread_mutex_unlock(&g_csReg);
        return ERROR_SUCCESS;
    }
    pthread_mutex_unlock(&g_csReg);
    tls_lastError = 259 /* ERROR_NO_MORE_ITEMS */;
    return 259;
}

}  // extern "C"
