#include "main.h"
#include "SoundEffect.h"
#include "audio.h"

// ---------------------------------------------------------------------------
// The sound table.
//
// This is the only thing to edit. Each line is:
//     { which sound, the file, its volume, a fallback file }
//
// Fallback is what plays when the first file is not there. The project
// already ships a handful of wavs in asset\Audio, so the game is audible
// before a single new file is dropped in - and stays audible if one is
// named differently than expected. Leave it "" for no fallback: the sound
// is then simply silent until its file exists.
//
// Volume is per clip, because sound effects never come out of a pack
// balanced against each other. Tune here, not at the call sites.
// ---------------------------------------------------------------------------
struct SoundDef
{
    SE          Sound;
    const char* File;
    float       Volume;
    const char* Fallback;
};

static const SoundDef s_Sounds[] =
{
    { SE::Jump,          "asset\\Audio\\SFX\\Jump.wav",               0.55f, "asset\\Audio\\wan.wav"       },
    { SE::Land,          "asset\\Audio\\SFX\\Land.wav",               0.40f, "asset\\Audio\\tyakuti.wav"   },

    { SE::PlayerAttack1, "asset\\Audio\\SFX\\Player_Atk_1.wav",       0.50f, ""                            },
    { SE::PlayerAttack2, "asset\\Audio\\SFX\\Player_Atk_2.wav",       0.50f, ""                            },
    { SE::PlayerAttack3, "asset\\Audio\\SFX\\Player_Atk_3.wav",       0.50f, ""                            },

    { SE::SwordHit,      "asset\\Audio\\SFX\\Sword_Hit.wav",          0.80f, ""                            },
    // No file of its own yet - the third combo swing is the heaviest of the
    // three, so it stands in until there is one.
    { SE::SpecialAttack, "asset\\Audio\\SFX\\Special_Attack.wav",     0.70f, "asset\\Audio\\SFX\\Player_Atk_3.wav" },
    { SE::Parry,         "asset\\Audio\\SFX\\Parry.wav",              1.00f, ""                            },

    { SE::PlayerHurt,    "asset\\Audio\\SFX\\Player_Hurt.wav",        0.75f, ""                            },

    { SE::EnemyAttack,   "asset\\Audio\\SFX\\Enemy_Attack_Sound.wav", 0.55f, ""                            },
    { SE::EnemyHurt,     "asset\\Audio\\SFX\\Enemy_Hurt.wav",         0.60f, ""                            },
    { SE::EnemyDeath,    "asset\\Audio\\SFX\\Enemy_Death.wav",        0.70f, "asset\\Audio\\explosion.wav" },

    { SE::CardHover,     "asset\\Audio\\SFX\\Card_Hover.wav",         0.35f, ""                            },
    { SE::CardSelect,    "asset\\Audio\\SFX\\Select_Card.wav",        0.70f, "asset\\Audio\\pause.wav"     },

    { SE::StageClear,    "asset\\Audio\\SFX\\Stage_clear.wav",        0.80f, "asset\\Audio\\gameclear.wav" },

    // The crate break ships as a loose wav rather than under SFX, so it is
    // named where it actually is instead of being moved.
    { SE::CrateBreak,    "asset\\Audio\\crate_broken.wav",            0.70f, ""                            },
    // No pickup sound of its own yet. The card select tick is short and
    // bright, which is the right shape for it, so it stands in until there
    // is one - the same way SpecialAttack borrows the third combo swing.
    { SE::PotionPickup,  "asset\\Audio\\SFX\\Potion_Pickup.wav",       0.65f, "asset\\Audio\\SFX\\Select_Card.wav" },
    // Picking one up and drinking it are different moments now, so they get
    // different clips. Neither file exists yet; both fall back to something
    // in the right register until they do.
    { SE::PotionDrink,   "asset\\Audio\\SFX\\Potion_Drink.wav",        0.75f, "asset\\Audio\\SFX\\Select_Card.wav" },
};

static const int s_SoundCount = sizeof(s_Sounds) / sizeof(s_Sounds[0]);

// How many copies of one clip can be in the air at once. Four covers a
// three-enemy swing plus whatever was already ringing out; with a single
// voice (which is what the Audio component has) each new hit cut the
// previous one off mid-sound.
static const int VOICES_PER_CLIP = 4;

// Two calls for the same clip closer together than this collapse into one.
// A swing that lands on three enemies calls SwordHit three times in the
// same frame, and three copies of one sound stacked on top of each other is
// just loud, not punchy.
static const DWORD RETRIGGER_MS = 40;

struct Clip
{
    BYTE*                   Data = nullptr;
    int                     Bytes = 0;
    int                     Frames = 0;
    float                   Volume = 1.0f;
    bool                    Loaded = false;

    IXAudio2SourceVoice*    Voices[VOICES_PER_CLIP] = {};
    int                     Next = 0;

    ULONGLONG               LastPlayTime = 0;
};

static Clip  s_Clips[(int)SE::Count];
static float s_MasterVolume = 1.0f;

// ---------------------------------------------------------------------------
// WAV loading. Same mmio walk the Audio component does, with one difference
// that matters: it reports failure instead of asserting. A missing sound
// file must not take the game down - at this stage of the project half the
// table is usually still empty.
// ---------------------------------------------------------------------------
static bool LoadWave(const char* FileName, BYTE** OutData, int* OutBytes, int* OutFrames, WAVEFORMATEX* OutFormat)
{
    if (FileName == nullptr || FileName[0] == '\0')
        return false;

    MMIOINFO mmioinfo = { 0 };
    HMMIO hmmio = mmioOpen((LPSTR)FileName, &mmioinfo, MMIO_READ);
    if (hmmio == NULL)
        return false;

    MMCKINFO riffchunkinfo = { 0 };
    riffchunkinfo.fccType = mmioFOURCC('W', 'A', 'V', 'E');
    if (mmioDescend(hmmio, &riffchunkinfo, NULL, MMIO_FINDRIFF) != MMSYSERR_NOERROR)
    {
        mmioClose(hmmio, 0);
        return false;
    }

    WAVEFORMATEX wfx = { 0 };

    MMCKINFO fmtchunkinfo = { 0 };
    fmtchunkinfo.ckid = mmioFOURCC('f', 'm', 't', ' ');
    if (mmioDescend(hmmio, &fmtchunkinfo, &riffchunkinfo, MMIO_FINDCHUNK) != MMSYSERR_NOERROR)
    {
        mmioClose(hmmio, 0);
        return false;
    }

    if (fmtchunkinfo.cksize >= sizeof(WAVEFORMATEX))
    {
        mmioRead(hmmio, (HPSTR)&wfx, sizeof(wfx));
    }
    else
    {
        PCMWAVEFORMAT pcmwf = { 0 };
        mmioRead(hmmio, (HPSTR)&pcmwf, sizeof(pcmwf));
        memset(&wfx, 0x00, sizeof(wfx));
        memcpy(&wfx, &pcmwf, sizeof(pcmwf));
        wfx.cbSize = 0;
    }
    mmioAscend(hmmio, &fmtchunkinfo, 0);

    MMCKINFO datachunkinfo = { 0 };
    datachunkinfo.ckid = mmioFOURCC('d', 'a', 't', 'a');
    if (mmioDescend(hmmio, &datachunkinfo, &riffchunkinfo, MMIO_FINDCHUNK) != MMSYSERR_NOERROR)
    {
        mmioClose(hmmio, 0);
        return false;
    }

    if (wfx.nBlockAlign == 0 || datachunkinfo.cksize == 0)
    {
        mmioClose(hmmio, 0);
        return false;
    }

    BYTE* data = new BYTE[datachunkinfo.cksize];
    LONG read = mmioRead(hmmio, (HPSTR)data, datachunkinfo.cksize);
    mmioClose(hmmio, 0);

    if (read <= 0)
    {
        delete[] data;
        return false;
    }

    *OutData = data;
    *OutBytes = read;
    *OutFrames = read / wfx.nBlockAlign;
    *OutFormat = wfx;
    return true;
}

void SoundEffect::Init()
{
    IXAudio2* xaudio = Audio::GetXAudio();
    if (xaudio == nullptr)
        return; // InitMaster has not run - nothing to attach voices to

    for (int i = 0; i < s_SoundCount; i++)
    {
        const SoundDef& def = s_Sounds[i];

        int index = (int)def.Sound;
        if (index < 0 || index >= (int)SE::Count)
            continue;

        Clip& clip = s_Clips[index];
        clip.Volume = def.Volume;

        WAVEFORMATEX wfx = { 0 };
        const char* loadedFrom = def.File;

        if (!LoadWave(def.File, &clip.Data, &clip.Bytes, &clip.Frames, &wfx))
        {
            loadedFrom = def.Fallback;

            if (!LoadWave(def.Fallback, &clip.Data, &clip.Bytes, &clip.Frames, &wfx))
            {
                char message[256];
                sprintf_s(message, "SoundEffect: missing '%s' - that sound stays silent\n", def.File);
                OutputDebugStringA(message);
                continue;
            }

            char message[256];
            sprintf_s(message, "SoundEffect: '%s' not found, using '%s'\n", def.File, loadedFrom);
            OutputDebugStringA(message);
        }

        // A pool per clip, all sharing the one decoded buffer.
        for (int v = 0; v < VOICES_PER_CLIP; v++)
        {
            if (FAILED(xaudio->CreateSourceVoice(&clip.Voices[v], &wfx)))
                clip.Voices[v] = nullptr;
        }

        clip.Loaded = (clip.Voices[0] != nullptr);

        if (!clip.Loaded)
        {
            delete[] clip.Data;
            clip.Data = nullptr;
        }
    }
}

void SoundEffect::Uninit()
{
    // Before Audio::UninitMaster - these voices feed the mastering voice it
    // destroys.
    for (int i = 0; i < (int)SE::Count; i++)
    {
        Clip& clip = s_Clips[i];

        for (int v = 0; v < VOICES_PER_CLIP; v++)
        {
            if (clip.Voices[v] == nullptr)
                continue;

            clip.Voices[v]->Stop();
            clip.Voices[v]->FlushSourceBuffers();
            clip.Voices[v]->DestroyVoice();
            clip.Voices[v] = nullptr;
        }

        delete[] clip.Data;
        clip.Data = nullptr;
        clip.Loaded = false;
    }
}

void SoundEffect::Play(SE Sound, float Volume)
{
    int index = (int)Sound;
    if (index < 0 || index >= (int)SE::Count)
        return;

    Clip& clip = s_Clips[index];
    if (!clip.Loaded)
        return; // file was never there - not an error, just nothing to play

    ULONGLONG now = GetTickCount64();
    if (now - clip.LastPlayTime < RETRIGGER_MS)
        return;

    // Prefer a voice that has finished. Only when every one of them is still
    // sounding does this steal the oldest, which is the round-robin slot.
    IXAudio2SourceVoice* voice = nullptr;

    for (int v = 0; v < VOICES_PER_CLIP; v++)
    {
        if (clip.Voices[v] == nullptr)
            continue;

        XAUDIO2_VOICE_STATE state = {};
        clip.Voices[v]->GetState(&state);

        if (state.BuffersQueued == 0)
        {
            voice = clip.Voices[v];
            break;
        }
    }

    if (voice == nullptr)
    {
        voice = clip.Voices[clip.Next];
        clip.Next = (clip.Next + 1) % VOICES_PER_CLIP;

        if (voice == nullptr)
            return;

        voice->Stop();
        voice->FlushSourceBuffers();
    }

    XAUDIO2_BUFFER buffer = {};
    buffer.AudioBytes = clip.Bytes;
    buffer.pAudioData = clip.Data;
    buffer.PlayBegin = 0;
    buffer.PlayLength = clip.Frames;
    buffer.Flags = XAUDIO2_END_OF_STREAM; // so BuffersQueued drops back to 0

    if (FAILED(voice->SubmitSourceBuffer(&buffer, NULL)))
        return;

    voice->SetVolume(clip.Volume * Volume * s_MasterVolume);
    voice->Start();

    clip.LastPlayTime = now;
}

void SoundEffect::SetMasterVolume(float Volume)
{
    if (Volume < 0.0f) Volume = 0.0f;
    if (Volume > 1.0f) Volume = 1.0f;

    s_MasterVolume = Volume;
}

float SoundEffect::GetMasterVolume()
{
    return s_MasterVolume;
}
