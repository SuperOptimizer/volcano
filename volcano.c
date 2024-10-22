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

}

