# Algorithm Overview
```mermaid
flowchart LR
    A[Data Acquisition] --> B[Signal Filtering]
    B --> C[Peak & Valley Detection]
    C --> D[Motion Characterization]
    D --> E[Statistical Analysis]
    E --> F[Motion Classification]
```

# Signal Processing
The implementation utilizes a Butterworth bandpass filter, chosen for its maximally flat frequency response in the passband. The filter is designed with the following mathematical formulation. For a digital Butterworth bandpass filter, we calculate the coefficients using these equations:
1. Normalize the cutoff frequencies to the Nyquist frequency:
```
low = lowcut / (fs/2)
high = highcut / (fs/2)
```
2. Calculate center frequency and bandwidth:
```
center_freq = sqrt(low * high)
bandwidth = high - low
```
3. Calculate filter coefficients:
```
alpha = sin(Wn * π/2) / cos(Wn * π/2)

b[0] = alpha
b[1] = 0
b[2] = -alpha

a[0] = 1 + alpha
a[1] = -2 * cos(π * center_freq)
a[2] = 1 - alpha
```
The implementation uses a 4th-order filter (FILTER_ORDER = 4) to achieve sufficient attenuation of noise while preserving the signal characteristics essential for motion detection.

# Peak and Valley Detection
The algorithm identifies local maxima (peaks) and minima (valleys) by comparing each point with its adjacent neighbors. A point is classified as a peak if its amplitude exceeds both its preceding and following points:
```
if (amplitudes[i] > amplitudes[i-1] && amplitudes[i] > amplitudes[i+1])
```
Similarly, a valley is identified when:
```
if (amplitudes[i] < amplitudes[i-1] && amplitudes[i] < amplitudes[i+1])
```
This simple yet effective approach captures significant fluctuations in the CSI signal that correspond to environmental changes caused by movement.

## Motion Characterization

Motion is characterized by analyzing the wave pattern formed by peaks and valleys. The algorithm calculates:

1.  Wave length: Distance between the first and last significant amplitude change

    ```
    wave_length = end - start
    ```

1.  Number of oscillations: Count of peaks and valleys minus 1

    ```
    count = num_peaks + num_valleys - 1
    ```

1.  Breathing rate: Oscillation frequency converted to breaths per minute

    ```
    breath_seconds = wave_length / count / fs
    bpm = 60.0 * breath_seconds
    ```

This approach leverages the principle that human movement causes distinctive periodic patterns in the CSI signal.

## Statistical Analysis

The algorithm employs robust statistical techniques to improve detection reliability:

1.  Outlier Removal: Values outside the physiologically plausible range (12.0-25.0 BPM) are filtered out

    ```
    filtered_size = remove_outliers(bpm, 114, filtered_bpm, 12.0, 25.0)
    ```

1.  Central Tendency Calculation:

    -   Mean: Average of all measurements
    -   Median: Middle value of sorted measurements
    -   Mode: Most frequently occurring value

The threshold values (12.0-25.0) were determined empirically based on typical human breathing rates. Using multiple statistical measures provides robustness against anomalous readings.

## Performance Evaluation

The algorithm's accuracy is evaluated using Mean Absolute Error (MAE):

```
MAE = (1/n) * Σ|predicted_value - ground_truth|
```

This metric quantifies the average deviation between the algorithm's motion detection results and the ground truth, with lower values indicating better performance.