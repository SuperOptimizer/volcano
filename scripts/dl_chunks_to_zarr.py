import os
import zarr
from numcodecs import Blosc
import skimage
import numpy as np
from skimage.filters import unsharp_mask
from numba import jit
import numpy as np
from scipy.optimize import minimize_scalar
import requests
import io
import tifffile
import cv2
from PIL import Image

CACHEDIR='.'

@jit(nopython=True)
def rescale_array(arr):
  min_val = np.min(arr)
  max_val = np.max(arr)

  # Check if the array is constant
  if min_val == max_val:
    return np.zeros_like(arr)

  # Rescale to [0, 1]
  scaled = (arr - min_val) / (max_val - min_val)
  return scaled

#https://github.com/pengyan510/glcae/tree/master
#this version is modified for 3d grayscale
def linearStretching(x_c, x_max, x_min, l):
    return (l - 1) * (x_c - x_min) / (x_max - x_min)


def mapping(h, l):
    cum_sum = np.cumsum(h)
    t = np.ceil((l - 1) * cum_sum + 0.5).astype(np.int64)
    return t


def f(lam, h_i, h_u, l):
    h_tilde = 1 / (1 + lam) * h_i + lam / (1 + lam) * h_u
    t = mapping(h_tilde, l)
    d = 0
    for i in range(l):
        for j in range(i + 1):
            if h_tilde[i] > 0 and h_tilde[j] > 0 and t[i] == t[j]:
                d = max(d, i - j)
    return d


def huePreservation(g_i, i, x_hat_c, l):
    g_i_f = g_i.flatten()
    i_f = i.flatten()
    x_hat_c_f = x_hat_c.flatten()
    g_c = np.zeros(g_i_f.shape)
    g_c[g_i_f <= i_f] = (g_i_f / (i_f + 1e-8) * x_hat_c_f)[g_i_f <= i_f]
    g_c[g_i_f > i_f] = ((l - 1 - g_i_f) / (l - 1 - i_f + 1e-8) * (x_hat_c_f - i_f) + g_i_f)[g_i_f > i_f]
    return g_c.reshape(i.shape)


def fusion(i):
    lap = cv2.Laplacian(i.astype(np.uint8), cv2.CV_16S, ksize=3)
    c_d = np.array(cv2.convertScaleAbs(lap))
    c_d = c_d / (np.max(c_d) + 1e-8) + 0.00001
    i_scaled = (i - np.min(i)) / (np.max(i) - np.min(i) + 1e-8)
    b_d = np.exp(- (i_scaled - 0.5) ** 2 / (2 * 0.2 ** 2))
    w_d = np.minimum(c_d, b_d)
    return w_d

def global_local_contrast_3d(x):
    # Convert input to float64
    x = x.astype(np.float32)
    x_max = np.max(x)
    x_min = np.min(x)

    l = 256
    x_hat = (l - 1) * (x - x_min) / (x_max - x_min + 1e-8)
    i = np.clip(x_hat, 0, 255).astype(np.uint8)

    h_i = np.bincount(i.flatten(), minlength=l) / i.size
    h_u = np.ones(l) / l

    result = minimize_scalar(f, method="brent", args=(h_i, h_u, l))
    h_tilde = 1 / (1 + result.x) * h_i + result.x / (1 + result.x) * h_u
    t = mapping(h_tilde, l)
    g_i = np.take(t, i)

    g = huePreservation(g_i, i, x_hat, l)

    clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8, 8))
    l_i = np.zeros_like(i)
    for slice_idx in range(i.shape[0]):
        l_i[slice_idx] = clahe.apply(i[slice_idx])
    l = huePreservation(l_i, i, x_hat, l)

    w_g = fusion(g_i)
    w_l = fusion(l_i)
    w_hat_g = w_g / (w_g + w_l + 1e-8)
    w_hat_l = w_l / (w_g + w_l + 1e-8)
    y = w_hat_g * g + w_hat_l * l

    # Handle NaN and infinity values, then rescale back to uint8
    y = np.nan_to_num(y, nan=0, posinf=255, neginf=0)
    y = np.clip(y, 0, 255)
    return rescale_array(y.astype(np.float32))




def download(url):
  path = url.replace("https://", "")
  path = os.path.join(CACHEDIR,path)
  if not os.path.exists(path):
    print(f"downloading {url}")
    response = requests.get(url)
    if response.status_code == 200:
      os.makedirs(os.path.dirname(path), exist_ok=True)
      with io.BytesIO(response.content) as filedata, tifffile.TiffFile(filedata) as tif:
        data = (tif.asarray() >> 8).astype(np.uint8) & 0xf0
        tifffile.imwrite(path, data)
    else:
      raise Exception(f'Cannot download {url}')
  else:
    print(f"getting {path} from cache")

  if url.endswith('.tif'):
      tif = tifffile.imread(path)
      data = tif
      if data.dtype == np.uint16:
        return ((data >> 8) & 0xf0).astype(np.uint8)
      else:
        return data
  elif url.endswith('.jpg') or url.endswith('.png'):
    data = np.asarray(Image.open(path))
    return data & 0xf0

class ZVol:
  def __init__(self, basepath, create=False, writeable=False):
    if create:
      if os.path.exists(f"{basepath}/volcano.zvol"):
        raise ValueError("Cannot create a zvol if there is already a zvol there")
      synch = zarr.ProcessSynchronizer(f'{basepath}/volcano.sync')
      self.root = zarr.group(store=f"{basepath}/volcano.zvol", synchronizer=synch,)
      compressor = Blosc(cname='blosclz', clevel=9, shuffle=Blosc.BITSHUFFLE)
      #scroll 1
      self.root.zeros('20230205180739', shape=(14376,7888,8096), chunks=(256,256,256), dtype='u1', compressor=compressor)
      self.root.zeros('20230205180739_downloaded', shape=((14376+499)//500,(7888+499)//500,(8096+499)//500), dtype='u1')
      #self.root.zeros('20230206171837', shape=(10532,7812,8316), chunks=(256,256,256), dtype='u1', compressor=compressor)


      #scroll 2
      #volumes = self.root.zeros('20230206082907', shape=(14376,7888,8096), chunks=(256,256,256), dtype='u1', compressor=compressor)
    else:
      self.root = zarr.open(f"{basepath}/volcano.zvol", mode="r+" if writeable else "r")

  def download_all(self):
    for y in range(1, 17):
      for x in range(1, 18):
        for z in range(1, 30):
          if self.root['20230205180739_downloaded'][z,y,x] == 1:
            print(f"skipping {z} {y} {x}")
            continue
          data = download(
            f"https://dl.ash2txt.org/full-scrolls/Scroll1/PHercParis4.volpkg/volume_grids/20230205180739/cell_yxz_{y:03}_{x:03}_{z:03}.tif")
          mask = data == 0
          data = skimage.restoration.denoise_tv_chambolle(data, weight=.10)
          data = skimage.filters.gaussian(data, sigma=2)
          data = unsharp_mask(data, radius=4.0, amount=2.0)
          data = global_local_contrast_3d(data)
          data = skimage.exposure.equalize_adapthist(data, nbins=16)
          data = data.astype(np.float32)
          data = rescale_array(data)
          data *= 255.
          data = data.astype(np.uint8)
          data[mask] = 0
          data = np.transpose(data, (2, 0, 1))

          self.root['20230205180739'][z, :, :] = data & 0xf0
          self.root['20230205180739_downloaded'][z,y,x] = 1


  def chunk(self, volume, start, size):
    if start[0] + size[0] < 0 or start[0] + size[0] >= self.root[volume].shape[0]:
      raise ValueError(f"{start} {size} out of dimension for {self.root[volume].shape}")
    if start[1] + size[1] < 0 or start[1] + size[1] >= self.root[volume].shape[1]:
      raise ValueError(f"{start} {size} out of dimension for {self.root[volume].shape}")
    if start[2] + size[2] < 0 or start[2] + size[2] >= self.root[volume].shape[2]:
      raise ValueError(f"{start} {size} out of dimension for {self.root[volume].shape}")

    return self.root[volume][start[0]:start[0]+size[0],start[1]:start[1]+size[1],start[2]:start[2]+size[2]]



if __name__ == '__main__':
  zvol = ZVol('d:/', create=False,writeable=True)
  zvol.download_all()
  #zvol.chunk('20230205180739',(1024,1024,1024),(128,128,128))