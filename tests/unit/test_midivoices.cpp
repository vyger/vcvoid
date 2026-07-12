#include "harness.hpp"
#include "src/midivoices.hpp"

// Spec: manual/circuits/midiin.md
//   * roundrobin (rr), line 339: default 0 => search for a free voice always
//     starts at pitch1 (lowest-numbered free output); "the notes played first
//     will always be played at the lowest numbered outputs". rr=1 => outputs
//     scanned round-robin ("like a rotating switch"), each output gets an equal
//     chance at the next note. "When all outputs are currently used by a note,
//     roundrobin has no influence. Here voiceallocation selects which of the
//     notes will be dropped."
//   * voiceallocation (va), line 340: when more notes are needed than voices
//     exist: 0 = the OLDEST currently-playing note is cancelled (default);
//     1 = the new note is not played and simply omitted; 2 = the LOWEST note is
//     cancelled; 3 = the HIGHEST note is cancelled.
//   * notegap is a time-domain gate feature (line 341) and is intentionally NOT
//     modelled by the allocator.
//
// Chosen readings where the manual is silent (documented in the report):
//   * A noteOn for a note already held retriggers the SAME voice (one active
//     voice per note number), updating velocity; no second voice is spent.
//   * noteOff releases only the voice whose held note matches (there is at most
//     one such voice per the reuse rule above).
//   * In va modes 2/3 the lowest/highest currently-playing note is cancelled
//     literally, and the new note takes that voice.

using droid::MidiVoiceAllocator;

// --- single-voice last-note priority (va=0 default, 1 voice) ------------------
TEST(midivoices_single_voice_last_note_priority) {
    MidiVoiceAllocator a;
    a.configure(1, /*roundRobin=*/false, /*allocationMode=*/0);
    CHECK(a.numVoices() == 1);
    CHECK(a.noteOn(60, 100) == 0);
    CHECK(a.voice(0).gate && a.voice(0).note == 60);
    // Second note steals the (only, hence oldest) voice: last note wins.
    CHECK(a.noteOn(64, 90) == 0);
    CHECK(a.voice(0).gate && a.voice(0).note == 64 && a.voice(0).velocity == 90);
}

// --- N-voice fill order: non-roundrobin fills lowest-numbered first ----------
TEST(midivoices_fill_order_lowest_first) {
    MidiVoiceAllocator a;
    a.configure(4, false, 0);
    CHECK(a.noteOn(60, 10) == 0);
    CHECK(a.noteOn(62, 20) == 1);
    CHECK(a.noteOn(64, 30) == 2);
    CHECK(a.voice(0).note == 60 && a.voice(1).note == 62 && a.voice(2).note == 64);
    CHECK(!a.voice(3).gate);
}

// --- roundrobin on/off difference -------------------------------------------
// After a voice frees, non-roundrobin restarts the search at voice 0, while
// roundrobin continues rotating from the last-used voice.
TEST(midivoices_roundrobin_off_reuses_voice0) {
    MidiVoiceAllocator a;
    a.configure(3, /*roundRobin=*/false, 0);
    CHECK(a.noteOn(60, 100) == 0);
    a.noteOff(60);                    // voice 0 freed
    CHECK(a.noteOn(62, 100) == 0);    // non-rr restarts at voice 0
}

TEST(midivoices_roundrobin_on_advances_start_voice) {
    MidiVoiceAllocator a;
    a.configure(3, /*roundRobin=*/true, 0);
    CHECK(a.noteOn(60, 100) == 0);    // start at 0, advance to 1
    a.noteOff(60);                    // voice 0 freed
    CHECK(a.noteOn(62, 100) == 1);    // rr continues from voice 1, not 0
    a.noteOff(62);
    CHECK(a.noteOn(64, 100) == 2);    // continues rotating
}

// --- voice stealing: va=0 cancels the OLDEST note ---------------------------
TEST(midivoices_steal_oldest_va0) {
    MidiVoiceAllocator a;
    a.configure(2, false, /*va=*/0);
    CHECK(a.noteOn(60, 100) == 0);    // oldest
    CHECK(a.noteOn(64, 100) == 1);
    CHECK(a.noteOn(67, 100) == 0);    // steals voice 0 (oldest = note 60)
    CHECK(a.voice(0).note == 67 && a.voice(1).note == 64);
}

// --- voice stealing: va=1 omits the new note --------------------------------
TEST(midivoices_steal_omit_new_va1) {
    MidiVoiceAllocator a;
    a.configure(2, false, /*va=*/1);
    CHECK(a.noteOn(60, 100) == 0);
    CHECK(a.noteOn(64, 100) == 1);
    CHECK(a.noteOn(67, 100) == -1);   // omitted, no voice affected
    CHECK(a.voice(0).note == 60 && a.voice(1).note == 64);
}

// --- voice stealing: va=2 cancels the LOWEST note ---------------------------
TEST(midivoices_steal_lowest_va2) {
    MidiVoiceAllocator a;
    a.configure(2, false, /*va=*/2);
    CHECK(a.noteOn(64, 100) == 0);
    CHECK(a.noteOn(60, 100) == 1);    // 60 is the lowest held
    CHECK(a.noteOn(67, 100) == 1);    // steals voice holding lowest note (60)
    CHECK(a.voice(0).note == 64 && a.voice(1).note == 67);
}

// --- voice stealing: va=3 cancels the HIGHEST note --------------------------
TEST(midivoices_steal_highest_va3) {
    MidiVoiceAllocator a;
    a.configure(2, false, /*va=*/3);
    CHECK(a.noteOn(60, 100) == 0);
    CHECK(a.noteOn(67, 100) == 1);    // 67 is the highest held
    CHECK(a.noteOn(64, 100) == 1);    // steals voice holding highest note (67)
    CHECK(a.voice(0).note == 60 && a.voice(1).note == 64);
}

// --- noteOff releases only the matching voice -------------------------------
TEST(midivoices_noteoff_releases_only_matching_voice) {
    MidiVoiceAllocator a;
    a.configure(3, false, 0);
    a.noteOn(60, 100);   // v0
    a.noteOn(64, 100);   // v1
    a.noteOn(67, 100);   // v2
    CHECK(a.noteOff(64) == 1);
    CHECK(a.voice(0).gate && a.voice(0).note == 60);
    CHECK(!a.voice(1).gate);
    CHECK(a.voice(2).gate && a.voice(2).note == 67);
    CHECK(a.noteOff(99) == -1);   // note not held: no voice affected
}

// --- same note re-struck while held reuses the same voice -------------------
TEST(midivoices_same_note_restruck_reuses_voice) {
    MidiVoiceAllocator a;
    a.configure(3, false, 0);
    CHECK(a.noteOn(60, 100) == 0);
    CHECK(a.noteOn(60, 120) == 0);    // same note -> same voice, new velocity
    CHECK(a.voice(0).gate && a.voice(0).note == 60 && a.voice(0).velocity == 120);
    CHECK(!a.voice(1).gate);          // no second voice consumed
    CHECK(!a.voice(2).gate);
}

// --- allNotesOff clears every gate ------------------------------------------
TEST(midivoices_all_notes_off) {
    MidiVoiceAllocator a;
    a.configure(4, false, 0);
    a.noteOn(60, 100);
    a.noteOn(64, 100);
    a.noteOn(67, 100);
    a.allNotesOff();
    for (int i = 0; i < a.numVoices(); i++) CHECK(!a.voice(i).gate);
}

// --- configure clamps voice count to the 8-output hardware maximum ----------
TEST(midivoices_configure_clamps_voice_count) {
    MidiVoiceAllocator a;
    a.configure(99, false, 0);
    CHECK(a.numVoices() == 8);
    a.configure(0, false, 0);
    CHECK(a.numVoices() == 0);
    CHECK(a.noteOn(60, 100) == -1);   // nowhere to put it
}
