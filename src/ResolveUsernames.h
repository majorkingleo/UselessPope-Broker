#pragma once

// One-shot maintenance task: fills in missing usernames in the archived
// PLAY_QUEUE_CHUNKS rows and in the "Protokoll der Predigt" (SERMON) entries
// that were written from them.
void resolve_protocol_usernames();
