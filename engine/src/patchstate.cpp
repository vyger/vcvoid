#include "patchstate.hpp"
#include "controllers.hpp"
#include "labels.hpp"
#include "parser.hpp"
#include <cctype>

namespace droid {

namespace {

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char) s[a])) a++;
    while (b > a && std::isspace((unsigned char) s[b - 1])) b--;
    return s.substr(a, b - a);
}

// FNV-1a, 64-bit. Chosen over anything cryptographic on purpose: the
// fingerprint is a cache key, not a security boundary, and it has to be
// reproducible across builds and platforms with no dependency.
uint64_t fnv1a(const std::string& s, uint64_t h = 0xcbf29ce484222325ull) {
    for (unsigned char c : s) {
        h ^= c;
        h *= 0x100000001b3ull;
    }
    return h;
}

std::string hex16(uint64_t h) {
    static const char* kDigits = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; i--) { out[size_t(i)] = kDigits[h & 0xf]; h >>= 4; }
    return out;
}

// Is this section name a controller declaration rather than a circuit? Only
// circuits carry state, so controllers stay out of the fingerprint: swapping a
// p2b8 for a p4b2 changes which registers exist, never which circuits hold what.
bool isControllerSection(const std::string& name) {
    return gen::findController(name) != nullptr;
}

// Section name as written between the brackets, lowercased for the header scan
// (the Forge's parser lowercases there, so "# [P2B8]" must still read as a
// controller declaration and leave the header open).
std::string sectionName(const std::string& line) {
    size_t close = line.find(']');
    if (close == std::string::npos || close < 2) return std::string();
    std::string n = line.substr(1, close - 1);
    for (char& ch : n) ch = char(std::tolower((unsigned char) ch));
    return n;
}

} // namespace

std::string patchFingerprint(const std::string& patchText) {
    ParseResult pr = parsePatch(patchText);
    uint64_t h = 0xcbf29ce484222325ull;
    for (const auto& sec : pr.sections) {
        if (isControllerSection(sec.name)) continue;
        h = fnv1a(sec.name, h);
        h = fnv1a("\n", h);
    }
    return hex16(h);
}

// Header scan, deliberately mirroring parseRegisterLabels' notion of "the
// header": the tag is only honoured before the first real circuit and before
// the first `# ----` section separator, so a `STATE:` line buried in a section's
// prose cannot silently rebind a patch's state lineage.
std::string patchStateTag(const std::string& patchText) {
    size_t pos = 0;
    while (pos <= patchText.size()) {
        size_t nl = patchText.find('\n', pos);
        std::string line = trim(patchText.substr(
            pos, nl == std::string::npos ? std::string::npos : nl - pos));
        pos = (nl == std::string::npos) ? patchText.size() + 1 : nl + 1;
        if (line.empty()) continue;

        if (line[0] == '#') {
            std::string c = trim(line.substr(1));
            if (c.empty()) continue;
            // "^----*$": a section separator ends the header.
            bool allDashes = c.size() >= 3;
            for (char ch : c) if (ch != '-') { allDashes = false; break; }
            if (allDashes) return "";
            // A commented-out circuit ends the header just as a live one does.
            if (c[0] == '[') {
                std::string n = sectionName(c);
                if (!n.empty() && !isControllerSection(n)) return "";
                continue;
            }
            // "STATE:" — uppercase, as documented, so it reads as a directive
            // and can never collide with a register label ("O1: ...").
            if (c.compare(0, 6, "STATE:") == 0) return trim(c.substr(6));
            continue;
        }
        if (line[0] == '[') {
            std::string n = sectionName(line);
            if (!n.empty() && !isControllerSection(n)) return "";
            continue;
        }
        // A parameter line or a syntax error: neither opens nor closes anything.
    }
    return "";
}

std::string normalizePatchPath(const std::string& path) {
    std::string s = trim(path);
    for (char& c : s) if (c == '\\') c = '/';
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '/' && !out.empty() && out.back() == '/') continue;   // collapse //
        out += s[i];
    }
    // Drop "./" segments (leading or after a separator).
    std::string cleaned;
    cleaned.reserve(out.size());
    for (size_t i = 0; i < out.size();) {
        bool atSegStart = cleaned.empty() || cleaned.back() == '/';
        if (atSegStart && out[i] == '.' && i + 1 < out.size() && out[i + 1] == '/') {
            i += 2;
            continue;
        }
        cleaned += out[i++];
    }
    if (cleaned.size() > 1 && cleaned.back() == '/') cleaned.pop_back();
    return cleaned;
}

PatchIdentity patchIdentity(const std::string& patchText, const std::string& path) {
    PatchIdentity id;
    id.fingerprint = patchFingerprint(patchText);
    id.path = normalizePatchPath(path);
    id.title = parseRegisterLabels(patchText).title;
    id.tag = patchStateTag(patchText);
    return id;
}

// --- PatchStateStore -------------------------------------------------------

const StoredSnapshot* PatchStateStore::find(const std::string& fingerprint) const {
    if (fingerprint.empty()) return nullptr;
    for (const auto& e : entries_)
        if (e.fingerprint == fingerprint) return &e;
    return nullptr;
}

StateLookup PatchStateStore::lookup(const PatchIdentity& id) const {
    if (const StoredSnapshot* exact = find(id.fingerprint))
        return {StateOrigin::Restored, exact};

    // Migration candidate: the most recently saved entry sharing the explicit
    // tag, else the most recently saved entry sharing the file path. savedAt
    // ties break towards the later entry (insertion order), which is also the
    // more recently written one.
    const StoredSnapshot* byTag = nullptr;
    const StoredSnapshot* byPath = nullptr;
    for (const auto& e : entries_) {
        if (e.fingerprint == id.fingerprint) continue;
        if (!id.tag.empty() && e.tag == id.tag)
            if (!byTag || e.savedAt >= byTag->savedAt) byTag = &e;
        if (!id.path.empty() && e.path == id.path)
            if (!byPath || e.savedAt >= byPath->savedAt) byPath = &e;
    }
    if (const StoredSnapshot* src = byTag ? byTag : byPath)
        return {StateOrigin::Migrated, src};
    return {};
}

void PatchStateStore::store(const PatchIdentity& id, StateSnapshot state,
                            int64_t savedAt) {
    if (id.fingerprint.empty()) return;
    for (auto& e : entries_) {
        if (e.fingerprint != id.fingerprint) continue;
        e.path = id.path;
        e.title = id.title;
        e.tag = id.tag;
        e.savedAt = savedAt;
        e.state = std::move(state);
        return;
    }
    StoredSnapshot e;
    e.fingerprint = id.fingerprint;
    e.path = id.path;
    e.title = id.title;
    e.tag = id.tag;
    e.savedAt = savedAt;
    e.state = std::move(state);
    entries_.push_back(std::move(e));
}

bool PatchStateStore::erase(const std::string& fingerprint) {
    for (size_t i = 0; i < entries_.size(); i++) {
        if (entries_[i].fingerprint != fingerprint) continue;
        entries_.erase(entries_.begin() + long(i));
        return true;
    }
    return false;
}

} // namespace droid
