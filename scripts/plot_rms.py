from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd
import argparse

def main(): 
    parser = argparse.ArgumentParser(description="CSV file to plot RMS files from.")
    parser.add_argument("csv")
    args = parser.parse_args()

    df = pd.read_csv(Path(args.csv))

    plt.plot(df['time'], df['rms'])
    plt.xlabel('Time (s)')
    plt.ylabel('RMS')
    plt.title('RMS Over Time')
    plt.grid(True)
    plt.show()

if __name__ == "__main__": 
    main()
