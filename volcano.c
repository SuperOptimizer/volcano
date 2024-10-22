#include "volcano.h"

struct {
  int z,y,x;
  int b[];
} a;

int main(int argc, char** argv) {
  printf("Hello World\n");
  printf("%llu\n",sizeof a);

  chunk* mychunk = tiff_to_chunk("../example_3d.tif");
  printf("%f\n",mychunk->data[0]);

}

