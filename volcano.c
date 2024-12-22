#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>

#include "volcano.h"

#define VESUVIUS_IMPL
#include "vesuvius-c.h"

#include "snic.h"
#include "chord.h"
#include "util.h"

#define ROOTPATH "/Volumes/vesuvius"
#define OUTPUTPATH_1A ROOTPATH "/output_1a"
#define SCROLL_1A_VOLUME_PATH ROOTPATH "/dl.ash2txt.org/data/full-scrolls/Scroll1/PHercParis4.volpkg/volumes_zarr_standardized/54keV_7.91um_Scroll1A.zarr/0"
#define SCROLL_1A_VOLUME_URL "https://dl.ash2txt.org/full-scrolls/Scroll1/PHercParis4.volpkg/volumes_zarr_standardized/54keV_7.91um_Scroll1A.zarr/0/"
#define SCROLL_1A_FIBER_PATH ROOTPATH "/scroll1a_fibers/s1-surface-erode.zarr"
#define SCROLL_1A_FIBER_URL "https://dl.ash2txt.org/community-uploads/bruniss/Fiber-and-Surface-Models/Predictions/s1/full-scroll-preds/s1-surface-erode.zarr/"




int scroll_1a_unwrap() {

  constexpr int zmax = 14376;
  constexpr int ymax = 7888;
  constexpr int xmax = 8096;

  //constexpr int zmax = 1024;
  //constexpr int ymax = 1024;
  //constexpr int xmax = 1024;

  constexpr int dims[3] = {dimension,dimension,dimension};

  auto volume_metadata = vs_zarr_parse_zarray(SCROLL_1A_VOLUME_PATH "/.zarray");
  auto fiber_metadata = vs_zarr_parse_zarray(SCROLL_1A_FIBER_PATH "/.zarray");

  printf("zmax%d\n",volume_metadata.shape[0]);
  for (int z = 128; z < zmax-128; z+=dims[0]) {
    for (int y = 128; y < ymax-128; y+=dims[1]) {
      for (int x = 128; x < xmax-128; x+=dims[2]) {
        constexpr u32 max_superpixels = (dims[0]/d_seed)*(dims[1]/d_seed)*(dims[2]/d_seed) + 1;
        constexpr f32 bounds[NUM_DIMENSIONS][2] = {
          {0, (f32)dims[0]},
          {0, (f32)dims[1]},
          {0, (f32)dims[2]}
        };

        chunk* scrollchunk = nullptr;
        chunk* fiberchunk = nullptr;
        uint32_t* labels = nullptr;
        Superpixel* superpixels = nullptr;
        SuperpixelConnections* connections = nullptr;
        Chord* chords = nullptr;

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
          printf("no data in the fiber chunk so skipping %d %d %d\n",z,y,x);
          goto cleanup;
        }

        auto fiberchunk_transposed = vs_transpose(fiberchunk,"zxy","zyx");
        vs_chunk_free(fiberchunk);
        fiberchunk = fiberchunk_transposed;
        fiberchunk_transposed = nullptr;

        labels = malloc(dims[0]*dims[1]*dims[2]*sizeof(u32));
        superpixels = calloc(max_superpixels, sizeof(Superpixel));

        memset(labels, 0, dims[0]*dims[1]*dims[2]*sizeof(u32));

        neigh_overflow = snic(scrollchunk->data, labels, superpixels);

        num_superpixels = filter_zero_superpixels(labels,superpixels);
        printf("got %d superpixels from a possible %d\n",num_superpixels, snic_superpixel_count());


        snprintf(csvpath,1023,"%s/superpixels.%d.%d.%d.csv",OUTPUTPATH_1A,z/128,y/128,x/128);
        superpixels_to_csv(csvpath,superpixels,snic_superpixel_count());

        connections = calculate_superpixel_connections(scrollchunk->data,labels);

        chords = grow_chords(superpixels,
                      connections,
                      snic_superpixel_count(),
                      bounds,
                      0,          // 0 for z-axis, 1 for y-axis, 2 for x-axis
                      4096,
                      &num_chords);

        free(chords);
        free_superpixel_connections(connections, snic_superpixel_count());
        free(labels);
        free(superpixels);

        printf("processed %d %d %d\n",z,y,x);
        cleanup:

        vs_chunk_free(fiberchunk);
        vs_chunk_free(scrollchunk);
      }
    }
  }


  return 0;
}

int main(int argc, char** argv) {
  return scroll_1a_unwrap();
 }