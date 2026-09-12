#pragma once
// Per-patch circuit state (issue #42).
//
// The hardware saves one DROIDSTA.BIN per SD card and reloads it into whatever
// patch is on that card, matching circuits by type + per-type ordinal
// (manual/hardware.md §11.1). Its escape hatch for "I want different state for
// a different patch" is a second SD card. A Rack module has no card slot, so a
// vcvoid master keeps a STORE of snapshots instead, keyed by what the patch
// structurally IS, and picks the right one when a patch is loaded.
//
// Everything here is Rack-free and deterministic so it can be unit-tested
// headless; the plugin layer only serialises the store and renders the status
// line. Nothing in this file influences patch semantics.
#include "engine.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace droid {

// --- patch identity --------------------------------------------------------

// Structural fingerprint: a hash of the ordered list of CIRCUIT TYPES in the
// patch. Comments, register labels, parameter values and controller
// declarations are all excluded, so re-tuning a patch (or renaming its jacks,
// or swapping a p2b8 for a p4b2) keeps the fingerprint and therefore keeps the
// state; inserting, deleting or reordering a circuit changes it. Returned as 16
// lowercase hex characters. An empty/unparseable patch fingerprints the empty
// list rather than failing.
std::string patchFingerprint(const std::string& patchText);

// The optional explicit state key: a `# STATE: <id>` comment in the patch
// HEADER (the same region register labels live in — after the title, before the
// first circuit or `# ----` separator). Two patches carrying the same tag share
// a state lineage even when their file names differ. Returns "" when absent.
std::string patchStateTag(const std::string& patchText);

// Loose path normalisation for the "is this the same file?" test: backslashes
// to forward slashes, collapsed separators, dropped "./" segments and trailing
// slash. Case is preserved (a case-insensitive filesystem still hands Rack back
// the same spelling it was given), and no symlink/`..` resolution is attempted —
// this is a hint for choosing a migration source, never an authorisation check.
std::string normalizePatchPath(const std::string& path);

// Everything the store needs to know about a patch that is about to be loaded.
struct PatchIdentity {
    std::string fingerprint;   // patchFingerprint(text)
    std::string path;          // normalizePatchPath(path)
    std::string title;         // first comment line, for the status line
    std::string tag;           // patchStateTag(text), "" when absent
    bool empty() const { return fingerprint.empty(); }
};
PatchIdentity patchIdentity(const std::string& patchText, const std::string& path);

// --- the store -------------------------------------------------------------

// One saved snapshot plus the identity it was saved under.
struct StoredSnapshot {
    std::string fingerprint;
    std::string path;
    std::string title;
    std::string tag;
    int64_t savedAt = 0;       // unix seconds, 0 when unknown
    StateSnapshot state;
};

// How a freshly loaded patch got (or did not get) its state.
enum class StateOrigin {
    Fresh,      // nothing in the store relates to this patch
    Restored,   // exact structural match: this patch's own saved state
    Migrated,   // same file / same `# STATE:` tag, different structure
};

struct StateLookup {
    StateOrigin origin = StateOrigin::Fresh;
    const StoredSnapshot* source = nullptr;   // null iff origin == Fresh
};

// An UNBOUNDED, insertion-ordered set of snapshots, one per fingerprint.
// Deliberately uncapped: dialled-in state is the user's work, and silently
// dropping the oldest entry would lose a patch's state the moment someone
// cycled through enough others. Entries only ever leave the store when the user
// asks (Reset circuit state / the UAT bridge's reset-state).
class PatchStateStore {
public:
    // Which snapshot a patch about to be loaded should start from.
    //   1. exact fingerprint -> Restored
    //   2. else the newest entry with the same non-empty `# STATE:` tag,
    //      then the newest with the same non-empty path -> Migrated
    //   3. else Fresh
    // The tag wins over the path because it is explicit: a user who writes one
    // is saying "this is the same instrument" across renames and copies.
    StateLookup lookup(const PatchIdentity& id) const;

    // Upsert by fingerprint, refreshing path/title/tag/savedAt. A snapshot with
    // no entries still creates/updates the entry: "this patch's state is empty"
    // is information, and it keeps the path/tag lineage alive.
    void store(const PatchIdentity& id, StateSnapshot state, int64_t savedAt);

    const StoredSnapshot* find(const std::string& fingerprint) const;
    // Drop one entry (the "current snapshot only" reset). True if it existed.
    bool erase(const std::string& fingerprint);

    const std::vector<StoredSnapshot>& entries() const { return entries_; }
    std::vector<StoredSnapshot>& entries() { return entries_; }
    size_t size() const { return entries_.size(); }
    bool empty() const { return entries_.empty(); }
    void clear() { entries_.clear(); }

private:
    std::vector<StoredSnapshot> entries_;   // insertion order; never evicted
};

} // namespace droid
