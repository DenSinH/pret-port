#include "gba/m4a_internal.h"
#include "log.h"

// a lot of work has been done by Kurausukun and atasro2 to port the PokeEmerald m4a engine
// this can be found at
// https://github.com/Kurausukun/pokeemerald/blob/pc_port/src/sound_mixer.c
// https://github.com/Kurausukun/pokeemerald/blob/pc_port/src/music_player.c

#define MIXER_UNLOCKED 0x68736D53
#define MIXER_LOCKED MIXER_UNLOCKED+1
#define PLAYER_UNLOCKED 0x68736D53
#define PLAYER_LOCKED PLAYER_UNLOCKED+1

#if ((-1 >> 1) == -1) && __has_builtin(__builtin_ctz)
#define FLOOR_DIV_POW2(a, b) ((a) >> __builtin_ctz(b))
#else
#define FLOOR_DIV_POW2(a, b) ((a) > 0 ? (a) / (b) : (((a) + 1 - (b)) / (b)))
#endif

extern const u8 gClockTable[];

#undef POKEMON_EXTENSIONS

#define VCOUNT_VBLANK  160
#define TOTAL_SCANLINES 228

extern char SoundMainRAM_Buffer[0x800];
char SoundMainRAM[sizeof(SoundMainRAM_Buffer)];

#define JUMP_TABLE_SIZE 36

extern void* gMPlayJumpTableTemplate[JUMP_TABLE_SIZE];

static u8 ConsumeTrackByte(struct MusicPlayerTrack *track) {
  u8 *ptr = track->cmdPtr++;
  return *ptr;
}


static u32 MidiKeyToFreq(struct WaveData *wav, u8 key, u8 pitch) {
  if (key > 178) {
    key = 178;
    pitch = 255;
  }

  // Alternatively, note = key % 12 and octave = 14 - (key / 12)
  u8 note = gScaleTable[key] & 0xF;
  u8 octave = gScaleTable[key] >> 4;
  u8 nextNote = gScaleTable[key + 1] & 0xF;
  u8 nextOctave = gScaleTable[key + 1] >> 4;

  u32 baseFreq1 = gFreqTable[note] >> octave;
  u32 baseFreq2 = gFreqTable[nextNote] >> nextOctave;

  u32 freqDifference = umul3232H32(baseFreq2 - baseFreq1, pitch << 24);
  // This is added by me. The real GBA and GBA BIOS don't verify this address, and as a result the
  // BIOS's memory can be dumped.
  u32 freq = wav->freq;
  return umul3232H32(freq, baseFreq1 + freqDifference);
}

static void ChnVolSetAsm(struct SoundChannel *chan, struct MusicPlayerTrack *track) {
  s8 forcedPan = chan->rhythmPan;
  u32 rightVolume = (u8)(forcedPan + 128) * chan->velocity * track->volMR / 128 / 128;
  if (rightVolume > 0xFF) {
    rightVolume = 0xFF;
  }
  chan->rightVolume = rightVolume;

  u32 leftVolume = (u8)(127 - forcedPan) * chan->velocity * track->volML / 128 / 128;
  if (leftVolume > 0xFF) {
    leftVolume = 0xFF;
  }
  chan->leftVolume = leftVolume;
}


void ply_fine(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) {
  struct MusicPlayerTrack *r5 = track;
  for (struct SoundChannel *chan = track->chan; chan != NULL; chan = chan->nextChannelPointer) {
    if (chan->statusFlags & 0xC7) {
      chan->statusFlags |= 0x40;
    }
    ClearChain(chan);
  }
  track->flags = 0;
}

void ply_goto(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) {
  u8 *addr;
  memcpy(&addr, track->cmdPtr, sizeof(u8 *));
  track->cmdPtr = addr;
}

void ply_patt(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) {
  u8 level = track->patternLevel;
  if (level < 3) {
    track->patternStack[level] = track->cmdPtr + sizeof(u8 *);
    track->patternLevel++;
    ply_goto(info, track);
  } else {
    // Stop playing this track, as an indication to the music programmer that they need to quit
    // nesting patterns so darn much.
    ply_goto(info, track);
  }
}

void ply_pend(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) {
  if (track->patternLevel != 0) {
    u8 index = --track->patternLevel;
    track->cmdPtr = track->patternStack[index];
  }
}

void ply_rept(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) {
  if (*track->cmdPtr == 0) {
    // "Repeat 0 times" == loop forever
    track->cmdPtr++;
    ply_goto(info, track);
  } else {
    u8 repeatCount = ++track->repN;
    if (repeatCount < ConsumeTrackByte(track)) {
      ply_goto(info, track);
    } else {
      track->repN = 0;
      track->cmdPtr += sizeof(u8) + sizeof(u8 *);
    }
  }
}

void ply_prio(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) {
  track->priority = ConsumeTrackByte(track);
}

void ply_tempo(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) {
  u16 bpm = ConsumeTrackByte(track);
  bpm *= 2;
  info->tempoD = bpm;
  info->tempoI = (bpm * info->tempoU) / 256;
}

void ply_keysh(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) {
  track->keyShift = ConsumeTrackByte(track);
  track->flags |= 0xC;
}

void ply_voice(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) {
  u8 voice = *(track->cmdPtr++);
  struct ToneData *instrument = &info->tone[voice];
  track->tone = *instrument;
}

void ply_vol(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) {
  track->vol = ConsumeTrackByte(track);
  track->flags |= 0x3;
}

void ply_pan(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) {
  track->pan = ConsumeTrackByte(track) - 0x40;
  track->flags |= 0x3;
}

void ply_bend(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) {
  track->bend = ConsumeTrackByte(track) - 0x40;
  track->flags |= 0xC;
}

void ply_bendr(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) {
  track->bendRange = ConsumeTrackByte(track);
  track->flags |= 0xC;
}

void ply_lfos(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) {
  track->lfoSpeed = *(track->cmdPtr++);
  if (track->lfoSpeed == 0) {
    ClearModM(track);
  }
}

void ply_lfodl(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) {
  track->lfoDelay = ConsumeTrackByte(track);
}

void ply_mod(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) {
  track->mod = *(track->cmdPtr++);
  if (track->mod == 0) {
    ClearModM(track);
  }
}

void ply_modt(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) {
  u8 type = ConsumeTrackByte(track);
  if (type != track->modT) {
    track->modT = type;
    track->flags |= 0xF;
  }
}

void ply_tune(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) {
  track->tune = ConsumeTrackByte(track) - 0x40;
  track->flags |= 0xC;
}

void ply_port(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) {
  track->cmdPtr += 2;
}

void ply_endtie(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) {
  u8 key = *track->cmdPtr;
  if (key < 0x80) {
    track->key = key;
    track->cmdPtr++;
  } else {
    key = track->key;
  }

  struct SoundChannel *chan = track->chan;
  while (chan != NULL) {
    if (chan->statusFlags & 0x83 && (chan->statusFlags & 0x40) == 0 && chan->midiKey == key) {
      chan->statusFlags |= 0x40;
      return;
    }
    chan = chan->nextChannelPointer;
  }
}

void ply_note(u32 note_cmd, struct MusicPlayerInfo *info, struct MusicPlayerTrack *track) {
  struct SoundInfo *mixer = SOUND_INFO_PTR;

  // A note can be anywhere from 1 to 4 bytes long. First is always the note length...
  track->gateTime = gClockTable[note_cmd];
  if (*track->cmdPtr < 0x80) {
    // Then the note name...
    track->key = *(track->cmdPtr++);
    if (*track->cmdPtr < 0x80) {
      // Then the velocity...
      track->velocity = *(track->cmdPtr++);
      if (*track->cmdPtr < 0x80) {
        // Then a number to add ticks to get exotic or more precise note lengths without TIE.
        track->gateTime += *(track->cmdPtr++);
      }
    }
  }

  // sp14
  s8 forcedPan = 0;
  // First r4, then r9
  struct ToneData *instrument = &track->tone;
  // sp8
  u8 key = track->key;
  u8 type = instrument->type;

  if (type & (TONEDATA_TYPE_RHY | TONEDATA_TYPE_SPL)) {
    log_warn("spl instrument unimplemented");
//    u8 instrumentIndex;
//    if (instrument->type & TONEDATA_TYPE_SPL) {
//      instrumentIndex = instrument->keySplitTable[track->key];
//    } else {
//      instrumentIndex = track->key;
//    }
//
//    instrument = instrument->group + instrumentIndex;
//    if (instrument->type & (TONEDATA_TYPE_RHY | TONEDATA_TYPE_SPL)) {
//      return;
//    }
//    if (type & TONEDATA_TYPE_RHY) {
//      if (instrument->panSweep & 0x80) {
//        forcedPan = ((s8)(instrument->panSweep & 0x7F) - 0x40) * 2;
//      }
//      key = instrument->drumKey;
//    }
  }

  // sp10
  u16 priority = info->priority + track->priority;
  if (priority > 0xFF) {
    priority = 0xFF;
  }

  u8 cgbType = instrument->type & TONEDATA_TYPE_CGB;
  void* _chan = NULL;

  if (cgbType != 0) {
    struct CgbChannel *chan;
    if (mixer->cgbChans == NULL) {
      return;
    }
    // There's only one CgbChannel of a given type, so we don't need to loop to find it.
    chan = mixer->cgbChans + cgbType - 1;
    _chan = chan;

    // If this channel is running and not stopped,
    if ((chan->statusFlags & SOUND_CHANNEL_SF_ON)
        && (chan->statusFlags & SOUND_CHANNEL_SF_STOP) == 0) {
      // then make sure this note is higher priority (or same priority but from a later track).
      if (chan->priority > priority || (chan->priority == priority && chan->track < track)) {
        return;
      }
    }
  } else {
    struct SoundChannel *chan;
    u16 p = priority;
    struct MusicPlayerTrack *t = track;
    bool32 foundStoppingChannel = FALSE;
    chan = NULL;
    u8 maxChans = mixer->maxChans;
    struct SoundChannel *currChan = mixer->chans;

    for (u8 i = 0; i < maxChans; i++, currChan++) {
      if ((currChan->statusFlags & SOUND_CHANNEL_SF_ON) == 0) {
        // Hey, we found a completely inactive channel! Let's use that.
        chan = currChan;
        _chan = chan;
        break;
      }

      if (currChan->statusFlags & SOUND_CHANNEL_SF_STOP && !foundStoppingChannel) {
        // In the absence of a completely finalized channel, we can take over one that's about to
        // finalize. That's a tier above any channel that's currently playing a note.
        foundStoppingChannel = TRUE;
        p = currChan->priority;
        t = currChan->track;
        chan = currChan;
        _chan = chan;
      } else if ((currChan->statusFlags & SOUND_CHANNEL_SF_STOP && foundStoppingChannel)
                 || ((currChan->statusFlags & SOUND_CHANNEL_SF_STOP) == 0 && !foundStoppingChannel)) {
        // The channel we're checking is on the same tier, so check the priority and track order
        if (currChan->priority < p) {
          p = currChan->priority;
          t = currChan->track;
          chan = currChan;
          _chan = chan;
        } else if (currChan->priority == p && currChan->track > t) {
          t = currChan->track;
          chan = currChan;
          _chan = chan;
        } else if (currChan->priority == p && currChan->track == t) {
          chan = currChan;
          _chan = chan;
        }
      }
    }

  }

  if (_chan == NULL) {
    return;
  }
  ClearChain(_chan);
  struct SoundChannel* result_chan = _chan;
  result_chan->prevChannelPointer = NULL;
  result_chan->nextChannelPointer = track->chan;
  if (track->chan != NULL) {
    track->chan->prevChannelPointer = result_chan;
  }
  track->chan = result_chan;
  result_chan->track = track;

  track->lfoDelayC = track->lfoDelay;
  if (track->lfoDelay != 0) {
    ClearModM(track);
  }
  TrkVolPitSet(info, track);

  result_chan->gateTime = track->gateTime;
  result_chan->midiKey = track->key;
  result_chan->velocity = track->velocity;
  result_chan->priority = priority;
  result_chan->key = key;
  result_chan->rhythmPan = forcedPan;
  result_chan->type = instrument->type;
  result_chan->wav = instrument->wav;
  result_chan->attack = instrument->attack;
  result_chan->decay = instrument->decay;
  result_chan->sustain = instrument->sustain;
  result_chan->release = instrument->release;
  result_chan->pseudoEchoVolume = track->pseudoEchoVolume;
  result_chan->pseudoEchoLength = track->pseudoEchoLength;
  ChnVolSetAsm(result_chan, track);

  // Avoid promoting keyShiftCalculated to u8 by splitting the addition into a separate statement
  u16 transposedKey = result_chan->key;
  transposedKey += track->keyShiftX;
  if (transposedKey < 0) {
    transposedKey = 0;
  }

  if (cgbType != 0) {
    struct CgbChannel *cgbChan = (struct CgbChannel *)result_chan;
    cgbChan->length = instrument->length;
    if (instrument->pan_sweep & 0x80 || (instrument->pan_sweep & 0x70) == 0) {
      cgbChan->sweep = 8;
    } else {
      cgbChan->sweep = instrument->pan_sweep;
    }

    cgbChan->frequency = mixer->MidiKeyToCgbFreq(cgbType, transposedKey, track->pitM);
  } else {
#ifdef POKEMON_EXTENSIONS
    chan->ct = track->ct;
#endif
    result_chan->frequency = MidiKeyToFreq(result_chan->wav, transposedKey, track->pitM);
  }

  result_chan->statusFlags = SOUND_CHANNEL_SF_START;
  track->flags &= ~0xF;
}


void MPlayJumpTableCopy(MPlayFunc *mplayJumpTable) {
  for (int i = 0; i < JUMP_TABLE_SIZE; i++) {
    mplayJumpTable[i] = gMPlayJumpTableTemplate[i];
  }
}


// Returns TRUE if channel is still active after moving envelope forward a frame
//__attribute__((target("thumb")))
static inline bool32 TickEnvelope(struct SoundChannel *chan, struct WaveData *wav) {
  // MP2K envelope shape
  //                                                                 |
  // (linear)^                                                       |
  // Attack / \Decay (exponential)                                   |
  //       /   \_                                                    |
  //      /      '.,        Sustain                                  |
  //     /          '.______________                                 |
  //    /                           '-.       Echo (linear)          |
  //   /                 Release (exp) ''--..|\                      |
  //  /                                        \                     |

  u8 status = chan->statusFlags;
  if ((status & 0xC7) == 0) {
    return FALSE;
  }

  u8 env = 0;
  if ((status & 0x80) == 0) {
    env = chan->envelopeVolume;

    if (status & 4) {
      // Note-wise echo
      --chan->pseudoEchoVolume;
      if (chan->pseudoEchoVolume <= 0) {
        chan->statusFlags = 0;
        return FALSE;
      } else {
        return TRUE;
      }
    } else if (status & 0x40) {
      // Release
      chan->envelopeVolume = env * chan->release / 256U;
      u8 echoVol = chan->pseudoEchoVolume;
      if (chan->envelopeVolume > echoVol) {
        return TRUE;
      } else if (echoVol == 0) {
        chan->statusFlags = 0;
        return FALSE;
      } else {
        chan->statusFlags |= 4;
        return TRUE;
      }
    }

    switch (status & 3) {
      u16 newEnv;
      case 2:
        // Decay
        chan->envelopeVolume = env * chan->decay / 256U;

        u8 sustain = chan->sustain;
        if (chan->envelopeVolume <= sustain && sustain == 0) {
          // Duplicated echo check from Release section above
          if (chan->pseudoEchoVolume == 0) {
            chan->statusFlags = 0;
            return FALSE;
          } else {
            chan->statusFlags |= 4;
            return TRUE;
          }
        } else if (chan->envelopeVolume <= sustain) {
          chan->envelopeVolume = sustain;
          --chan->statusFlags;
        }
        break;
      case 3:
      attack:
        newEnv = env + chan->attack;
        if (newEnv > 0xFF) {
          chan->envelopeVolume = 0xFF;
          --chan->statusFlags;
        } else {
          chan->envelopeVolume = newEnv;
        }
        break;
      case 1: // Sustain
      default:
        break;
    }

    return TRUE;
  } else if (status & 0x40) {
    // Init and stop cancel each other out
    chan->statusFlags = 0;
    return FALSE;
  } else {
    // Init channel
    chan->statusFlags = 3;
#ifdef POKEMON_EXTENSIONS
    chan->current = wav->data + chan->ct;
        chan->ct = wav->size - chan->ct;
#else
    chan->currentPointer = wav->data;
    chan->count = wav->size;
#endif
    chan->fw = 0;
    chan->envelopeVolume = 0;
    // todo: check
//    if (wav->loopFlags & 0xC0) {
//      chan->statusFlags |= 0x10;
//    }
    if (wav->status & 0xC000) {
      chan->statusFlags |= 0x10;
    }
    goto attack;
  }
}

//__attribute__((target("thumb")))
static inline void GenerateAudio(struct SoundInfo *mixer, struct SoundChannel *chan, struct WaveData *wav, s8 *outBuffer, u16 samplesPerFrame, float sampleRateReciprocal) {/*, [[[]]]) {*/
  u8 v = chan->envelopeVolume * (mixer->masterVolume + 1) / 16U;
  chan->envelopeVolumeRight = chan->rightVolume * v / 256U;
  chan->envelopeVolumeLeft  = chan->leftVolume * v / 256U;

  s32 loopLen = 0;
  s8 *loopStart;
  if (chan->statusFlags & 0x10) {
    loopStart = wav->data + wav->loopStart;
    loopLen = wav->size - wav->loopStart;
  }
  s32 samplesLeftInWav = chan->count;
  s8 *current = chan->currentPointer;
  signed envR = chan->envelopeVolumeRight;
  signed envL = chan->envelopeVolumeLeft;
#ifdef POKEMON_EXTENSIONS
  if (chan->type & 0x30) {
    GeneratePokemonSampleAudio(mixer, chan, current, outBuffer, samplesPerFrame, sampleRateReciprocal, samplesLeftInWav, envR, envL, loopLen);
  }
  else
#endif
  if (chan->type & 8) {
    for (u16 i = 0; i < samplesPerFrame; i++, outBuffer+=2) {
      u8 c = *(current++);

      outBuffer[1] += (c * envR) / 32768.0f;
      outBuffer[0] += (c * envL) / 32768.0f;
      if (--samplesLeftInWav == 0) {
        samplesLeftInWav = loopLen;
        if (loopLen != 0) {
          current = loopStart;
        } else {
          chan->statusFlags = 0;
          return;
        }
      }
    }

    chan->count = samplesLeftInWav;
    chan->currentPointer = current;
  } else {
    float finePos = chan->fw;
    float romSamplesPerOutputSample = chan->frequency * sampleRateReciprocal;

    u16 b = current[0];
    u16 m = current[1] - b;
    current += 1;

    for (u16 i = 0; i < samplesPerFrame; i++, outBuffer+=2) {
      // Use linear interpolation to calculate a value between the current sample in the wav
      // and the next sample. Also cancel out the 9.23 stuff
      float sample = (finePos * m) + b;

      outBuffer[1] += (sample * envR) / 32768.0f;
      outBuffer[0] += (sample * envL) / 32768.0f;

      finePos += romSamplesPerOutputSample;
      u32 newCoarsePos = finePos;
      if (newCoarsePos != 0) {
        finePos -= (int)finePos;
        samplesLeftInWav -= newCoarsePos;
        if (samplesLeftInWav <= 0) {
          if (loopLen != 0) {
            current = loopStart;
            newCoarsePos = -samplesLeftInWav;
            samplesLeftInWav += loopLen;
            while (samplesLeftInWav <= 0) {
              newCoarsePos -= loopLen;
              samplesLeftInWav += loopLen;
            }
            b = current[newCoarsePos];
            m = current[newCoarsePos + 1] - b;
            current += newCoarsePos + 1;
          } else {
            chan->statusFlags = 0;
            return;
          }
        } else {
          b = current[newCoarsePos - 1];
          m = current[newCoarsePos] - b;
          current += newCoarsePos;
        }
      }
    }

    chan->fw = finePos;
    chan->count = samplesLeftInWav;
    chan->currentPointer = current - 1;
  }
}

void SampleMixer(struct SoundInfo *mixer, u32 scanlineLimit, u16 samplesPerFrame, s8 *outBuffer, u8 dmaCounter, u16 maxBufSize) {
  u32 reverb = mixer->reverb;
  if (reverb) {
    // The vanilla reverb effect outputs a mono sound from four sources:
    //  - L/R channels as they were mixer->framesPerDmaCycle frames ago
    //  - L/R channels as they were (mixer->framesPerDmaCycle - 1) frames ago
    s8 *tmp1 = outBuffer;
    s8 *tmp2;
    if (dmaCounter == 2) {
      tmp2 = mixer->pcmBuffer;
    } else {
      tmp2 = outBuffer + samplesPerFrame * 2;
    }
    u16 i = 0;
    do {
      s32 s = tmp1[0] + tmp1[1] + tmp2[0] + tmp2[1];
      s *= ((float)reverb / 512.0f);
      tmp1[0] = tmp1[1] = (u8)s;
      tmp1+=2;
      tmp2+=2;
    }
    while(++i < samplesPerFrame);
  } else {
    // memset(outBuffer, 0, samplesPerFrame);
    // memset(outBuffer + maxBufSize, 0, samplesPerFrame);
    for (int i = 0; i < samplesPerFrame; i++) {
      s8 *dst = &outBuffer[i*2];
      dst[1] = dst[0] = 0;
    }
  }

  // todo: this might be wrong
  s32 sampleRateReciprocal = mixer->divFreq;
  u8 numChans = mixer->maxChans;
  struct SoundChannel *chan = mixer->chans;

  for (int i = 0; i < numChans; i++, chan++) {
    struct WaveData *wav = chan->wav;

    if (scanlineLimit != 0) {
      // todo: fix this
      vu16 vcount = REG_VCOUNT;
      if (vcount < VCOUNT_VBLANK) {
        vcount += TOTAL_SCANLINES;
      }
      if (vcount >= scanlineLimit) {
        goto returnEarly;
      }
    }

    if (TickEnvelope(chan, wav))
    {

      GenerateAudio(mixer, chan, wav, outBuffer, samplesPerFrame, sampleRateReciprocal);
    }
  }
  returnEarly:
  mixer->ident = MIXER_UNLOCKED;
}


void SoundMain(void) {
  struct SoundInfo *mixer = SOUND_INFO_PTR;

  if (mixer->ident != MIXER_UNLOCKED) {
    return;
  }
  mixer->ident = MIXER_LOCKED;

  u32 maxLines = mixer->maxLines;
  if (mixer->maxLines != 0) {
    // todo: fix this
    u32 vcount = REG_VCOUNT;
    maxLines += vcount;
    if (vcount < VCOUNT_VBLANK) {
      maxLines += TOTAL_SCANLINES;
    }
  }

  if (mixer->MPlayMainHead != NULL) {
    mixer->MPlayMainHead(mixer->musicPlayerHead);
  }

  mixer->CgbSound();

  s32 samplesPerFrame = mixer->pcmSamplesPerVBlank;
  s8 *outBuffer = mixer->pcmBuffer;
  s32 dmaCounter = mixer->pcmDmaCounter;

  // todo: fix this
//  if (dmaCounter > 1) {
//    outBuffer += samplesPerFrame * (mixer->pcmDmaPeriod - (dmaCounter - 1)) * 2;
//  }

  //MixerRamFunc mixerRamFunc = ((MixerRamFunc)MixerCodeBuffer);
  SampleMixer(mixer, maxLines, samplesPerFrame, outBuffer, dmaCounter, PCM_DMA_BUF_SIZE);
#ifdef PORTABLE
  cgb_audio_generate(samplesPerFrame);
#endif
}

void SoundMainBTM(void* dest) {
  memset(dest, 0, 4 * 4);
}

void TrackStop(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {
  if (track->flags & 0x80) {
    for (struct SoundChannel *chan = track->chan; chan != NULL; chan = chan->nextChannelPointer) {
      if (chan->statusFlags != 0) {
        u8 cgbType = chan->type & 0x7;
        if (cgbType != 0) {
          struct SoundInfo *mixer = SOUND_INFO_PTR;
          mixer->CgbOscOff(cgbType);
        }
        chan->statusFlags = 0;
      }
      chan->track = NULL;
    }
    track->chan = NULL;
  }
}

void MPlayMain(struct MusicPlayerInfo * player) {
  struct SoundInfo *info = SOUND_INFO_PTR;

  if (player->ident != PLAYER_UNLOCKED) {
    return;
  }
  player->ident = PLAYER_LOCKED;

  if (player->MPlayMainNext != NULL) {
    player->MPlayMainNext(player->musicPlayerNext);
  }

  if (player->status & MUSICPLAYER_STATUS_PAUSE) {
    goto returnEarly;
  }
  FadeOutBody(player);
  if (player->status & MUSICPLAYER_STATUS_PAUSE) {
    goto returnEarly;
  }

  player->tempoC += player->tempoI;
  while (player->tempoC >= 150) {
    u16 trackBits = 0;

    for (u32 i = 0; i < player->trackCount; i++) {
      struct MusicPlayerTrack *currentTrack = player->tracks + i;
      struct SoundChannel *chan;
      if ((currentTrack->flags & MPT_FLG_EXIST) == 0) {
        continue;
      }
      trackBits |= (1 << i);

      chan = currentTrack->chan;
      while (chan != NULL) {
        if ((chan->statusFlags & SOUND_CHANNEL_SF_ON) == 0) {
          ClearChain(chan);
        } else if (chan->gateTime != 0 && --chan->gateTime == 0) {
          chan->statusFlags |= SOUND_CHANNEL_SF_STOP;
        }
        chan = chan->nextChannelPointer;
      }

      if (currentTrack->flags & MPT_FLG_START) {
        CpuFill32(0, currentTrack, 0x40);
        currentTrack->flags = MPT_FLG_EXIST;
        currentTrack->bendRange = 2;
        currentTrack->volX = 64;
        currentTrack->lfoSpeed = 22;
        currentTrack->tone.type = 1;
      }

      while (currentTrack->wait == 0) {
        u8 event = *currentTrack->cmdPtr;
        if (event < 0x80) {
          event = currentTrack->runningStatus;
        } else {
          currentTrack->cmdPtr++;
          if (event >= 0xBD) {
            currentTrack->runningStatus = event;
          }
        }

        if (event >= 0xCF) {
          info->plynote(event - 0xCF, player, currentTrack);
        } else if (event >= 0xB1) {
          void (*eventFunc)(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
          player->cmd = event - 0xB1;
          eventFunc = info->MPlayJumpTable[player->cmd];
          eventFunc(player, currentTrack);

          if (currentTrack->flags == 0) {
            goto nextTrack;
          }
        } else {
          currentTrack->wait = gClockTable[event - 0x80];
        }
      }

      currentTrack->wait--;

      if (currentTrack->lfoSpeed != 0 && currentTrack->mod != 0) {
        if (currentTrack->lfoDelayC != 0U) {
          currentTrack->lfoDelayC--;
          goto nextTrack;
        }

        currentTrack->lfoDelayC += currentTrack->lfoSpeed;

        s8 r;
        if (currentTrack->lfoSpeedC >= 0x40U && currentTrack->lfoSpeedC < 0xC0U) {
          r = 128 - currentTrack->lfoSpeedC;
        } else if (currentTrack->lfoSpeedC >= 0xC0U) {
          // Unsigned -> signed casts where the value is out of range are implementation defined.
          // Why not add a few extra lines to make behavior the same for literally everyone?
          r = currentTrack->lfoSpeedC - 256;
        } else {
          r = currentTrack->lfoSpeedC;
        }
        r = FLOOR_DIV_POW2(currentTrack->mod * r, 64);

        if (r != currentTrack->modM) {
          currentTrack->modM = r;
          if (currentTrack->modT == 0) {
            currentTrack->flags |= MPT_FLG_PITCHG;
          } else {
            currentTrack->flags |= MPT_FLG_VOLCHG;
          }
        }
      }

      nextTrack:;
    }

    player->clock++;
    if (trackBits == 0) {
      player->status = MUSICPLAYER_STATUS_PAUSE;
      goto returnEarly;
    }
    player->status = trackBits;
    player->tempoC -= 150;
  }

  u32 i = 0;

  do {
    struct MusicPlayerTrack *track = player->tracks + i;

    if ((track->flags & MPT_FLG_EXIST) == 0 || (track->flags & 0xF) == 0) {
      continue;
    }
    TrkVolPitSet(player, track);
    for (struct SoundChannel *chan = track->chan; chan != NULL; chan = chan->nextChannelPointer) {
      if ((chan->statusFlags & 0xC7) == 0) {
        ClearChain(chan);
        continue;
      }
      u8 cgbType = chan->type & 0x7;
      if (track->flags & MPT_FLG_VOLCHG) {
        ChnVolSetAsm(chan, track);
        if (cgbType != 0) {
          // todo: check cgbStatus
          chan->fw |= 10000;
        }
      }
      if (track->flags & MPT_FLG_PITCHG) {
        s32 key = chan->key + track->keyM;
        if (key < 0) {
          key = 0;
        }
        if (cgbType != 0) {
          chan->frequency = info->MidiKeyToCgbFreq(cgbType, key, track->pitM);
          // todo: check cgbStatus
          chan->fw |= 0x20000;
        } else {
          chan->frequency = MidiKeyToFreq(chan->wav, key, track->pitM);
        }
      }
    }
    track->flags &= ~0xF;
  }
  while(++i < player->trackCount);
  returnEarly:
  player->ident = PLAYER_UNLOCKED;
}

void RealClearChain(void* x) {
  struct SoundChannel* chan = x;
  struct MusicPlayerTrack *track = chan->track;
  if (chan->track == NULL) {
    return;
  }
  struct SoundChannel *next = chan->nextChannelPointer;
  struct SoundChannel *prev = chan->prevChannelPointer;

  if (prev != NULL) {
    prev->nextChannelPointer = next;
  } else {
    track->chan = next;
  }

  if (next != NULL) {
    next->prevChannelPointer = prev;
  }

  chan->track = NULL;
}

void m4aSoundVSync(void) {
  // in the original code, the ID number is checked (Smsh)
  // we skip this as we know the game has this identifier

  // Decrement the PCM DMA counter. If it reaches 0, we need to do a DMA.
  if (--sound_info->pcmDmaCounter != 0) {
    return;
  }

  // Reload the PCM DMA counter.
  sound_info->pcmDmaCounter = sound_info->pcmDmaPeriod;
  const u32 dma1cnt_h = REG_DMA1CNT_H;
  if (dma1cnt_h & DMA_REPEAT) {
    // 	ldr r1, =((DMA_ENABLE | DMA_START_NOW | DMA_32BIT | DMA_SRC_INC | DMA_DEST_FIXED) << 16) | 4
    //	str r1, [r2, 0x8] @ DMA1CNT
    log_warn("Do m4a vsync DMA 1");
  }

  const u32 dma2cnt_h = REG_DMA1CNT_H;
  if (dma2cnt_h & DMA_REPEAT) {
    //  ldr r1, =((DMA_ENABLE | DMA_START_NOW | DMA_32BIT | DMA_SRC_INC | DMA_DEST_FIXED) << 16) | 4
    //	str r1, [r2, 0xC + 0x8] @ DMA2CNT
    log_warn("Do m4a vsync DMA 2");
  }

  // turn off DMA1/DMA2
  REG_DMA1CNT_H = DMA_32BIT;
  REG_DMA2CNT_H = DMA_32BIT;

  // todo: uncomment / HLE
  // enable sound FIFO
  // LSB is 0, so DMA_SRC_INC is used (destination is always fixed in FIFO mode)
//  REG_DMA1CNT_H = (DMA_ENABLE | DMA_START_SPECIAL | DMA_32BIT | DMA_REPEAT);
//  REG_DMA2CNT_H = (DMA_ENABLE | DMA_START_SPECIAL | DMA_32BIT | DMA_REPEAT);
}