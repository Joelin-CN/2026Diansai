# 相机标定参数

当前两次独立标定文件为：

```text
usb_camera_screen_1280x720.npz   # 第一组，保留用于对照
usb_camera_1280x720_v2.npz       # 第二组，当前候选正式参数
```

该参数只适用于当前 USB 摄像头、当前对焦状态和 1280x720 分辨率。
黑框检测保持使用原始画面；PnP 解算直接读取内参和畸变参数，不需要
对完整画面执行 `remap`。

树莓派实物尺度验证：

```bash
python3 scripts/test/test_pi_target_pose_live.py \
  --calibration src/calibration/usb_camera_1280x720_v2.npz \
  --target-width-mm 297 --target-height-mm 210
```

目标宽高必须填写检测到的四个外角之间的真实尺寸；如果黑框缩进于
A4 纸边缘，就必须测量黑框外边缘，不能直接填写 297 x 210 mm。
