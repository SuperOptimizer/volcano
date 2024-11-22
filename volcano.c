#define VESUVIUS_IMPL
#include "vesuvius-c.h"
#include "minisnic.h"


 int easy_snic(chunk *mychunk, s32 density, f32 compactness, u32 **labelsout, Superpixel **superpixelsout) {
  s32 lz, ly, lx;
  lz = mychunk->dims[0];
  ly = mychunk->dims[1];
  lx = mychunk->dims[2];

  *labelsout = calloc(lz*ly*lx*4, 1);
  s32 superpixels_count = snic_superpixel_count(lx, ly, lz, density);
  *superpixelsout = calloc(superpixels_count, sizeof(Superpixel));
  return snic(mychunk->data, lx, ly, lz, density, compactness, 80.0f, 160.0f, *labelsout, *superpixelsout);
}


int main(int argc, char** argv) {
  volume* scroll_vol = vs_vol_new("./54keV_7.91um_Scroll1A.zarr/0/",
     "https://dl.ash2txt.org/full-scrolls/Scroll1/PHercParis4.volpkg/volumes_zarr_standardized/54keV_7.91um_Scroll1A.zarr/0/");
  chunk* scroll_chunk = vs_vol_get_chunk(scroll_vol, (s32[3]){3072,3072,3072},(s32[3]){3072,384,384});

  volume* fiber_vol = vs_vol_new("s1-surface-regular.zarr/",
     "https://dl.ash2txt.org/community-uploads/bruniss/Fiber-and-Surface-Models/Predictions/s1/full-scroll-preds/s1-surface-regular.zarr/");

  chunk* fiber_chunk = vs_vol_get_chunk(fiber_vol, (s32[3]){3072,3072,3072},(s32[3]){3072,384,384});

  chunk* rescaled_vol = vs_normalize_chunk(scroll_chunk);
  chunk* rescaled_fiber = vs_normalize_chunk(fiber_chunk);

  chunk* out_r = vs_chunk_new((s32[3]){3072,384,384});
  chunk* out_g = vs_chunk_new((s32[3]){3072,384,384});
  chunk* out_b = vs_chunk_new((s32[3]){3072,384,384});

   f32 ISO = 0.2f;


  for (int z = 0; z < 3071; z++) {
    for (int y = 0; y < 384; y++) {
      for (int x = 0; x < 384; x++) {
        f32 me   = vs_chunk_get(rescaled_vol,z,y,x);
        f32 next = vs_chunk_get(rescaled_vol,z+1,y,x);
        f32 fiber = vs_chunk_get(rescaled_fiber,z,y,x);

        if (fiber > 0) {
          vs_chunk_set(out_b,z,y,x,1.0f);
        } else {
          vs_chunk_set(out_b,z,y,x,0.0f);
        }
        if (me > ISO && next > ISO) {
          //white
          if (vs_chunk_get(out_b,z,y,x) <= 0.0001f) {
            vs_chunk_set(out_r,z,y,x,me);
            vs_chunk_set(out_g,z,y,x,me);
            vs_chunk_set(out_b,z,y,x,me);
          }
        } else if (me < ISO && next < ISO) {
          if (vs_chunk_get(out_b,z,y,x) <= 0.0001f) {
            vs_chunk_set(out_r,z,y,x,0.0f);
            vs_chunk_set(out_g,z,y,x,0.0f);
            vs_chunk_set(out_b,z,y,x,0.0f);
          }
          //black
        } else if (me < ISO && next >= ISO) {
          if (vs_chunk_get(out_b,z,y,x) <= 0.0001f) {
            vs_chunk_set(out_r,z,y,x,1.0f);
            vs_chunk_set(out_g,z,y,x,0.0f);
            //vs_chunk_set(out_b,z,y,x,0.0f);
          }
          //red
        } else if (me >= ISO && next < ISO) {
          if (vs_chunk_get(out_b,z,y,x) <= 0.0001f) {
            vs_chunk_set(out_r,z,y,x,0.0f);
            vs_chunk_set(out_g,z,y,x,1.0f);
            //vs_chunk_set(out_b,z,y,x,0.0f);
          }
          //green
        } else {
          //is this possible?
        }

      }
    }
  }

  vs_chunks_to_video(out_r, out_g, out_b, "output.mp4", 30);
  return 0;
 }