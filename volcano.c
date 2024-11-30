#define VESUVIUS_IMPL
#include "vesuvius-c.h"
#include "minisnic.h"

chunk* vs_denoise_window(chunk* input, s32 kernel, f32 iso_threshold) {
  s32 dims[3] = {input->dims[0], input->dims[1], input->dims[2]};
  chunk* output = vs_chunk_new(dims);
  s32 offset = kernel / 2;

  for (s32 z = 0; z < dims[0]; z++) {
    for (s32 y = 0; y < dims[1]; y++) {
      for (s32 x = 0; x < dims[2]; x++) {
        f32 sum = 0.0f;
        s32 count = 0;

        // Sum values in the window around current voxel
        for (s32 zi = -offset; zi <= offset; zi++) {
          for (s32 yi = -offset; yi <= offset; yi++) {
            for (s32 xi = -offset; xi <= offset; xi++) {
              s32 nz = z + zi;
              s32 ny = y + yi;
              s32 nx = x + xi;

              // Skip if outside volume bounds
              if (nz < 0 || nz >= dims[0] ||
                  ny < 0 || ny >= dims[1] ||
                  nx < 0 || nx >= dims[2]) {
                continue;
                  }

              sum += vs_chunk_get(input, nz, ny, nx);
              count++;
            }
          }
        }

        // Calculate average and set output mask
        f32 average = sum / count;
        vs_chunk_set(output, z, y, x, (average > iso_threshold) ? 1.0f : 0.0f);
      }
    }
  }

  return output;
}

chunk* vs_dilate_labels(chunk* inchunk, s32 kernel) {
  s32 dims[3] = {inchunk->dims[0], inchunk->dims[1], inchunk->dims[2]};
  chunk* ret = vs_chunk_new(dims);
  s32 offset = kernel / 2;

  // First copy the input to preserve all labels
  for (s32 z = 0; z < dims[0]; z++)
    for (s32 y = 0; y < dims[1]; y++)
      for (s32 x = 0; x < dims[2]; x++) {
        vs_chunk_set(ret, z, y, x, vs_chunk_get(inchunk, z, y, x));
      }

  // Then dilate labeled regions
  for (s32 z = 0; z < dims[0]; z++)
    for (s32 y = 0; y < dims[1]; y++)
      for (s32 x = 0; x < dims[2]; x++) {
        // Only dilate into unlabeled regions
        if (!is_unlabeled(vs_chunk_get(ret, z, y, x))) {
          continue;
        }

        // Take first non-zero label we find in neighborhood
        for (s32 zi = -offset; zi <= offset; zi++)
          for (s32 yi = -offset; yi <= offset; yi++)
            for (s32 xi = -offset; xi <= offset; xi++) {
              s32 nz = z + zi;
              s32 ny = y + yi;
              s32 nx = x + xi;
              if (nz < 0 || nz >= dims[0] ||
                  ny < 0 || ny >= dims[1] ||
                  nx < 0 || nx >= dims[2]) {
                continue;
                  }
              f32 val = vs_chunk_get(inchunk, nz, ny, nx);
              if (!is_unlabeled(val)) {
                vs_chunk_set(ret, z, y, x, val);
                goto next_voxel;  // Break out of all loops once we find a label
              }
            }
        next_voxel:;
      }
  return ret;
}

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

chunk* vs_get_label_mask(chunk* labeled_chunk, f32 label) {
   chunk* mask = vs_chunk_new(labeled_chunk->dims);

   for (s32 z = 0; z < labeled_chunk->dims[0]; z++) {
     for (s32 y = 0; y < labeled_chunk->dims[1]; y++) {
       for (s32 x = 0; x < labeled_chunk->dims[2]; x++) {
         f32 voxel_label = vs_chunk_get(labeled_chunk, z, y, x);
         // Using epsilon comparison since we're dealing with floats
         vs_chunk_set(mask, z, y, x, fabsf(voxel_label - label) < 0.0001f ? 1.0f : 0.0f);
       }
     }
   }

   return mask;
 }

s32* sort_label_counts(s32 num_labels, s32* counts) {
   // Create array of sorted indices [0,1,2,...,num_labels-1]
   s32* sorted = (s32*)malloc(num_labels * sizeof(s32));
   for (s32 i = 0; i < num_labels; i++) {
     sorted[i] = i;
   }

   // Sort indices by their corresponding counts
   for (s32 i = 0; i < num_labels - 1; i++) {
     for (s32 j = 0; j < num_labels - i - 1; j++) {
       if (counts[sorted[j]] < counts[sorted[j + 1]]) {
         // Swap indices
         s32 temp = sorted[j];
         sorted[j] = sorted[j + 1];
         sorted[j + 1] = temp;
       }
     }
   }

   return sorted;
 }

void fiber_overlay() {

  s32 CHUNK_DIMS[3] = {512,512,512};

  volume* scroll_vol = vs_vol_new("./54keV_7.91um_Scroll1A.zarr/0/",
     "https://dl.ash2txt.org/full-scrolls/Scroll1/PHercParis4.volpkg/volumes_zarr_standardized/54keV_7.91um_Scroll1A.zarr/0/");
  chunk* scroll_chunk = vs_vol_get_chunk(scroll_vol, (s32[3]){3072,3072,3072},CHUNK_DIMS);

  volume* fiber_vol = vs_vol_new("s1-surface-regular.zarr/",
     "https://dl.ash2txt.org/community-uploads/bruniss/Fiber-and-Surface-Models/Predictions/s1/full-scroll-preds/s1-surface-regular.zarr/");

  chunk* fiber_chunk = vs_vol_get_chunk(fiber_vol, (s32[3]){3072,3072,3072},CHUNK_DIMS);
  chunk* equalized_scroll = vs_histogram_equalize(scroll_chunk,256);

  chunk* rescaled_scroll = vs_normalize_chunk(equalized_scroll);
  chunk* rescaled_fiber = vs_normalize_chunk(fiber_chunk);


  f32 ISO = 0.2f;


  chunk* scroll_mask = vs_threshold(rescaled_scroll,ISO,0.0f,1.0f);
  chunk* masked_fiber0 = vs_mask(rescaled_fiber,scroll_mask);


  //chunk* dilated_fiber1 = vs_dilate(rescaled_fiber,3);
  //chunk* dilated_masked1 = vs_mask(dilated_fiber1,scroll_mask);
  //chunk* dilated_fiber2 = vs_dilate(dilated_masked1,3);
  //chunk* dilated_masked2 = vs_mask(dilated_fiber2,scroll_mask);

  //chunk* dilated_fiber3 = vs_dilate(dilated_masked2,3);
  //chunk* dilated_masked3 = vs_mask(dilated_fiber3,scroll_mask);

  //chunk* dilated_fiber4 = vs_dilate(dilated_masked3,3);
  //chunk* dilated_masked4 = vs_mask(dilated_fiber4,scroll_mask);


  chunk* connected = vs_connected_components_3d(masked_fiber0);


   f32 max,min;
   min = vs_chunk_min(connected);
   max = vs_chunk_max(connected);
   printf("num labels: %f\n",max);
  chunk* out_r = vs_chunk_new(CHUNK_DIMS);
  chunk* out_g = vs_chunk_new(CHUNK_DIMS);
  chunk* out_b = vs_chunk_new(CHUNK_DIMS);


   s32* counts;
   s32 num_labels = vs_count_labels(connected,&counts);
   s32* sorted = sort_label_counts(num_labels,counts);
   printf("num labels: %d\n",num_labels);

  chunk* mask = vs_get_label_mask(connected,sorted[1]);

  chunk* masked_scroll = vs_mask(rescaled_scroll,mask);


  f32* vertices;
  s32* indices;
  s32 index_count;
  s32 vertex_count;
  f32* gray_colors;


  if (vs_march_cubes(masked_scroll->data,masked_scroll->dims[0],masked_scroll->dims[1],masked_scroll->dims[2],ISO,&vertices,&gray_colors,&indices,&vertex_count,&index_count)) {
    LOG_ERROR("failed to march cubes");
  }
  rgb* colors = malloc(sizeof(rgb) * vertex_count);
  vs_colorize(gray_colors,colors,vertex_count,0.0f,1.0f);
  if (vs_ply_write("generated.ply",vertices,NULL,colors,indices,vertex_count,index_count)) {
    LOG_ERROR("failed to write ply");
  }

  for (int z = 0; z < CHUNK_DIMS[0]-1; z++) {
    for (int y = 0; y < CHUNK_DIMS[1]; y++) {
      for (int x = 0; x < CHUNK_DIMS[2]; x++) {
        f32 me   = vs_chunk_get(rescaled_scroll,z,y,x);
        f32 next = vs_chunk_get(rescaled_scroll,z+1,y,x);
        f32 fiber = vs_chunk_get(mask,z,y,x);

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
}

void scroll5_preview() {
  s32 CHUNK_DIMS[3] = {128,2048,2048};

  volume* scroll_vol = vs_vol_new("/home/forrest/dl.ash2txt.org/full-scrolls/Scroll5/PHerc172.volpkg/volumes_zarr_standardized/53keV_7.91um_Scroll5.zarr/0",
   "https://dl.ash2txt.org/full-scrolls/Scroll5/PHerc172.volpkg/volumes_zarr_standardized/53keV_7.91um_Scroll5.zarr/0/");
  chunk* scroll_chunk = vs_vol_get_chunk(scroll_vol, (s32[3]){0,4096,4096},CHUNK_DIMS);

  f32 ISO = 0.2f;

  chunk* out_r = vs_chunk_new(CHUNK_DIMS);
  chunk* out_g = vs_chunk_new(CHUNK_DIMS);
  chunk* out_b = vs_chunk_new(CHUNK_DIMS);
  LOG_INFO("equalizing chunk");
  chunk* equalized_scroll = vs_histogram_equalize(scroll_chunk,256);
LOG_INFO("normalizing chunk");
  chunk* rescaled_scroll = vs_normalize_chunk(equalized_scroll);
  chunk* mask = vs_denoise_window(rescaled_scroll,3,ISO);
  chunk* denoised = vs_mask(rescaled_scroll,mask);
/*

  f32* vertices;
  s32* indices;
  s32 index_count;
  s32 vertex_count;
  f32* gray_colors;

LOG_INFO("marching cubes");
  if (vs_march_cubes(denoised->data,denoised->dims[0],denoised->dims[1],denoised->dims[2],ISO,&vertices,&gray_colors,&indices,&vertex_count,&index_count)) {
    LOG_ERROR("failed to march cubes");
  }
  rgb* colors = malloc(sizeof(rgb) * vertex_count);
  LOG_INFO("colorizing");
  vs_colorize(gray_colors,colors,vertex_count,0.0f,1.0f);
  LOG_INFO("writing ply");
  if (vs_ply_write("generated.ply",vertices,NULL,colors,indices,vertex_count,index_count)) {
    LOG_ERROR("failed to write ply");
  }
  */
  LOG_INFO("coloring video");
  for (int z = 1; z < CHUNK_DIMS[0]-1; z++) {
    for (int y = 0; y < CHUNK_DIMS[1]; y++) {
      for (int x = 0; x < CHUNK_DIMS[2]; x++) {
        f32 me   = vs_chunk_get(denoised,z,y,x);
        f32 next = vs_chunk_get(denoised,z+1,y,x);

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
            vs_chunk_set(out_b,z,y,x,0.0f);
          }
          //red
        } else if (me >= ISO && next < ISO) {
          if (vs_chunk_get(out_b,z,y,x) <= 0.0001f) {
            vs_chunk_set(out_r,z,y,x,0.0f);
            vs_chunk_set(out_g,z,y,x,1.0f);
            vs_chunk_set(out_b,z,y,x,0.0f);
          }
          //green
        } else {
          //is this possible?
        }

      }
    }
  }
  LOG_INFO("writing video to mp4");
  vs_chunks_to_video(out_r, out_g, out_b, "output.mp4", 30);

}

int main(int argc, char** argv) {
  scroll5_preview();
  return 0;
 }