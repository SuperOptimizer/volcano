#include "vesuvius-c.h"
#include "minisnic.h"


 int easy_snic(chunk *mychunk, s32 density, f32 compactness, chunk **labelsout, Superpixel **superpixelsout) {
  s32 lz, ly, lx;
  lz = mychunk->dims[0];
  ly = mychunk->dims[1];
  lx = mychunk->dims[2];

  *labelsout = vs_chunk_new(mychunk->dims);
  s32 superpixels_count = snic_superpixel_count(lx, ly, lz, density);
  *superpixelsout = calloc(superpixels_count, sizeof(Superpixel));
  return snic(mychunk->data, lx, ly, lz, density, compactness, 80.0f, 160.0f, (*labelsout)->data, *superpixelsout);
}

int main(int argc, char** argv) {
   volume* vol = vs_vol_new("./54keV_7.91um_Scroll1A.zarr/0/",
     "https://dl.ash2txt.org/full-scrolls/Scroll1/PHercParis4.volpkg/volumes_zarr_standardized/54keV_7.91um_Scroll1A.zarr/0/");
   chunk* mychunk = vs_vol_get_chunk(vol, (s32[3]){2048,2048,2048},(s32[3]){384,384,384});

  //chunk* mychunk = vs_zarr_fetch_block("https://dl.ash2txt.org/full-scrolls/Scroll1/PHercParis4.volpkg/volumes_zarr_standardized/54keV_7.91um_Scroll1A.zarr/0/30/30/30",vol->metadata);
  chunk* rescaled = vs_normalize_chunk(mychunk);
   f32* vertices;
   s32* indices;
   s32 vertex_count;
   s32 index_count;
  if (vs_march_cubes(rescaled->data,rescaled->dims[0],rescaled->dims[1],rescaled->dims[2],0.7f,&vertices,&indices,&vertex_count,&index_count)) {
    printf("failed to march cubes\n");
  }
   vs_ply_write("out.ply",vertices,NULL,indices,vertex_count,index_count);


   return 0;
 }