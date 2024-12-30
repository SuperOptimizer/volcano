#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>

#include "volcano.h"

#define VESUVIUS_IMPL
#include "vesuvius-c.h"

#include "preprocess.h"
#include "snic.h"
#include "chord.h"
#include "util.h"
#include "flood.h"

#define SINGLE_THREADED


#define ROOTPATH "/Volumes/vesuvius"
#define OUTPUTPATH_1A ROOTPATH "/output_1a"
#define SCROLL_1A_VOLUME_PATH ROOTPATH "/dl.ash2txt.org/data/full-scrolls/Scroll1/PHercParis4.volpkg/volumes_zarr_standardized/54keV_7.91um_Scroll1A.zarr/0"
#define SCROLL_1A_FIBER_PATH ROOTPATH "/scroll1a_fibers/s1-surface-erode.zarr"

constexpr int zmax = 14376;
constexpr int ymax = 7888;
constexpr int xmax = 8096;
constexpr f32 iso = 32.0f;
constexpr int dims[3] = {dimension,dimension,dimension};
constexpr u32 max_superpixels = snic_superpixel_count();
constexpr f32 bounds[NUM_DIMENSIONS][2] = {
  {0, (f32)dims[0]},
  {0, (f32)dims[1]},
  {0, (f32)dims[2]}
};


typedef struct WorkerArgs {
  int worker_num;
  int z_start, z_end;
  char* volume_path;
  char* fiber_path;
} WorkerArgs;

void* worker_thread(void* arg) {
  WorkerArgs* args = arg;

  char path[1024] = {'\0'};
  snprintf(path,1023,"%s/.zarray",args->volume_path);
  const auto volume_metadata = vs_zarr_parse_zarray(path);

  snprintf(path,1023,"%s/.zarray",args->fiber_path);
  const auto fiber_metadata = vs_zarr_parse_zarray(path);

  printf("worker %d start z %d end z %d\n",args->worker_num,args->z_start,args->z_end);

  for (int z = args->z_start; z < args->z_end; z += dims[0]) {
    for (int y = 0; y < ymax; y += dims[1]) {
      for (int x = 0; x <  xmax; x += dims[2]) {
        chunk* scrollchunk = nullptr;
        chunk* fiberchunk = nullptr;
        u32* labels = nullptr;
        Superpixel* superpixels = nullptr;
        SuperpixelConnections* connections = nullptr;
        Chord* chords = nullptr;
        ChordStats* stats = nullptr;
        chunk* labeled_fiber = nullptr;

        char chunkpath[1024] = {'\0'};
        char csvpath[1024] = {'\0'};
        int num_chords = -1;
        int neigh_overflow = -1;
        int num_superpixels = -1;

        snprintf(chunkpath,1023,"%s/%d/%d/%d",SCROLL_1A_VOLUME_PATH,z/128,y/128,x/128);
        scrollchunk = vs_zarr_read_chunk(chunkpath,volume_metadata);
        if (scrollchunk == nullptr) {
          goto cleanup;
        }

        snprintf(chunkpath,1023,"%s/%d.%d.%d",SCROLL_1A_FIBER_PATH,z/128,x/128,y/128);
        fiberchunk = vs_zarr_read_chunk(chunkpath,fiber_metadata);
        if (fiberchunk == nullptr) {
          goto cleanup;
        }

        if (vs_chunk_max(fiberchunk) < 0.5f) {
          goto cleanup;
        }

        chunk* c = vs_avgpool_denoise(scrollchunk,3);
        vs_chunk_free(scrollchunk);
        scrollchunk = c;
        c = nullptr;

        float* cleaned_volume = segment_and_clean_f32(scrollchunk->data, dims[0], dims[1], dims[2], iso, iso + 96.0f);

        memcpy(scrollchunk->data, cleaned_volume, dims[0] * dims[1] * dims[2] * sizeof(float));
        free(cleaned_volume);
        cleaned_volume = nullptr;

        auto fiberchunk_transposed = vs_transpose(fiberchunk,"zxy","zyx");
        vs_chunk_free(fiberchunk);
        fiberchunk = fiberchunk_transposed;
        fiberchunk_transposed = nullptr;

        // the fiber data we are using has been eroded, so lets dilate it a bit. How much is an open question...
        auto dilated = vs_dilate(fiberchunk, 7);
        vs_chunk_free(fiberchunk);
        fiberchunk = dilated;
        dilated = nullptr;

        labels = malloc(dims[0]*dims[1]*dims[2]*sizeof(u32));
        superpixels = calloc(max_superpixels, sizeof(Superpixel));
        memset(labels, 0, dims[0]*dims[1]*dims[2]*sizeof(u32));

        neigh_overflow = snic(scrollchunk->data, labels, superpixels);

        num_superpixels = filter_superpixels(labels,superpixels,1,iso);

        snprintf(csvpath,1023,"%s/superpixels.%d.%d.%d.csv",OUTPUTPATH_1A,z/128,y/128,x/128);
        superpixels_to_csv(csvpath,superpixels,num_superpixels);

        connections = calculate_superpixel_connections(scrollchunk->data,labels,num_superpixels);

        // 0 for z-axis, 1 for y-axis, 2 for x-axis
        chords = grow_chords(superpixels, connections, num_superpixels, bounds, 0, 4096, &num_chords);

        snprintf(csvpath, 1023, "%s/chords.%d.%d.%d.csv", OUTPUTPATH_1A, z/128, y/128, x/128);
        chords_to_csv(csvpath, chords, num_chords);
        stats = analyze_chords(chords, num_chords,superpixels,connections);
        snprintf(csvpath, 1023, "%s/chords.stats.%d.%d.%d.csv", OUTPUTPATH_1A, z/128, y/128, x/128);
        write_chord_stats_csv(csvpath,stats,num_chords);

        snprintf(csvpath, 1023, "%s/chords.only.%d.%d.%d.csv", OUTPUTPATH_1A, z/128, y/128, x/128);
        chords_with_data_to_csv(csvpath,chords,num_chords,superpixels);

        // after getting the chords, it's time to map them to fiber data
        // the fiber data is a binary mask of a few voxels wide demonstrating the recto side of the papyrus
        // we first want to split it into individual connected sections
        labeled_fiber = vs_chunk_label_components(fiberchunk);
        printf("got %f unique sections of fiber\n",vs_chunk_max(labeled_fiber));
        // the sections are either part of the same papyrus sheet or not, and the disconnect can occur in any z y x axis
        // generally due to the fiber just being too hard to trace for the input ML fiber model coming from @bruniss

        // we want to check the superpixels in a chord and see if they fall in a fiber. we need to handle
        // 1) all of the superpixels in a chord falling in a single fiber
        // 2) some of the superpixels falling in one fiber and not in any other
        //    - in this case, we extend the fiber to include those bits
        // 3) some of the superpixels falling in one fiber, and some in a different fiber
        //    - this means that we've _either_
        //    1) continued a fiber through an area that the fiber data couldnt cover, or
        //    2) two fibers touch and the chord spans incorrectly across both. i.e. sheets are touching
        //    we'll assume it's 1 and hope/pray that 2 doesnt happen often

        for (int i = 0; i < num_chords; i++) {
          Chord mychord = chords[i];
          int num_unique = 0;
          int unique_labels[32] = {0};
          for (int j = 0; j < mychord.point_count; j++) {
            Superpixel sp = superpixels[mychord.points[j]];
            int label = vs_chunk_get(fiberchunk,sp.z,sp.y,sp.x);
            assert(label < 32);
            if (label == 0) {
              continue;
            }
            if (unique_labels[label] == 0) {
              num_unique++;
              unique_labels[label] = 1;
            }

          }
        }

        vs_chunk_free(labeled_fiber);
        free(stats);
        free_chords(chords,num_chords);
        free_superpixel_connections(connections, num_superpixels);
        free(labels);
        free(superpixels);

        printf("worker %d processed %d %d %d\n",args->worker_num,z,y,x);
        cleanup:

        vs_chunk_free(fiberchunk);
        vs_chunk_free(scrollchunk);
      }
    }
  }
  printf("worker %d done\n",args->worker_num);
  return NULL;
}



int scroll_1a_snic_chord() {
#ifdef SINGLE_THREADED
  constexpr int num_threads = 1;
#else
  constexpr int num_threads = 8;
#endif

  constexpr int chunk_per_thread = zmax / num_threads;

  pthread_t threads[num_threads];
  WorkerArgs args[num_threads];

  for (int i = 0; i < num_threads; i++) {
    args[i] = (WorkerArgs){
      .worker_num = i,
      .z_start = i * chunk_per_thread,
      .z_end = (i == num_threads-1) ? zmax : (i+1) * chunk_per_thread,
      .volume_path = SCROLL_1A_VOLUME_PATH,
      .fiber_path = SCROLL_1A_FIBER_PATH
    };
#ifdef SINGLE_THREADED
    worker_thread(&args[i]);
#else
    pthread_create(&threads[i], nullptr, worker_thread, &args[i]);
#endif
  }

#ifndef SINGLE_THREADED
  for (int i = 0; i < num_threads; i++) {
    pthread_join(threads[i], nullptr);
  }
#endif

  return 0;
}

int scroll_1a_unwrap() {
  scroll_1a_snic_chord();
  
}

int main(int argc, char** argv) {
  return scroll_1a_unwrap();
 }