// Stub for Clipboard. The real clipboard.cpp is a GUI clipboard bridge that
// drags in the whole editor stack. PatchSection::getSelectionAsPatch() is the
// only referencer, and it is never called by the problem checker. The QObject
// meta machinery comes from moc_clipboard.cpp; these four methods are the only
// non-moc Clipboard symbols patchsection.o references.

#include "clipboard.h"

Clipboard::Clipboard() : lockGlobalClipboardUntil(0) {}
Clipboard::~Clipboard() {}
void Clipboard::copyFromSelection(const Selection *, const PatchSection *) {}
Patch *Clipboard::getAsPatch() const { return nullptr; }
