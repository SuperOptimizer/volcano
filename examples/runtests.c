#include "../volcano.h"

void testcurl() {
  char* buf;
  long len = download("https://dl.ash2txt.org/full-scrolls/Scroll1/PHercParis4.volpkg/paths/20230503225234/author.txt", &buf);
  assert(len == 6);
  assert(strncmp(buf,"noemi",5) == 0);
  printf("%d\n",(int) len);
}


int main(int argc, char** argv) {
  testcurl();

  zarr_metadata metadata = parse_zarray("D:\\vesuvius.zarr\\Scroll1\\20230205180739\\.zarray");

  printf("Hello World\n");

  chunk* mychunk = tiff_to_chunk("../example_data/example_3d.tif");
  printf("%f\n", mychunk->data[0]);

  slice* myslice = tiff_to_slice("../example_data/example_3d.tif", 0);
  printf("%f\n", myslice->data[0]);

  printf("%f\n", slice_at(myslice, 0, 0));
  printf("%f\n", chunk_at(mychunk, 0, 0, 0));

  chunk* smallchunk = avgpool(mychunk, 4, 4);
  chunk* labels;
  Superpixel* superpixels;
  easy_snic(smallchunk, 4, 10.0f, &labels, &superpixels);


  //mesh* mymesh = march(smallchunk, 32768.0f);
  //write_mesh_to_ply("out.ply", mymesh);


  //mesh_free(mymesh);
  //free(mymesh);
  //mymesh = NULL;

  chunk_free(smallchunk);
  free(smallchunk);
  smallchunk = NULL;
/*
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
*/
  nrrd_t* nrrd = nrrd_read("../example_data/example_volume_raw.nrrd");
  if (!nrrd) {
    printf("Failed to read NRRD file\n");
    return 1;
  }

  printf("Dimensions: %d\n", nrrd->dimension);
  printf("Type: %s\n", nrrd->type);
  printf("Size: ");
  for (int i = 0; i < nrrd->dimension; i++) { printf("%d ", nrrd->sizes[i]); }
  printf("\n");

  uint16_t* data = (uint16_t*)nrrd->data;
  nrrd_free(nrrd);

  return 0;
}
