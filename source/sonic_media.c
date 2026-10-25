/* Local MP3 streaming for the original Android MediaPlayerManager interface. */
#define MPG123_PORTABLE_API
#include <mpg123.h>
#include "sonic_media.h"
#include "sonic_media_range.h"
#include "sonic_media_duration.h"
#include "sonic_jni_registry.h"
#include "sonic_java.h"
#include "utils/logger.h"
#include "utils/dialog.h"
#include <falso_jni/FalsoJNI.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/audioout.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <pthread.h>
#include <stdatomic.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CHANNELS 16
#define FRAMES 1024
#define MEDIA_PATH_CAPACITY 1024
typedef struct {
    SceUID fd;
    int64_t start, length, cursor;
} MediaReader;
typedef struct {
    MediaReader reader;
    mpg123_handle *decoder;
    pthread_t thread;
    int created, port, rate, channels, repeats;
    char path[MEDIA_PATH_CAPACITY];
    int16_t *samples;
    atomic_int status, stop, volume, seek_ms, position, duration, pending;
} MediaChannel;

static MediaChannel channels[CHANNELS];
static pthread_mutex_t control = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t once = PTHREAD_ONCE_INIT;
static int decoder_ready;
static atomic_int shutting_down;
static void (*stopped_callback)(JNIEnv *, jobject, jint);

static void initialize(void) {
    decoder_ready = mpg123_init() == MPG123_OK;
    for (int i = 0; i < CHANNELS; ++i) {
        channels[i].reader.fd = -1;
        channels[i].port = -1;
        atomic_store(&channels[i].volume, 100);
        atomic_store(&channels[i].seek_ms, -1);
    }
}
static MediaChannel *channel_at(int index) {
    pthread_once(&once, initialize);
    if (index < 0 || index >= CHANNELS)
        fatal_error("Sonic MediaPlayer: invalid channel %d", index);
    return &channels[index];
}
static int reader_read(void *opaque, void *buffer, size_t wanted, size_t *received) {
    MediaReader *reader = opaque;
    *received = 0;
    int64_t left = reader->length - reader->cursor;
    if (left < 0) return -1;
    if ((uint64_t)wanted > (uint64_t)left) wanted = (size_t)left;
    if (wanted > INT_MAX) wanted = INT_MAX;
    if (!wanted) return 0;
    int result = sceIoRead(reader->fd, buffer, wanted);
    if (result < 0) return -1;
    reader->cursor += result;
    *received = (size_t)result;
    return 0;
}
static int64_t reader_seek(void *opaque, int64_t offset, int whence) {
    MediaReader *reader = opaque;
    int64_t target;
    if (!sonic_media_seek_target(reader->length, reader->cursor, offset, whence, &target))
        return -1;
    if (sceIoLseek(reader->fd, reader->start + target, SCE_SEEK_SET) < 0) return -1;
    reader->cursor = target;
    return target;
}
static void notify(int index) {
    /* Never called by a decoder worker or while the control mutex is held. */
    if (atomic_load(&shutting_down) || !stopped_callback) return;
    l_info("MediaPlayer stopped notification: channel=%d", index);
    stopped_callback(&jni, SONIC_OBJECT_THREAD, index);
}
static void dispose(MediaChannel *c) {
    if (c->decoder) { mpg123_delete(c->decoder); c->decoder = NULL; }
    if (c->reader.fd >= 0) { sceIoClose(c->reader.fd); c->reader.fd = -1; }
    if (c->port >= 0) { sceAudioOutReleasePort(c->port); c->port = -1; }
    free(c->samples); c->samples = NULL;
}
static void *playback(void *opaque) {
    MediaChannel *c = opaque;
    int index = (int)(c - channels), failed = 0, completed = 0, last_volume = -1;
    unsigned blocks = 0;
    uint64_t prepare_start = sceKernelGetProcessTimeWide(), scan_us = 0;
    c->reader.fd = sceIoOpen(c->path, SCE_O_RDONLY, 0);
    if (c->reader.fd < 0 || reader_seek(&c->reader, 0, SEEK_SET) < 0) {
        l_error("MediaPlayer async open failed: channel=%d", index);
        failed = 1;
    }
    int decoder_error = 0;
    if (!failed) {
        c->decoder = mpg123_new(NULL, &decoder_error);
        if (!c->decoder || mpg123_format_none(c->decoder) != MPG123_OK) failed = 1;
    }
    static const long rates[] = {8000,11025,12000,16000,22050,24000,32000,44100,48000};
    if (!failed) {
        for (unsigned i = 0; i < sizeof(rates)/sizeof(rates[0]); ++i) {
            if (mpg123_format(c->decoder, rates[i], MPG123_MONO | MPG123_STEREO,
                              MPG123_ENC_SIGNED_16) != MPG123_OK) {
                failed = 1;
                break;
            }
        }
    }
    if (!failed && (mpg123_reader64(c->decoder, reader_read, reader_seek, NULL) != MPG123_OK ||
                    mpg123_open_handle64(c->decoder, &c->reader) != MPG123_OK)) failed = 1;
    long rate = 0;
    int encoding = 0;
    if (!failed && (mpg123_getformat(c->decoder, &rate, &c->channels, &encoding) != MPG123_OK ||
                    encoding != MPG123_ENC_SIGNED_16 ||
                    (c->channels != 1 && c->channels != 2))) failed = 1;
    c->rate = (int)rate;
    struct mpg123_frameinfo2 frame_info = {0};
    if (!failed && mpg123_info(c->decoder, &frame_info) != MPG123_OK) failed = 1;
    int64_t frames = failed ? 0 : mpg123_length64(c->decoder);
    int scanned = 0, cbr_estimated = 0;
    if (!failed && frames <= 0 && frame_info.vbr == MPG123_CBR &&
        sonic_media_cbr_frames(c->reader.length, frame_info.bitrate,
                               c->rate, &frames)) {
        /* CBR frame metadata gives an accurate length without decoding every
         * frame. All 38 Sonic MP3 assets are 128 kbps CBR. */
        cbr_estimated = 1;
    }
    if (!failed && frames <= 0) {
        /* Retain support for VBR files without a Xing length, but do the
         * expensive scan only on this background worker. */
        uint64_t scan_start = sceKernelGetProcessTimeWide();
        if (mpg123_scan(c->decoder) != MPG123_OK) failed = 1;
        scan_us = sceKernelGetProcessTimeWide() - scan_start;
        frames = failed ? 0 : mpg123_length64(c->decoder);
        scanned = 1;
    }
    if (!failed && (frames <= 0 || frames > (int64_t)INT_MAX * c->rate / 1000 ||
                    (scanned && mpg123_seek64(c->decoder, 0, SEEK_SET) < 0))) failed = 1;
    if (!failed) {
        atomic_store(&c->duration, (int)(frames * 1000 / c->rate));
        c->samples = memalign(64, FRAMES * c->channels * sizeof(*c->samples));
        if (!c->samples) failed = 1;
    }
    if (!failed) {
        c->port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, FRAMES, c->rate,
                                     c->channels == 2 ? SCE_AUDIO_OUT_MODE_STEREO : SCE_AUDIO_OUT_MODE_MONO);
        if (c->port < 0) {
            l_error("MediaPlayer BGM port open failed: 0x%x", c->port);
            failed = 1;
        }
    }
    if (failed) {
        l_error("MediaPlayer async prepare failed: channel=%d decoder=%s", index,
                c->decoder ? mpg123_strerror(c->decoder) : "unavailable");
        goto finished;
    }
    l_info("MediaPlayer prepared MP3: channel=%d rate=%d channels=%d duration=%d ms repeats=%d length_source=%s prepare_us=%u scan_us=%u",
            index, c->rate, c->channels, atomic_load(&c->duration), c->repeats,
            scanned ? "scan" : cbr_estimated ? "cbr-size" : "mpg123",
            (unsigned)(sceKernelGetProcessTimeWide() - prepare_start),
            (unsigned)scan_us);
    while (!atomic_load(&c->stop)) {
        int seek = atomic_exchange(&c->seek_ms, -1);
        if (seek >= 0) {
            if (mpg123_seek64(c->decoder, (int64_t)seek * c->rate / 1000, SEEK_SET) < 0) {
                l_error("MediaPlayer seek failed: channel=%d ms=%d", index, seek);
                failed = 1; break;
            }
            atomic_store(&c->position, seek);
        }
        if (atomic_load(&c->status) == 2) { sceKernelDelayThread(10000); continue; }
        size_t bytes = 0, capacity = FRAMES * c->channels * sizeof(*c->samples);
        int result = mpg123_read(c->decoder, c->samples, capacity, &bytes);
        if (result == MPG123_NEW_FORMAT) {
            long rate; int count, encoding;
            if (mpg123_getformat(c->decoder, &rate, &count, &encoding) != MPG123_OK ||
                rate != c->rate || count != c->channels || encoding != MPG123_ENC_SIGNED_16) {
                l_error("MediaPlayer unsupported mid-stream format change: channel=%d", index);
                failed = 1; break;
            }
        } else if (result != MPG123_OK && result != MPG123_DONE) {
            l_error("MediaPlayer decode failed: channel=%d code=%d %s",
                    index, result, mpg123_strerror(c->decoder));
            failed = 1; break;
        }
        if (atomic_load(&c->stop)) break;
        if (bytes) {
            if (bytes > capacity || bytes % (c->channels * sizeof(*c->samples))) {
                failed = 1; break;
            }
            memset((char *)c->samples + bytes, 0, capacity - bytes);
            int volume = atomic_load(&c->volume);
            if (volume != last_volume) {
                int values[2] = {volume * SCE_AUDIO_OUT_MAX_VOL / 100,
                                 volume * SCE_AUDIO_OUT_MAX_VOL / 100};
                if (sceAudioOutSetVolume(c->port, SCE_AUDIO_VOLUME_FLAG_L_CH |
                        SCE_AUDIO_VOLUME_FLAG_R_CH, values) < 0) { failed = 1; break; }
                last_volume = volume;
            }
            if (sceAudioOutOutput(c->port, c->samples) < 0) { failed = 1; break; }
            int64_t frame = mpg123_tell64(c->decoder);
            if (frame >= 0) atomic_store(&c->position, (int)(frame * 1000 / c->rate));
            if (++blocks == 1) l_info("MediaPlayer first MP3 block submitted: channel=%d rate=%d channels=%d",
                                      index, c->rate, c->channels);
        }
        if (result == MPG123_DONE) {
            if (!sonic_media_repeat(&c->repeats)) { completed = 1; break; }
            if (mpg123_seek64(c->decoder, 0, SEEK_SET) < 0) { failed = 1; break; }
            atomic_store(&c->position, 0);
        }
    }
finished:
    if (c->port >= 0) {
        sceAudioOutOutput(c->port, NULL);
        sceAudioOutReleasePort(c->port); c->port = -1;
    }
    atomic_store(&c->status, failed ? 3 : 0);
    if ((failed || completed) && !atomic_load(&c->stop)) atomic_store(&c->pending, 1);
    l_info("MediaPlayer worker finished: channel=%d blocks=%u complete=%d error=%d",
            index, blocks, completed, failed);
    return NULL;
}

void sonic_media_stop(int index) {
    MediaChannel *c = channel_at(index);
    pthread_mutex_lock(&control);
    int status = atomic_load(&c->status);
    atomic_store(&c->stop, 1);
    if (c->created) {
        /* Workers never enter the game/JNI, and never take control. */
        int result = pthread_join(c->thread, NULL);
        if (result) {
            pthread_mutex_unlock(&control);
            fatal_error("Sonic MediaPlayer: join failed %d", result);
        }
        c->created = 0;
    }
    int pending = atomic_exchange(&c->pending, 0);
    dispose(c);
    atomic_store(&c->status, 0);
    atomic_store(&c->position, 0);
    atomic_store(&c->seek_ms, -1);
    pthread_mutex_unlock(&control);
    if (status == 1 || status == 2 || pending) notify(index);
}

int sonic_media_play(const char *path, int repeats, int64_t offset, int64_t length, int index) {
    if (atomic_load(&shutting_down)) return -2;
    MediaChannel *c = channel_at(index);
    sonic_media_stop(index);
    pthread_mutex_lock(&control);
    int result = -2;
    if (!stopped_callback)
        stopped_callback = sonic_find_native("com/ideaworks3d/marmalade/LoaderThread",
                                            "audioStoppedNotify", "(I)V");
    if (!decoder_ready || !stopped_callback) {
        l_error("MediaPlayer initialization/native completion callback unavailable"); goto fail;
    }
    if (!path || strncmp(path, DATA_PATH, strlen(DATA_PATH)) || strstr(path, "..")) {
        l_error("MediaPlayer supports local Sonic data paths only"); goto fail;
    }
    if (strlen(path) >= sizeof(c->path)) {
        l_error("MediaPlayer path too long: channel=%d", index);
        goto fail;
    }
    SceIoStat st = {0};
    if (sceIoGetstat(path, &st) < 0 || !SCE_S_ISREG(st.st_mode)) { result = -1; goto fail; }
    if (!sonic_media_bounds(st.st_size, offset, length, &c->reader.start, &c->reader.length)) {
        l_error("MediaPlayer invalid file range: offset=%lld length=%lld size=%lld",
                (long long)offset, (long long)length, (long long)st.st_size); goto fail;
    }
    strcpy(c->path, path);
    c->reader.cursor = 0;
    c->repeats = repeats;
    atomic_store(&c->stop, 0); atomic_store(&c->pending, 0);
    atomic_store(&c->position, 0); atomic_store(&c->seek_ms, -1);
    atomic_store(&c->duration, 0);
    atomic_store(&c->status, 1);
    pthread_attr_t attr;
    int error = pthread_attr_init(&attr);
    if (!error) {
        error = pthread_attr_setstacksize(&attr, 512 * 1024);
        if (!error) error = pthread_create(&c->thread, &attr, playback, c);
        pthread_attr_destroy(&attr);
    }
    if (error) { l_error("MediaPlayer thread creation failed: %d", error); goto fail; }
    c->created = 1;
    l_info("MediaPlayer prepare queued: channel=%d offset=%lld length=%lld repeats=%d",
            index, (long long)c->reader.start, (long long)c->reader.length, repeats);
    pthread_mutex_unlock(&control);
    return 0;
fail:
    l_error("MediaPlayer prepare failed: channel=%d result=%d", index, result);
    dispose(c); atomic_store(&c->status, 3);
    pthread_mutex_unlock(&control);
    return result;
}

int sonic_media_pause(int index) {
    MediaChannel *c = channel_at(index);
    int state = 1;
    return atomic_compare_exchange_strong(&c->status, &state, 2) ? 0 : -1;
}
int sonic_media_resume(int index) {
    MediaChannel *c = channel_at(index);
    int state = 2;
    return atomic_compare_exchange_strong(&c->status, &state, 1) ? 0 : -1;
}
int sonic_media_status(int index) { return atomic_load(&channel_at(index)->status); }
int sonic_media_position(int index) {
    MediaChannel *c = channel_at(index);
    int state = atomic_load(&c->status);
    return state == 1 || state == 2 ? atomic_load(&c->position) : 0;
}
int sonic_media_duration(int index) {
    MediaChannel *c = channel_at(index);
    pthread_mutex_lock(&control);
    int state = atomic_load(&c->status);
    int result = state == 1 || state == 2 ? atomic_load(&c->duration) : 0;
    pthread_mutex_unlock(&control);
    return result;
}
void sonic_media_seek(int milliseconds, int index) {
    MediaChannel *c = channel_at(index);
    if (milliseconds < 0) milliseconds = 0;
    int duration = sonic_media_duration(index);
    if (milliseconds > duration) milliseconds = duration;
    if (atomic_load(&c->status) == 1 || atomic_load(&c->status) == 2)
        atomic_store(&c->seek_ms, milliseconds);
}
void sonic_media_volume(int volume, int index) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    atomic_store(&channel_at(index)->volume, volume);
}
void sonic_media_pump(void) {
    if (atomic_load(&shutting_down)) return;
    pthread_once(&once, initialize);
    for (int i = 0; i < CHANNELS; ++i)
        if (atomic_exchange(&channels[i].pending, 0)) notify(i);
}

void sonic_media_shutdown(void) {
    /* Runtime has returned; its callback state may already be destroyed.
     * Join decoder workers, but never notify the terminated game. */
    atomic_store(&shutting_down, 1);
    for (int i = 0; i < CHANNELS; ++i) sonic_media_stop(i);
    l_info("MediaPlayer shutdown: all decoder workers and ports released");
}
