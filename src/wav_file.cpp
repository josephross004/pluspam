
#include <cstdlib>
#include <stdint.h>
#include "dr_wav.h"


// -----------------------------------


typedef struct {
    int fs;
    uint64_t len_ms;
    char* input_path;
    drwav* pWav;
} wav_file;


/**
 * @brief generates a wav_file struct based on the file at the path. 
 *
 * @param path: location of the file in the file system.
 *
 * @returns a pointer to the struct in the heap. 
 *
 */

wav_file* create_wav(char* path) {
    wav_file* rv = (wav_file*)calloc(1, sizeof(wav_file));
    if (!rv) return NULL;

    rv->pWav = (drwav*)calloc(1, sizeof(drwav));
    if (!rv->pWav) {
        free(rv);
        return NULL;
    }

    // Open using the correct API
    if (!drwav_init_file(rv->pWav, path, NULL)) {
        free(rv->pWav);
        free(rv);
        return NULL;
    }

    rv->fs = rv->pWav->sampleRate;
    rv->len_ms = (uint64_t)(
        ((double)rv->pWav->totalPCMFrameCount / rv->pWav->sampleRate) * 1000.0
    );
    rv->input_path = path;

    return rv;
}


/**
 * @brief frees memory for a wav_file struct allocated. 
 *
 * @param wv a pointer to the wav_file struct. 
 */

void destroy_wav(wav_file* wv) {
    if (!wv) return;

    if (wv->pWav) {
        drwav_uninit(wv->pWav);
        free(wv->pWav);
    }

    free(wv);
}


