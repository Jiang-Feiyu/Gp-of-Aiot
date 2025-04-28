# AIoT Project

This repository contains the code and resources for an AIoT (Artificial Intelligence of Things) project.

**Group name: AIot Pass**

| Name | UID |
| --- | --- |
| JIANG Feiyu | 3035770800 |
| CAO Shuochen | 3036382961 |
| Wang Shiwei | 3036410392|
| WU Jiaxu | 3036410330 |

## Demo Video
- [Connection Demo]
- [Monitoring & Breathing Function Demo]

[Connection Demo]: https://youtu.be/0062GpkVOzM
[Monitoring & Breathing Function Demo]:https://youtu.be/tf9VvY734YM

## Project Structure
```
.
├── CSI/ - CSI-related code
├── benchmark/ - Dataset for benchmarking
├── breathing_rate/ - Breathing rate detection code
├── doc/ - Documentation
├── esp32c5/ - ESP32-C5, including sending and recieving
├── final_report/ - Final report (LaTeX format)
├── motion_detection/ - Motion detection algorithms
└── web/ - Data visualization interface
```

- The final report is in: [final_report.pdf](https://github.com/Jiang-Feiyu/Gp-of-Aiot/blob/main/final_report/final_report.pdf)
- Navigate to the `esp32c5` floder, the sender and reviever is stored respectivly in `.esp32c5/csi_send` and `./esp32c5/csi_recv`.
- The breathing and motion function is written in [.esp32c5](https://github.com/Jiang-Feiyu/Gp-of-Aiot/blob/main/esp32c5). The algorithm is also seperated for testing in [.breathing_rate](https://github.com/Jiang-Feiyu/Gp-of-Aiot/blob/main/breathing_rate) and [.motion_detection](https://github.com/Jiang-Feiyu/Gp-of-Aiot/blob/main/motion_detection).
- For comprehensive project details, please refer to: [Group Project Documentation](https://github.com/Jiang-Feiyu/Gp-of-Aiot/blob/main/doc/group_project_2025.md)
