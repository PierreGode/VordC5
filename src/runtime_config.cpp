#include "runtime_config.h"
#include "config.h"
#include <vector>

static SemaphoreHandle_t  s_namesMutex = nullptr;
static std::vector<String> s_skimmerNames;
static std::vector<String> s_pentoolNames;

static String normalizeName(const String& in) {
    String out;
    out.reserve(in.length());
    for (size_t i = 0; i < in.length(); i++) {
        char c = in[i];
        if (c == ' ' || c == '-' || c == '_' || c == '.') continue;
        out += (char)toupper((unsigned char)c);
    }
    return out;
}

static void seedDefaults(std::vector<String>& dst, const char* const* names) {
    for (int i = 0; names[i] != nullptr; i++) {
        dst.emplace_back(names[i]);
    }
}

void runtime_config_init() {
    if (s_namesMutex) return;
    s_namesMutex = xSemaphoreCreateMutex();
    seedDefaults(s_skimmerNames, SKIMMER_NAMES_DEFAULT);
    seedDefaults(s_pentoolNames, PENTOOL_NAMES_DEFAULT);
}

static bool nameInList(const String& name, const std::vector<String>& list) {
    if (!s_namesMutex || name.length() == 0) return false;
    const String nrm = normalizeName(name);
    bool found = false;
    if (xSemaphoreTake(s_namesMutex, portMAX_DELAY) == pdTRUE) {
        for (const auto& configured : list) {
            const String cn = normalizeName(configured);
            if (cn.length() == 0) continue;
            if (nrm == cn || nrm.startsWith(cn) || nrm.indexOf(cn) >= 0) {
                found = true;
                break;
            }
        }
        xSemaphoreGive(s_namesMutex);
    }
    return found;
}

static String listCsv(const std::vector<String>& list) {
    String out;
    if (!s_namesMutex) return out;
    if (xSemaphoreTake(s_namesMutex, portMAX_DELAY) == pdTRUE) {
        for (size_t i = 0; i < list.size(); i++) {
            if (i) out += ",";
            out += list[i];
        }
        xSemaphoreGive(s_namesMutex);
    }
    return out;
}

bool isSkimmerName(const String& name)  { return nameInList(name, s_skimmerNames); }
bool isPentoolName(const String& name)  { return nameInList(name, s_pentoolNames); }

String getSkimmerNamesCsv()  { return listCsv(s_skimmerNames); }
String getPentoolNamesCsv()  { return listCsv(s_pentoolNames); }
