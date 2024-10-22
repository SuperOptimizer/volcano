import nrrd

# Read the data back from file
readdata, header = nrrd.read("../example_data/example_mask.nrrd")
print(readdata.shape)
print(header)
nrrd.write("../example_data/example_mask_raw.nrrd", readdata, header={'encoding':'raw'})


# Read the data back from file
readdata, header = nrrd.read("../example_data/example_volume.nrrd")
print(readdata.shape)
print(header)
nrrd.write("../example_data/example_volume_raw.nrrd", readdata, header={'encoding':'raw'})