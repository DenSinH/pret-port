#include "gba/m4a_internal.h"
#include "log.h"

extern char SoundMainRAM_Buffer[0x800];
char SoundMainRAM[sizeof(SoundMainRAM_Buffer)];


void ply_fine(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) { log_fatal("ply_fine call"); }
void ply_goto(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) { log_fatal("ply_goto call"); }
void ply_patt(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) { log_fatal("ply_patt call"); }
void ply_pend(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) { log_fatal("ply_pend call"); }
void ply_rept(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) { log_fatal("ply_rept call"); }
//void ply_memacc(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) { }
void ply_prio(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) { log_fatal("ply_prio call"); }
void ply_tempo(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) { log_fatal("ply_tempo call"); }
void ply_keysh(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) { log_fatal("ply_keysh call"); }
void ply_voice(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) { log_fatal("ply_voice call"); }
void ply_vol(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) { log_fatal("ply_vol call"); }
void ply_pan(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) { log_fatal("ply_pan call"); }
void ply_bend(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) { log_fatal("ply_bend call"); }
void ply_bendr(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) { log_fatal("ply_bendr call"); }
void ply_lfos(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) { log_fatal("ply_lfos call"); }
void ply_lfodl(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) { log_fatal("ply_lfod call"); }
void ply_mod(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) { log_fatal("ply_mod call"); }
void ply_modt(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) { log_fatal("ply_modt call"); }
void ply_tune(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) { log_fatal("ply_tune call"); }
void ply_port(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) { log_fatal("ply_port call"); }
void ply_endtie(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) { log_fatal("ply_endtie call"); }
void ply_note(u32 note_cmd, struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) { log_fatal("ply_note call"); }


void MPlayJumpTableCopy(MPlayFunc *mplayJumpTable) {
  log_fatal("MPlayJumpTableCopy call");
}

void SoundMain(void) {
  log_fatal("SoundMain call");
}

void SoundMainBTM(void) {
  log_fatal("SoundMainBTM call");
}

void TrackStop(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
  log_fatal("TrackStop call");
}

void MPlayMain(struct MusicPlayerInfo * info) {
  log_fatal("MPlayMain call");
}

void RealClearChain(void *x) {
  log_fatal("RealClearChain call");
}

void m4aSoundVSync(void) {
  log_fatal("m4aSoundVSync call");
}