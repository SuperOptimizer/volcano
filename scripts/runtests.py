import pandas as pd
import matplotlib.pyplot as plt

def load_and_plot_histogram(csv_path):
    plt.style.use('dark_background')
    df = pd.read_csv(csv_path)
    bin_centers = (df['bin_start'] + df['bin_end']) / 2

    plt.figure(figsize=(12, 7))

    plt.bar(bin_centers, df['count'],
            width=(df['bin_end'][0] - df['bin_start'][0]),
            alpha=0.7,
            color='#00bc8c',
            edgecolor= 'white',
            linewidth=0.5)

    total_pixels = df['count'].sum()
    mean_val = (df['count'] * bin_centers).sum() / total_pixels

    plt.title('Value Distribution Histogram', size=14, pad=20)
    plt.xlabel('Pixel Value', size=12)
    plt.ylabel('Frequency', size=12)

    plt.grid(True, alpha=0.3, linestyle='--')
    stats_text = (f'Total Pixels: {total_pixels:,}\n'
                  f'Mean Value: {mean_val:.2f}\n'
                  f'Min Value: {df["bin_start"].min():.2f}\n'
                  f'Max Value: {df["bin_end"].max():.2f}')

    plt.text(0.95, 0.95, stats_text,
             transform=plt.gca().transAxes,
             verticalalignment='top',
             horizontalalignment='right',
             bbox=dict(boxstyle='round',
                       facecolor='black',
                       alpha=0.8,
                       edgecolor='gray'))

    plt.tight_layout()
    plt.show()
    plt.close()

def main():
    load_and_plot_histogram('../cmake-build-release/slice_histogram.csv')

if __name__ == "__main__":
    main()