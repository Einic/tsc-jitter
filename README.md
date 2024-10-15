# Utilizing the TSC Jitter Kernel Module to Improve Clock Source Accuracy

The `tsc_jitter` is based on the `rdtscp` + kernel module collector，defaults to collecting TSC counts every 100ms, and if the TSC count deviation exceeds +/- 5%, it will print out this anomaly.

![Image text](https://github.com/Einic/tsc-jitter/blob/main/20241014141302.png)
![Image text](https://github.com/Einic/tsc-jitter/blob/main/20241014141304.png)