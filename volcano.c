#include "volcano.h"


int main(int argc, char** argv) {
  printf("Hello World\n");

  chunk* mychunk = tiff_to_chunk("../example_3d.tif");
  printf("%f\n",mychunk->data[0]);

  slice* myslice = tiff_to_slice("../example_3d.tif", 0);
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


}

