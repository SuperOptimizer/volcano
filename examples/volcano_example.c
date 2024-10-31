#include "../volcano.h"
#include <getopt.h>


static void print_usage(const char* program_name) {
  fprintf(stderr, "Usage: %s [-s scroll_num | -f fragment_num] -v volume_timestamp \n", program_name);
  fprintf(stderr, "       (--slice x,y:width,height | --chunk x,y,z:width,height,depth) \n");
  fprintf(stderr, "       -o output_path [--verbose]\n\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -s <num>        Specify scroll number (1-4)\n");
  fprintf(stderr, "  -f <num>        Specify fragment number (1-6)\n");
  fprintf(stderr, "  -v <timestamp>  Specify volume timestamp\n");
  fprintf(stderr, "  --slice <pos:dim>  2D slice position and dimensions (x,y:width,height)\n");
  fprintf(stderr, "  --chunk <pos:dim>  3D chunk position and dimensions (x,y,z:width,height,depth)\n");
  fprintf(stderr, "  -o <path>       Output path:\n");
  fprintf(stderr, "                    - For 2D slices: must be a .bmp file\n");
  fprintf(stderr, "                    - For 3D chunks: either a .tif file or directory for .bmp stack\n");
  fprintf(stderr, "  --verbose       Verbose output\n");
  exit(1);
}

static bool parse_position_2d(const char* pos_str, int* x, int* y) { return sscanf(pos_str, "%d,%d", x, y) == 2; }

static bool parse_position_3d(const char* pos_str, int* x, int* y, int* z) {
  return sscanf(pos_str, "%d,%d,%d", x, y, z) == 3;
}

static bool parse_coordinates(const char* spec, bool is_slice,
                              int* start_x, int* start_y, int* start_z,
                              int* size_x, int* size_y, int* size_z) {
  char* colon_ptr = strchr(spec, ':');
  if (!colon_ptr) return false;

  // Split the string at the colon
  size_t pos_len = colon_ptr - spec;
  char* pos_str = malloc(pos_len + 1);
  strncpy(pos_str, spec, pos_len);
  pos_str[pos_len] = '\0';

  bool success;
  if (is_slice) {
    success = parse_position_2d(pos_str, start_x, start_y) &&
      parse_position_2d(colon_ptr + 1, size_x, size_y);
    *start_z = 0;
    *size_z = 1;
  }
  else {
    success = parse_position_3d(pos_str, start_x, start_y, start_z) &&
      parse_position_3d(colon_ptr + 1, size_x, size_y, size_z);
  }

  free(pos_str);
  return success;
}

int main(int argc, char** argv) {
  if (argc == 1) {
    //no args so lets just slap some stuff in argc and argv
  }
  bool source_is_scroll = false;
  u32 source_number = 0;
  u64 volume_timestamp = 0;
  bool is_slice = false;
  u32 start_x = 0, start_y = 0, start_z = 0;
  u32 size_x = 0, size_y = 0, size_z = 0;

  char* output_path = NULL;
  bool output_is_dir = false;
  bool verbose = false;

  static struct option long_options[] = {
    {"slice", required_argument, 0, 'l'},
    {"chunk", required_argument, 0, 'c'},
    {"verbose", no_argument, 0, 'V'},
    {"cache_dir", required_argument, 0, 'C'},
    {0, 0, 0, 0}
  };

  bool source_set = false;
  bool data_type_set = false;
  bool volume_set = false;
  bool output_set = false;

  int opt;
  while ((opt = getopt_long(argc, argv, "s:f:v:o:h", long_options, NULL)) != -1) {
    switch (opt) {
    case 's':
      if (source_set) {
        fprintf(stderr, "Error: Cannot specify both scroll and fragment\n");
        print_usage(argv[0]);
      }
      source_is_scroll = true;
      source_number = atoi(optarg);
      if (source_number < 1 || source_number > 4) {
        fprintf(stderr, "Error: Invalid scroll number (must be 1-4)\n");
        print_usage(argv[0]);
      }
      source_set = true;
      break;

    case 'f':
      if (source_set) {
        fprintf(stderr, "Error: Cannot specify both scroll and fragment\n");
        print_usage(argv[0]);
      }
      source_is_scroll = false;
      source_number = atoi(optarg);
      if (source_number < 1 || source_number > 6) {
        fprintf(stderr, "Error: Invalid fragment number (must be 1-6)\n");
        print_usage(argv[0]);
      }
      source_set = true;
      break;

    case 'v':
      errno = 0; // Reset errno before the call
      char* endptr;
      volume_timestamp = strtoull(optarg, &endptr, 10);
      if (errno == ERANGE) {
        fprintf(stderr, "Error: Volume timestamp out of range\n");
        print_usage(argv[0]);
      }
      if (*endptr != '\0') {
        fprintf(stderr, "Error: Invalid volume timestamp format\n");
        print_usage(argv[0]);
      }
      volume_set = true;
      break;


    case 'l': // --slice
      if (data_type_set) {
        fprintf(stderr, "Error: Cannot specify both slice and chunk\n");
        print_usage(argv[0]);
      }
      is_slice = true;
      if (!parse_coordinates(optarg, true,
                             &start_x, &start_y, &start_z,
                             &size_x, &size_y, &size_z)) {
        fprintf(stderr, "Error: Invalid slice format. Use: x,y:width,height\n");
        print_usage(argv[0]);
      }
      data_type_set = true;
      break;

    case 'c': // --chunk
      if (data_type_set) {
        fprintf(stderr, "Error: Cannot specify both slice and chunk\n");
        print_usage(argv[0]);
      }
      is_slice = false;
      if (!parse_coordinates(optarg, false,
                             &start_x, &start_y, &start_z,
                             &size_x, &size_y, &size_z)) {
        fprintf(stderr, "Error: Invalid chunk format. Use: x,y,z:width,height,depth\n");
        print_usage(argv[0]);
      }
      data_type_set = true;
      break;

    case 'o':
      output_path = optarg;
      size_t len = strlen(optarg);
      if (is_slice) {
        // 2D slices must output to .bmp
        if (len <= 4 || strcasecmp(optarg + len - 4, ".bmp") != 0) {
          fprintf(stderr, "Error: 2D slice output must be a .bmp file\n");
          print_usage(argv[0]);
        }
        output_is_dir = false;
      }
      else {
        // 3D chunks can output to .tif or directory
        if (len > 4 && strcasecmp(optarg + len - 4, ".tif") == 0) { output_is_dir = false; }
        else { output_is_dir = true; }
      }
      output_set = true;
      break;

    case 'V':
      verbose = true;
      break;

    case 'C':
      //TODO: cacheing
      break;


    case 'h':
    default:
      print_usage(argv[0]);
      break;
    }
  }

  if (!source_set || !volume_set || !data_type_set || !output_set) {
    fprintf(stderr, "Error: Missing required arguments\n");
    print_usage(argv[0]);
  }

  if (verbose) {
    printf("Source: %s %d\n", source_is_scroll ? "Scroll" : "Fragment", source_number);
    printf("Volume timestamp: %llu\n", volume_timestamp);
    printf("Data type: %s\n", is_slice ? "Slice" : "Chunk");
    printf("Start position: (%d, %d, %d)\n", start_x, start_y, start_z);
    printf("Dimensions: %dx%dx%d\n", size_x, size_y, size_z);
    printf("Output path: %s\n", output_path);
    printf("Output type: %s\n", output_is_dir ? "Directory" : "Single file");
  }

  zarr_metadata metadata = parse_zarray("D:\\vesuvius.zarr\\Scroll1\\20230205180739\\.zarray");

  printf("Hello World\n");

  chunk* mychunk = tiff_to_chunk("../example_data/example_3d.tif");
  printf("%f\n", mychunk->data[0]);

  slice* myslice = tiff_to_slice("../example_data/example_3d.tif", 0);
  printf("%f\n", myslice->data[0]);

  printf("%f\n", slice_get(myslice, 0, 0));
  printf("%f\n", chunk_get(mychunk, 0, 0, 0));

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
