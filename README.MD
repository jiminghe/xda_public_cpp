# XDA Public C++ CMake Example

## Introduction

This code is based on the MT SDK 2025.0 stable release for Linux x64, with some fixes in the Makefile for `make clean` and `xscommon` dependency. This code was tested on an Ubuntu 22.04 virtual device with the MTI-630 sensor.

## Clone this branch
```bash
git clone --recursive --branch cmake https://github.com/jiminghe/xda_public_cpp.git
```

## Installation

To build the project, execute the following commands:

```bash
cd xda_public_cpp
mkdir build
cd build
cmake ..
make
```

## Running the Program

After building, you can run the program using:

```bash
./mti_example
```

## Sensor and Code Configuration

If you want to change the device output configuration, modify the [`device_configurator.cpp`](./src/device_configurator.cpp) file. Depending on the device you are using, look for the corresponding code sections:

- For MTi-1/610/100, check the code for `device->deviceId().isImu()`.
- For MTi-2/3/320/620/630/200/300, check the code for `device->deviceId().isVru() || device->deviceId().isAhrs()`.
- For GNSS/INS devices such as MTi-7/8/670/680/G-710, check the code for `device->deviceId().isGnss()`.


## Troubleshooting

If the program displays the message `No MTi device found`:

- For the MTi-1/600 product series, where the FTDI chip was used, try the following steps:
  ```bash
  sudo /sbin/modprobe ftdi_sio
  echo 2639 0300 | sudo tee /sys/bus/usb-serial/drivers/ftdi_sio/new_id
  ```
  Then, ensure you are in the `dialout` group:
  ```bash
  ls -l /dev/ttyUSB0
  groups
  ```
  If not, add yourself to the `dialout` group:
  ```bash
  sudo usermod -G dialout -a $USER
  ```
  Finally, reboot your computer:
  ```bash
  sudo reboot
  ```

- For MTi-100/300/G-710 devices:
  ```bash
  git clone https://github.com/xsens/xsens_mt.git
  cd xsens_mt
  sudo make HAVE_LIBUSB=1
  sudo modprobe usbserial
  sudo insmod ./xsens_mt.ko
  ```

- You can specify your own port and baud rate in the [`device_scanner.cpp`](./src/device_scanner.cpp) file:
  ```cpp
  // Change this port name and baud rate to your own values (by default it is 115200, unless you have changed the value with MT Manager).
  m_portInfo = XsScanner::scanPort("/dev/ttyUSB0", XBR_115k2); 
  ```

- If either of the above methods works, try using `cutecom` to see if you can receive `FA FF 36` hex messages:
  ```bash
  sudo apt install cutecom
  cutecom
  ```



