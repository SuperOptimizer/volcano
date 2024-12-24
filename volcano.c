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
} WorkerArgs;

void* worker_thread(void* arg) {
  WorkerArgs* args = arg;

  const auto volume_metadata = vs_zarr_parse_zarray(SCROLL_1A_VOLUME_PATH "/.zarray");
  const auto fiber_metadata = vs_zarr_parse_zarray(SCROLL_1A_FIBER_PATH "/.zarray");

  printf("worker %d start z %d end z %d\n",args->worker_num,args->z_start,args->z_end);

  for (int z = args->z_start; z < args->z_end; z += dims[0]) {
    for (int y = 128; y < ymax - 128; y += dims[1]) {
      for (int x = 128; x <  xmax - 128; x += dims[2]) {
        chunk* scrollchunk = nullptr;
        chunk* fiberchunk = nullptr;
        u32* labels = nullptr;
        Superpixel* superpixels = nullptr;
        SuperpixelConnections* connections = nullptr;
        Chord* chords = nullptr;
        ChordStats* stats = nullptr;

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
          //printf("no data in the fiber chunk so skipping %d %d %d\n",z,y,x);
          goto cleanup;
        }

        chunk* c = vs_avgpool_denoise(scrollchunk,3);
        vs_chunk_free(scrollchunk);
        scrollchunk = c;
        c = nullptr;

        float* cleaned_volume = segment_and_clean_f32(
                    scrollchunk->data,
                    dims[0], dims[1], dims[2],
                    iso,
                    iso + 96.0f);

        memcpy(scrollchunk->data, cleaned_volume, dims[0] * dims[1] * dims[2] * sizeof(float));
        free(cleaned_volume);

        auto fiberchunk_transposed = vs_transpose(fiberchunk,"zxy","zyx");
        vs_chunk_free(fiberchunk);
        fiberchunk = fiberchunk_transposed;
        fiberchunk_transposed = nullptr;

        labels = malloc(dims[0]*dims[1]*dims[2]*sizeof(u32));
        superpixels = calloc(max_superpixels, sizeof(Superpixel));
        memset(labels, 0, dims[0]*dims[1]*dims[2]*sizeof(u32));

        neigh_overflow = snic(scrollchunk->data, labels, superpixels);

        num_superpixels = filter_superpixels(labels,superpixels,1,iso);
        //printf("worker %d got %d superpixels from a possible %d\n",args->worker_num,num_superpixels, snic_superpixel_count());


        snprintf(csvpath,1023,"%s/superpixels.%d.%d.%d.csv",OUTPUTPATH_1A,z/128,y/128,x/128);
        superpixels_to_csv(csvpath,superpixels,num_superpixels);

        connections = calculate_superpixel_connections(scrollchunk->data,labels,num_superpixels);

        chords = grow_chords(superpixels,
                      connections,
                      num_superpixels,
                      bounds,
                      0,          // 0 for z-axis, 1 for y-axis, 2 for x-axis
                      4096,
                      &num_chords);

        snprintf(csvpath, 1023, "%s/chords.%d.%d.%d.csv", OUTPUTPATH_1A, z/128, y/128, x/128);
        chords_to_csv(csvpath, chords, num_chords);
        stats = analyze_chords(chords, num_chords,superpixels,connections);
        snprintf(csvpath, 1023, "%s/chords.stats.%d.%d.%d.csv", OUTPUTPATH_1A, z/128, y/128, x/128);
        write_chord_stats_csv(csvpath,stats,num_chords);

        snprintf(csvpath, 1023, "%s/chords.only.%d.%d.%d.csv", OUTPUTPATH_1A, z/128, y/128, x/128);
        chords_with_data_to_csv(csvpath,chords,num_chords,superpixels);

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

int scroll_1a_unwrap() {


  constexpr int num_threads = 8;
  constexpr int chunk_per_thread = ((zmax-128) - 2048) / (num_threads * dims[0]);

  pthread_t threads[num_threads];
  WorkerArgs args[num_threads];

  for (int i = 0; i < num_threads; i++) {
    args[i] = (WorkerArgs){
      .worker_num = i,
      .z_start = 2048 + i * chunk_per_thread * dims[0],
      .z_end = i == num_threads-1 ? zmax-128 : 2048 + (i+1) * chunk_per_thread * dims[0]
  };
    pthread_create(&threads[i], nullptr, worker_thread, &args[i]);
  }

  for (int i = 0; i < num_threads; i++) {
    pthread_join(threads[i], nullptr);
  }
  return 0;
}

int main(int argc, char** argv) {
  return scroll_1a_unwrap();
 }