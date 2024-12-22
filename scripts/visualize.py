import vtk
import numpy as np
from PyQt5.QtWidgets import QApplication, QMainWindow, QWidget, QVBoxLayout, QSlider
from PyQt5.QtCore import Qt
from vtkmodules.qt.QVTKRenderWindowInteractor import QVTKRenderWindowInteractor
import sys
import pandas as pd
import matplotlib.pyplot as plt

def visualize_volume(centroids, values, patch_indices, r, g, b, d_seed):
    # Convert centroids to numpy array if not already
    centroids = np.array(centroids)

    # Calculate the midpoint of the volume
    max_coords = np.max(centroids, axis=0)
    min_coords = np.min(centroids, axis=0)
    midpoint = (max_coords + min_coords) / 2

    # Center the coordinates by subtracting the midpoint
    centered_centroids = centroids - midpoint

    # Create and show the viewer with centered coordinates
    app = QApplication.instance() or QApplication(sys.argv)
    viewer = VolumeViewer(centered_centroids, values, patch_indices, r, g, b, d_seed)
    viewer.show()
    app.exec_()


class VolumeViewer(QMainWindow):
    def __init__(self, centroids, values, patch_indices, r, g, b, d_seed):
        super().__init__()
        self.centroids = np.array(centroids)
        self.values = np.nan_to_num(values, nan=0.0, posinf=0.0, neginf=0.0)
        self.patch_indices = np.array(patch_indices)
        self.r = np.array(r, dtype=np.uint8)
        self.g = np.array(g, dtype=np.uint8)
        self.b = np.array(b, dtype=np.uint8)

        # Normalize base values to [0,1]
        v_min, v_max = 0, 255
        self.normalized_base = ((self.values - v_min) / (v_max - v_min)).astype(np.float32)

        # Initialize modifiers
        self.radius_modifier = 0.0
        self.threshold = 0.0
        self.d_seed = d_seed

        self.setup_ui()
        self.setup_vtk_objects()
        self.setup_visualization()
        self.update_mappings()
        self.update_visualization()

    def setup_vtk_objects(self):
        # Initialize VTK pipeline objects
        self.points = vtk.vtkPoints()
        self.radius_scalars = vtk.vtkFloatArray()
        self.colors_vtk = vtk.vtkUnsignedCharArray()
        self.colors_vtk.SetNumberOfComponents(3)
        self.colors_vtk.SetName("Colors")

        # Set initial points and scalars
        max_coord = np.max(np.abs(self.centroids))
        scale_factor = 99 / max_coord if max_coord > 0 else 1
        scaled_points = self.centroids * scale_factor

        for i, (point, value) in enumerate(zip(scaled_points, self.normalized_base)):
            # Points are already centered, just need to swap z and x for VTK
            z, y, x = point
            self.points.InsertNextPoint(x, y, z)
            self.radius_scalars.InsertNextValue(value)
            self.colors_vtk.InsertNextTuple3(
                int(self.r[i]),
                int(self.g[i]),
                int(self.b[i])
            )

        self.polydata = vtk.vtkPolyData()
        self.polydata.SetPoints(self.points)
        self.polydata.GetPointData().SetScalars(self.radius_scalars)
        self.polydata.GetPointData().AddArray(self.colors_vtk)

        self.sphere = vtk.vtkSphereSource()
        self.sphere.SetPhiResolution(4)
        self.sphere.SetThetaResolution(4)
        self.sphere.SetRadius(1.0)

        self.glyph3D = vtk.vtkGlyph3D()
        self.glyph3D.SetSourceConnection(self.sphere.GetOutputPort())
        self.glyph3D.SetInputData(self.polydata)
        self.glyph3D.SetScaleModeToScaleByScalar()
        self.glyph3D.SetScaleFactor(1.0 * self.d_seed)

    def setup_visualization(self):
        self.renderer = vtk.vtkRenderer()
        self.vtk_widget.GetRenderWindow().AddRenderer(self.renderer)
        self.renderer.SetBackground(0.01, 0.01, 0.01)

        # Enable shadows
        self.renderer.SetUseShadows(1)
        self.renderer.SetTwoSidedLighting(True)

        self.mapper = vtk.vtkPolyDataMapper()
        self.mapper.SetInputConnection(self.glyph3D.GetOutputPort())
        self.mapper.SetScalarModeToUsePointFieldData()
        self.mapper.SelectColorArray("Colors")
        self.mapper.SetColorModeToDirectScalars()

        self.actor = vtk.vtkActor()
        self.actor.SetMapper(self.mapper)
        self.actor.GetProperty().SetSpecular(0.5)
        self.actor.GetProperty().SetSpecularPower(40)
        self.actor.GetProperty().SetAmbient(0.5)
        self.actor.GetProperty().SetDiffuse(0.5)

        # Add lighting
        point_light = vtk.vtkLight()
        # Update light position for centered coordinates
        point_light.SetPosition(0, 0, 100)  # Light from above
        point_light.SetFocalPoint(0, 0, 0)  # Focus on center
        point_light.SetIntensity(0.5)
        point_light.SetColor(1, 1, 1)
        point_light.SetConeAngle(180)
        point_light.SetPositional(True)

        ambient_light = vtk.vtkLight()
        ambient_light.SetLightTypeToHeadlight()
        ambient_light.SetIntensity(0.7)

        self.renderer.AddLight(point_light)
        self.renderer.AddLight(ambient_light)
        self.renderer.AddActor(self.actor)

        # Add coordinate axes
        axes = vtk.vtkAxesActor()
        axes.SetTotalLength(20, 20, 20)
        axes.SetShaftType(0)
        axes.SetCylinderRadius(0.02)
        axes.SetConeRadius(0.2)

        axes.SetXAxisLabelText("Z")
        axes.SetYAxisLabelText("Y")
        axes.SetZAxisLabelText("X")

        axes.GetXAxisCaptionActor2D().GetTextActor().SetTextScaleModeToNone()
        axes.GetYAxisCaptionActor2D().GetTextActor().SetTextScaleModeToNone()
        axes.GetZAxisCaptionActor2D().GetTextActor().SetTextScaleModeToNone()

        axes.GetZAxisShaftProperty().SetColor(1, 0, 0)
        axes.GetYAxisShaftProperty().SetColor(0, 1, 0)
        axes.GetXAxisShaftProperty().SetColor(0, 0, 1)

        axes.GetZAxisTipProperty().SetColor(1, 0, 0)
        axes.GetYAxisTipProperty().SetColor(0, 1, 0)
        axes.GetXAxisTipProperty().SetColor(0, 0, 1)

        self.axes_widget = vtk.vtkOrientationMarkerWidget()
        self.axes_widget.SetOrientationMarker(axes)
        self.axes_widget.SetInteractor(self.vtk_widget.GetRenderWindow().GetInteractor())
        self.axes_widget.SetViewport(0.0, 0.0, 0.2, 0.2)
        self.axes_widget.SetEnabled(1)
        self.axes_widget.InteractiveOff()

        self.renderer.ResetCamera()
        self.vtk_widget.Initialize()

    def update_mappings(self):
        if self.radius_modifier >= 0:
            self.radius_values = self.normalized_base * (1 - self.radius_modifier) + self.radius_modifier
        else:
            self.radius_values = self.normalized_base * (1 + self.radius_modifier)

    def update_visualization(self):
        valid_indices = self.normalized_base >= self.threshold
        valid_centroids = self.centroids[valid_indices]
        valid_radius = self.radius_values[valid_indices]
        valid_r = self.r[valid_indices]
        valid_g = self.g[valid_indices]
        valid_b = self.b[valid_indices]

        self.points.Reset()
        self.radius_scalars.Reset()
        self.colors_vtk.Reset()

        if len(valid_centroids) > 0:
            max_coord = np.max(np.abs(valid_centroids))
            scale_factor = 99 / max_coord if max_coord > 0 else 1
            scaled_points = valid_centroids * scale_factor

            for i, (point, radius_val) in enumerate(zip(scaled_points, valid_radius)):
                z, y, x = point  # Swap z and x for VTK
                self.points.InsertNextPoint(x, y, z)
                self.radius_scalars.InsertNextValue(radius_val)
                self.colors_vtk.InsertNextTuple3(
                    int(valid_r[i]),
                    int(valid_g[i]),
                    int(valid_b[i])
                )

        self.polydata.Modified()
        self.vtk_widget.GetRenderWindow().Render()

    def setup_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)

        self.vtk_widget = QVTKRenderWindowInteractor()
        layout.addWidget(self.vtk_widget)

        # Threshold slider
        self.threshold_slider = QSlider(Qt.Horizontal)
        self.threshold_slider.setMinimum(0)
        self.threshold_slider.setMaximum(100)
        self.threshold_slider.setValue(0)
        self.threshold_slider.valueChanged.connect(self.update_threshold)
        layout.addWidget(self.threshold_slider)

        # Radius slider
        self.radius_slider = QSlider(Qt.Horizontal)
        self.radius_slider.setMinimum(-100)
        self.radius_slider.setMaximum(100)
        self.radius_slider.setValue(0)
        self.radius_slider.valueChanged.connect(self.update_radius_modifier)
        layout.addWidget(self.radius_slider)

        self.resize(800, 600)

    def update_radius_modifier(self):
        self.radius_modifier = self.radius_slider.value() / 100.0
        self.update_mappings()
        self.update_visualization()

    def update_threshold(self):
        self.threshold = self.threshold_slider.value() / 100.0
        self.update_visualization()



def load_and_visualize_superpixels(csv_path: str, d_seed: float = 1.0):
    """
    Load superpixels from CSV and visualize them using VTK.

    Args:
        csv_path: Path to the superpixels CSV file
        d_seed: Seed diameter for visualization (controls sphere size)
    """
    try:
        df = pd.read_csv(csv_path)
    except Exception as e:
        print(f"Error reading CSV file: {e}")
        return

    # Extract coordinates and intensity
    centroids = df[['z', 'y', 'x']].values
    intensities = df['intensity'].values

    # Create simple RGB coloring based on intensity
    normalized_intensity = (intensities - intensities.min()) / (intensities.max() - intensities.min())
    viridis_colors = plt.cm.viridis(normalized_intensity)

    # Extract RGB components and convert to 0-255 range
    r = (viridis_colors[:, 0] * 255).astype(np.uint8)
    g = (viridis_colors[:, 1] * 255).astype(np.uint8)
    b = (viridis_colors[:, 2] * 255).astype(np.uint8)

    patch_indices = np.arange(len(centroids))

    visualize_volume(
        centroids=centroids,
        values=intensities,
        patch_indices=patch_indices,
        r=r,
        g=g,
        b=b,
        d_seed=d_seed
    )

def load_and_visualize_chords(superpixels_csv: str, chords_csv: str, d_seed: float = 1.0):
    """
    Load chords and their superpixels from CSV files and visualize using VTK.

    Args:
        superpixels_csv: Path to the superpixels CSV file containing positions
        chords_csv: Path to the chords CSV file
        d_seed: Seed diameter for visualization (controls sphere size)
    """
    # Load superpixels for positions and intensities
    try:
        superpixels_df = pd.read_csv(superpixels_csv)

        # Read chords - each line is comma-separated indices
        with open(chords_csv, 'r') as f:
            next(f)  # Skip header
            chords = [list(map(int, line.strip().split(','))) for line in f]

    except Exception as e:
        print(f"Error reading CSV files: {e}")
        return

    # Create list of all superpixel indices and their positions that are in chords
    all_indices = []
    for chord in chords:
        all_indices.extend(chord)
    all_indices = sorted(set(all_indices))  # Remove duplicates

    # Get positions and intensities for superpixels in chords
    positions = superpixels_df.iloc[all_indices][['z', 'y', 'x']].values
    intensities = superpixels_df.iloc[all_indices]['intensity'].values

    # Create a mapping from superpixel index to chord number
    superpixel_to_chord = {idx: chord_num for chord_num, chord in enumerate(chords) for idx in chord}

    # Create colors based on chord membership
    colors = np.zeros((len(all_indices), 4))
    for i, idx in enumerate(all_indices):
        chord_num = superpixel_to_chord[idx]
        colors[i] = plt.cm.viridis(chord_num % 256 / 255.0)

    # Convert to RGB 0-255 range
    r = (colors[:, 0] * 255).astype(np.uint8)
    g = (colors[:, 1] * 255).astype(np.uint8)
    b = (colors[:, 2] * 255).astype(np.uint8)

    patch_indices = np.arange(len(positions))

    visualize_volume(
        centroids=positions,
        values=intensities,
        patch_indices=patch_indices,
        r=r,
        g=g,
        b=b,
        d_seed=d_seed
    )

if __name__ == "__main__":
    superpixels_csv = "/Volumes/vesuvius/output_1a/superpixels.16.16.16.csv"
    chords_csv = "/Volumes/vesuvius/output_1a/chords.16.16.16.csv"

    # Visualize just superpixels
    # load_and_visualize_superpixels(superpixels_csv)

    # Or visualize chords
    load_and_visualize_chords(superpixels_csv, chords_csv)