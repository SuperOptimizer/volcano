#include "volcano.h"


int main(int argc, char** argv) {
  printf("Hello World\n");
#if 0
  chunk* mychunk = tiff_to_chunk("../example_data/example_3d.tif");
  printf("%f\n",mychunk->data[0]);

  slice* myslice = tiff_to_slice("../example_data/example_3d.tif", 0);
  printf("%f\n",myslice->data[0]);

  printf("%f\n",slice_at(myslice, 0, 0));
  printf("%f\n",chunk_at(mychunk, 0, 0, 0));

  chunk* smallchunk = avgpool(mychunk,4,4);

  mesh* mymesh = march(smallchunk, 32768.0f);
  write_mesh_to_ply("out.ply",mymesh);


  mesh_free(mymesh);
  free(mymesh);
  mymesh = NULL;

  chunk_free(smallchunk);
  free(smallchunk);
  smallchunk = NULL;

  histogram* slice_hist = slice_histogram(myslice, 256);
  histogram* chunk_hist = chunk_histogram(mychunk, 256);

  hist_stats stats = calculate_histogram_stats(slice_hist);
  printf("Mean: %.2f\n", stats.mean);
  printf("Median: %.2f\n", stats.median);
  printf("Mode: %.2f (count: %u)\n", stats.mode, stats.mode_count);
  printf("Standard Deviation: %.2f\n", stats.std_dev);

  write_histogram_to_csv(slice_hist, "slice_histogram.csv");
  write_histogram_to_csv(chunk_hist, "chunk_histogram.csv");

  histogram_free(slice_hist);
  histogram_free(chunk_hist);
#endif
  nrrd_t* nrrd = nrrd_read("../example_data/example_volume_raw.nrrd");
  if (!nrrd) {
    printf("Failed to read NRRD file\n");
    return 1;
  }

  printf("Dimensions: %d\n", nrrd->dimension);
  printf("Type: %s\n", nrrd->type);
  printf("Size: ");
  for (int i = 0; i < nrrd->dimension; i++) {
    printf("%d ", nrrd->sizes[i]);
  }
  printf("\n");

  uint16_t* data = (uint16_t*)nrrd->data;
  nrrd_free(nrrd);

  return 0;
}

