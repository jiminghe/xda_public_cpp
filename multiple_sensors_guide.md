# Multiple Sensors Guide

## Overview

This project supports connecting and running multiple MTi sensors simultaneously on a single host. All sensors share one `XsControl` instance, are configured independently, and are driven by a single `PeriodicRequestScheduler` that fires `requestData()` to every sensor at the same moment — ensuring synchronized data delivery with identical UTC timestamps.

---

## Key Design Decisions

### Shared XsControl

`XsControl` is the SDK's top-level controller that manages all open device connections. Only one instance should exist per process. The first `ImuSensor` creates and owns it; subsequent sensors receive a pointer to it and do not own it.

| Sensor        | Constructor                              | XsControl ownership |
|---------------|------------------------------------------|---------------------|
| First sensor  | `ImuSensor()`                            | Creates and owns    |
| Second sensor | `ImuSensor(sensor1.getControl(), 1)`     | Borrows, does not destruct |
| Third sensor  | `ImuSensor(sensor1.getControl(), 2)`     | Borrows, does not destruct |

On destruction, a borrowing sensor closes only its own port. The owning sensor closes its port and destructs `XsControl` last.

### Pre-Scan Before Opening Any Port

`XsScanner::scanPorts()` communicates with each port to identify connected devices. Once a port is opened (after `initialize()`), it is no longer visible to a subsequent scan — the scanner cannot talk to a device that another connection already holds.

To solve this, the scan is performed in the **`DeviceScanner` constructor** (`preScan()`), before any sensor calls `initialize()`. Since all `ImuSensor` objects are constructed before any of them is initialized, all devices are visible during every scan.

```
Construction phase (no ports open):
  sensor1 constructed → preScan() finds device at index 0, caches portInfo
  sensor2 constructed → preScan() finds device at index 1, caches portInfo

Initialization phase (ports opened one by one):
  sensor1.initialize() → opens cached port for device 0
  sensor2.initialize() → opens cached port for device 1
```

This is why **all `ImuSensor` objects must be constructed before any `initialize()` is called**.

---

## ImuSensor Constructors

```cpp
// Single sensor — creates and owns XsControl, connects to the first MTi device found
ImuSensor sensor1;

// Additional sensors — share XsControl from sensor1, connect to device at deviceIndex
ImuSensor sensor2(sensor1.getControl(), 1);
ImuSensor sensor3(sensor1.getControl(), 2);
```

The `deviceIndex` corresponds to the order in which MTi devices appear during `XsScanner::scanPorts()` — typically matching the USB enumeration order (`/dev/ttyUSB0` = 0, `/dev/ttyUSB1` = 1, etc.).

---

## Two-Sensor Example (Send Latest at 1 Hz)

```cpp
// 1. Construct all sensors first (triggers pre-scan before any port is opened)
ImuSensor sensor1;
ImuSensor sensor2(sensor1.getControl(), 1);

// 2. Configure mode before initialize()
sensor1.setSendLatestEnabled(true);
sensor2.setSendLatestEnabled(true);

sensor1.setDisconnectionCallback([]() { keep_running = false; });
sensor2.setDisconnectionCallback([]() { keep_running = false; });

// 3. Initialize and start logging
if (!sensor1.initialize() || !sensor1.startLogging()) { return -1; }
if (!sensor2.initialize() || !sensor2.startLogging()) { return -1; }

// 4. Start measurement
ImuDataProcessor processor1, processor2;
sensor1.startMeasurement();
sensor2.startMeasurement();

// 5. Single scheduler fires requestData() to both sensors simultaneously
//    registerWithScheduler() passes both the XsDevice* and the internal
//    CallbackHandler* so all devices share one batch timestamp per period.
std::unique_ptr<PeriodicRequestScheduler> scheduler;
if (sensor1.isSendLatestEnabled()) {
    scheduler = std::make_unique<PeriodicRequestScheduler>();
    sensor1.registerWithScheduler(*scheduler);
    sensor2.registerWithScheduler(*scheduler);
    scheduler->start(RequestRate::Hz1);
}

// 6. Main loop — each sensor has its own packet queue
while (keep_running) {
    if (!sensor1.isConnected() || !sensor2.isConnected()) break;

    if (sensor1.hasNewData()) {
        TimestampedPacket tp = sensor1.getLatestData();
        processor1.addData(tp);
        printPacketData(tp);  // prints DeviceID, UtcTime, orientation, etc.
    }
    if (sensor2.hasNewData()) {
        TimestampedPacket tp = sensor2.getLatestData();
        processor2.addData(tp);
        printPacketData(tp);
    }

    XsTime::msleep(1);
}

// 7. Shutdown in reverse order of startup
if (scheduler) scheduler->stop();
sensor1.stopMeasurement();
sensor2.stopMeasurement();
sensor1.stopLogging();
sensor2.stopLogging();
```

---

## Three-Sensor Example

Extending to three sensors requires only adding the third sensor and registering it with the scheduler:

```cpp
ImuSensor sensor1;
ImuSensor sensor2(sensor1.getControl(), 1);
ImuSensor sensor3(sensor1.getControl(), 2);

sensor1.setSendLatestEnabled(true);
sensor2.setSendLatestEnabled(true);
sensor3.setSendLatestEnabled(true);

if (!sensor1.initialize() || !sensor1.startLogging()) { return -1; }
if (!sensor2.initialize() || !sensor2.startLogging()) { return -1; }
if (!sensor3.initialize() || !sensor3.startLogging()) { return -1; }

sensor1.startMeasurement();
sensor2.startMeasurement();
sensor3.startMeasurement();

PeriodicRequestScheduler scheduler;
sensor1.registerWithScheduler(scheduler);
sensor2.registerWithScheduler(scheduler);
sensor3.registerWithScheduler(scheduler);
scheduler.start(RequestRate::Hz50);
```

---

## Output Format

Each packet carries the device ID captured at the moment of receipt in `CallbackHandler::onLiveDataAvailable()`. The output line identifies which device the data came from:

```
DeviceID:0388000C |UtcTime:1774325614.123456 |SampleTimeFine:4837345 |Roll:-115.23, Pitch:-72.58, Yaw:-0.03 |Acc X:0.12, Acc Y:-0.03, Acc Z:9.81 |Gyr X:0.00, Gyr Y:0.01, Gyr Z:-0.00
DeviceID:0388000D |UtcTime:1774325614.123789 |SampleTimeFine:4837346 |Roll:2.14, Pitch:0.87, Yaw:45.00 |Acc X:-0.01, Acc Y:0.02, Acc Z:9.80 |Gyr X:0.01, Gyr Y:0.00, Gyr Z:0.00
```

The two `UtcTime` values will be identical (same nanosecond) because they share one batch timestamp captured before any `requestData()` call is issued (see Batch Timestamp below).

---

## Batch Timestamp — Why UTC Times Are Identical

The SDK processes callbacks from multiple serial ports **sequentially**, not in parallel. Without special handling, `onLiveDataAvailable` fires for sensor1 and then ~10 ms later for sensor2. Each callback capturing its own `clock_gettime()` would produce timestamps ~10 ms apart.

The fix is a shared **batch timestamp**:

```
schedulerLoop (once per period):
  1. clock_gettime(CLOCK_REALTIME) → batchTimestamp
  2. store batchTimestamp in m_batchTimestamp (atomic)
  3. notify_all() → wake all worker threads

workerLoop (one per device, simultaneously woken):
  1. handler->setBatchTimestamp(m_batchTimestamp)
  2. device->requestData()

onLiveDataAvailable (called by SDK, sequentially):
  1. receiveTimeNs = m_batchTimestamp.exchange(0)  // consume pre-set value
  2. if receiveTimeNs == 0: fallback to clock_gettime()  // continuous mode
  3. store receiveTimeNs in TimestampedPacket
```

All devices in the same scheduler tick share the same `batchTimestamp`, so their `TimestampedPacket::receiveTimeNs` (and thus `utcTimeString()`) values are identical regardless of SDK callback ordering.

Use `sensor.registerWithScheduler(scheduler)` (not `scheduler.registerDevice(sensor.getDevice())`) because the scheduler needs the internal `CallbackHandler*` pointer to call `setBatchTimestamp()`. `registerWithScheduler` is the only method that has access to both.

---

## Common Pitfalls

| Problem | Cause | Fix |
|---------|-------|-----|
| Second sensor not found | `initialize()` called on sensor1 before sensor2 is constructed | Construct all sensors before calling any `initialize()` |
| Both sensors connect to the same device | Both scanners see the same port as index 0 | Ensure construction order matches USB enumeration order |
| Only one device found during pre-scan | One device is powered off or not enumerated yet | Check `lsusb` / `ls /dev/ttyUSB*` before running |
| Sensor2 destructs XsControl too early | Sensor2 incorrectly owns the control | Always use `ImuSensor(sensor1.getControl(), N)` for sensors 2+ |

---

## File Reference

| File                               | Role                                                               |
|------------------------------------|--------------------------------------------------------------------|
| `include/imu_sensor.h`             | `ImuSensor(XsControl*, int)` and `registerWithScheduler()`        |
| `include/device_scanner.h/cpp`     | `preScan()`, `DeviceScanner(XsControl*, int)` constructor          |
| `include/periodic_request_scheduler.h/cpp` | Batch timestamp + simultaneous `requestData()` to all devices |
| `include/callback_handler.h/cpp`   | `setBatchTimestamp()` / `m_batchTimestamp` consumed in `onLiveDataAvailable` |
| `include/timestamped_packet.h`     | `deviceId` and `receiveTimeNs` fields, `utcTimeString()`           |
