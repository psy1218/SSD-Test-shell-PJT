# Performance Report

- Clock: `CLOCK_MONOTONIC`
- Repetitions: 5
- Unit: milliseconds

| Metric | Min | Average | Median | Max |
|---|---:|---:|---:|---:|
| process_write_100 | 388.150 | 625.328 | 541.757 | 1121.653 |
| process_read_100 | 449.199 | 614.452 | 474.786 | 1123.216 |
| direct_write_100 | 253.035 | 408.613 | 258.631 | 1017.263 |
| direct_read_100 | 0.143 | 0.221 | 0.149 | 0.491 |
| testapp1 | 1823.780 | 2879.205 | 2005.360 | 4693.249 |
| testapp2 | 232.568 | 379.814 | 352.789 | 534.196 |
| all_automated_tests | 3257.892 | 5116.605 | 3489.672 | 7883.809 |

## Analysis

- Process/direct Write average ratio: 1.53x
- Process/direct Read average ratio: 2783.82x
- The comparison measures process and file-simulator overhead, not physical SSD performance.
