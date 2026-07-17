# 已验证测试入口

- `test/test_pi_camera_live.py`：树莓派 USB 摄像头实时绿框预览、跟踪与录像。
- `test/test_pi_target_pose_live.py`：使用标定参数实时验证矩形靶面三维位置和距离。
- `test/calibrate_usb_camera.py`：屏幕棋盘格采集和相机内参标定。
- `test/simulate_basic_aiming.py`：基础要求（2）、（3）的 PyBullet 仿真。

树莓派实时运行示例：

```bash
python3 scripts/test/test_pi_camera_live.py --backend opencv --camera 0 --width 1280 --height 720 --fps 30
```

PyBullet 批量验证：

```powershell
& "C:\Users\35336\.conda\envs\py311\python.exe" scripts\test\simulate_basic_aiming.py --mode all --trials 100
```
