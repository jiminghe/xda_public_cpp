# Send Latest Mode Guide

## Overview

By default, an MTi device in measurement mode continuously streams data at the configured output rate. **Send Latest** mode changes this behavior: the device samples internally at its configured rate but only transmits a packet when it receives an explicit `ReqData` message. This gives the host full control over the transmission rate and timing.

This is useful when:
- You need a precise, software-controlled output rate independent of the device's internal sample rate.
- You are running multiple sensors and need them to deliver data at the same moment with identical UTC timestamps.
- You want to reduce USB/serial bus traffic by pulling data at a lower rate than the internal sample rate.

---

## How It Works

### SDK Sync Setting

Send Latest mode is configured through the MTi's synchronization system using two SDK identifiers defined in `xssyncline.h` and `xssyncfunction.h`:

| Identifier     | Header              | Meaning                                                      |
|----------------|---------------------|--------------------------------------------------------------|
| `XSL_ReqData`  | `xssyncline.h`      | Serial sync line — triggered by sending `XMID_ReqData`      |
| `XSF_SendLatest` | `xssyncfunction.h` | On trigger, transmit the latest internally sampled packet   |

These are combined into an `XsSyncSetting` and applied via `XsDevice::setSyncSettings()` while the device is in **configuration mode**:

```cpp
XsSyncSettingArray syncSettings;
syncSettings.push_back(XsSyncSetting(
    XSL_ReqData,      // line:       triggered by XMID_ReqData message
    XSF_SendLatest,   // function:   transmit latest sample on trigger
    XSP_RisingEdge,   // polarity:   unused for serial line
    1000,             // pulseWidth: unused for serial line (µs)
    0,                // offset:     no delay
    0,                // skipFirst:  act from first trigger
    0,                // skipFactor: act on every trigger
    0,                // clockPeriod: unused (ClockBiasEstimation only)
    0                 // triggerOnce: repeat on every trigger
));
device->setSyncSettings(syncSettings);
```

This is encapsulated in `DeviceConfigurator::configureSyncSendLatest()` and called automatically inside `configureDevice()` when the mode is enabled.

### Triggering Data Transmission

Once the device is in measurement mode with Send Latest configured, it will not transmit any data until `MtDevice::requestData()` is called. This sends the `XMID_ReqData` message over the serial bus, which the device responds to by sending its latest packet.

```cpp
device->requestData();  // device transmits one packet
```

---

## PeriodicRequestScheduler

`PeriodicRequestScheduler` (`include/periodic_request_scheduler.h`) manages one high-accuracy background thread (the scheduler) and one worker thread per device. The scheduler fires all workers simultaneously at a fixed rate; each worker calls `requestData()` on its device immediately.

### Supported Rates

```cpp
enum class RequestRate {
    Hz1   = 1,
    Hz2   = 2,
    Hz4   = 4,
    Hz5   = 5,
    Hz10  = 10,
    Hz20  = 20,
    Hz25  = 25,
    Hz50  = 50,
    Hz100 = 100
};
```

### Timing Accuracy

The scheduler uses `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, ...)` with absolute deadlines. Each wake-up advances the deadline by exactly one period, so timing error does not accumulate over time. If a deadline is missed (overrun), the deadline is reset to `now + period` to prevent a burst of back-to-back requests.

### Batch Timestamp

The SDK processes callbacks from multiple serial ports **sequentially**, not in parallel. Without special handling, `onLiveDataAvailable` would fire for sensor1 and then ~10 ms later for sensor2, producing timestamps ~10 ms apart.

To fix this, the scheduler captures one `CLOCK_REALTIME` timestamp immediately before waking the worker threads. Each worker pushes this shared timestamp into its device's `CallbackHandler` via `setBatchTimestamp()` before calling `requestData()`. When `onLiveDataAvailable` fires, it consumes the pre-set timestamp instead of calling `clock_gettime()` — so all devices in the same batch get an identical `receiveTimeNs`.

```
schedulerLoop (once per period):
  1. clock_gettime(CLOCK_REALTIME) → batchTimestamp
  2. store batchTimestamp atomically
  3. notify_all() → wake all worker threads

workerLoop (one per device, simultaneously woken):
  1. handler->setBatchTimestamp(batchTimestamp)
  2. device->requestData()

onLiveDataAvailable (SDK callback, sequential):
  1. receiveTimeNs = m_batchTimestamp.exchange(0)  // consume pre-set value
  2. if receiveTimeNs == 0: fallback to clock_gettime()  // continuous mode
  3. store receiveTimeNs in TimestampedPacket
```

### API

```cpp
PeriodicRequestScheduler scheduler;

// Always use registerWithScheduler() — it passes both XsDevice* and the
// internal CallbackHandler* needed for the batch timestamp mechanism.
sensor.registerWithScheduler(scheduler);

scheduler.unregisterDevice(device);     // remove a device

scheduler.start(RequestRate::Hz50);     // start firing at 50 Hz
scheduler.stop();                       // stop all threads

scheduler.isRunning();                  // bool
scheduler.getRate();                    // RequestRate
```

> **Note:** Do not call `scheduler.registerDevice(sensor.getDevice())` directly — this bypasses the `CallbackHandler*` and breaks the batch timestamp. Always use `sensor.registerWithScheduler(scheduler)`.

---

## Enabling and Disabling Send Latest Mode

`ImuSensor::setSendLatestEnabled(bool)` must be called **before** `initialize()`.

`configureDevice()` always makes the sync state explicit in both directions — it does not assume the device is in a clean state from a previous session:

| Value | Action during `configureDevice()`                                      |
|-------|------------------------------------------------------------------------|
| `true`  | Applies `XSL_ReqData + XSF_SendLatest` via `setSyncSettings()`     |
| `false` | Clears all sync settings via `setSyncSettings({})` (empty array)   |

This ensures that if a previous run left the device in Send Latest mode, switching to `false` will restore continuous streaming rather than leaving the device waiting for `requestData()` indefinitely.

The `PeriodicRequestScheduler` is only created when `isSendLatestEnabled()` returns `true`. In continuous mode it is not needed and not instantiated.

### Enable (default)

```cpp
ImuSensor sensor;
sensor.setSendLatestEnabled(true);  // optional — true is the default
// configureDevice() will call setSyncSettings(XSL_ReqData + XSF_SendLatest)

if (!sensor.initialize() || !sensor.startLogging()) {
    return -1;
}

sensor.startMeasurement();

// Scheduler is required — device will not send data without requestData() calls
std::unique_ptr<PeriodicRequestScheduler> scheduler;
if (sensor.isSendLatestEnabled()) {
    scheduler = std::make_unique<PeriodicRequestScheduler>();
    sensor.registerWithScheduler(*scheduler);
    scheduler->start(RequestRate::Hz50);
}

// ... main loop ...

if (scheduler) scheduler->stop();
sensor.stopMeasurement();
sensor.stopLogging();
```

### Disable (continuous streaming mode)

```cpp
ImuSensor sensor;
sensor.setSendLatestEnabled(false);
// configureDevice() will call setSyncSettings({}) to clear any previous sync config

if (!sensor.initialize() || !sensor.startLogging()) {
    return -1;
}

sensor.startMeasurement();

// No scheduler needed — device streams data automatically at the configured rate
while (keep_running) {
    if (sensor.hasNewData()) {
        TimestampedPacket tp = sensor.getLatestData();
        // process tp ...
    }
}

sensor.stopMeasurement();
sensor.stopLogging();
```

### Clearing Sync Settings Directly

If you need to clear sync settings without going through `ImuSensor` (e.g. in a standalone script or a one-off reset), put the device into configuration mode first and pass an empty `XsSyncSettingArray`:

```cpp
// device must be in configuration mode
device->gotoConfig();

if (device->setSyncSettings(XsSyncSettingArray())) {
    std::cout << "Sync settings cleared." << std::endl;
} else {
    std::cout << "Warning: could not clear sync settings." << std::endl;
}

device->gotoMeasurement();
```

This is exactly what `DeviceConfigurator::configureDevice()` does internally when `setSendLatestEnabled(false)` is set. Passing an empty array overwrites whatever sync configuration the device currently holds — including any `XSL_ReqData + XSF_SendLatest` settings left by a previous run.

> **Why this matters:** If a process exits without clearing sync settings, the device retains them across power cycles. The next session will start in Send Latest mode even if the new application does not call `requestData()`, causing the device to appear silent. Always call `setSendLatestEnabled(false)` (or clear directly as above) when switching back to continuous streaming.

---

## Multi-Sensor Synchronized Example

A single `PeriodicRequestScheduler` fires `requestData()` to all sensors simultaneously via per-device worker threads. All sensors share one batch timestamp, so their `UtcTime` values are identical.

See [multiple_sensors_guide.md](multiple_sensors_guide.md) for the full two- and three-sensor setup including shared `XsControl` and pre-scan requirements.

```cpp
ImuSensor sensor1;
ImuSensor sensor2(sensor1.getControl(), 1);
ImuSensor sensor3(sensor1.getControl(), 2);

sensor1.setSendLatestEnabled(true);
sensor2.setSendLatestEnabled(true);
sensor3.setSendLatestEnabled(true);

// Construct all sensors before calling initialize() (pre-scan requirement)
sensor1.initialize();
sensor2.initialize();
sensor3.initialize();

PeriodicRequestScheduler scheduler;
sensor1.registerWithScheduler(scheduler);
sensor2.registerWithScheduler(scheduler);
sensor3.registerWithScheduler(scheduler);
scheduler.start(RequestRate::Hz50);  // all 3 sensors triggered together at 50 Hz

// Main loop — collect from each sensor independently
while (keep_running) {
    if (sensor1.hasNewData()) { auto tp = sensor1.getLatestData(); /* ... */ }
    if (sensor2.hasNewData()) { auto tp = sensor2.getLatestData(); /* ... */ }
    if (sensor3.hasNewData()) { auto tp = sensor3.getLatestData(); /* ... */ }
}

scheduler.stop();
```

---

## UTC Timestamps

Every packet is stamped using `CLOCK_REALTIME`. In Send Latest mode the timestamp is captured once by the scheduler before any `requestData()` call (batch timestamp). In continuous mode it is captured at the top of `onLiveDataAvailable()`. The timestamp is stored in `TimestampedPacket::receiveTimeNs` (nanoseconds since Unix epoch) and formatted via `utcTimeString()`:

```
UtcTime:1774325614.123456 |SampleTimeFine:4837345 |Roll:-115.23, Pitch:-72.58, Yaw:-0.03
```

```cpp
struct TimestampedPacket {
    XsDataPacket packet;
    uint64_t     receiveTimeNs;   // CLOCK_REALTIME nanoseconds
    std::string  deviceId;        // from device->deviceId().toString()

    std::string utcTimeString();  // returns "seconds.microseconds"
    static uint64_t now();        // current CLOCK_REALTIME in nanoseconds
};
```

---

## File Reference

| File                                    | Role                                                              |
|-----------------------------------------|-------------------------------------------------------------------|
| `include/device_configurator.h/cpp`     | `configureSyncSendLatest()`, `setSendLatestEnabled(bool)`         |
| `include/periodic_request_scheduler.h/cpp` | Batch timestamp + high-accuracy `requestData()` scheduler      |
| `include/callback_handler.h/cpp`        | `setBatchTimestamp()` / `m_batchTimestamp` consumed in callback   |
| `include/timestamped_packet.h`          | `TimestampedPacket` struct with UTC timestamp and device ID       |
| `include/imu_sensor.h`                  | `setSendLatestEnabled()`, `registerWithScheduler()`               |
| `xspublic/xstypes/xssyncline.h`         | `XSL_ReqData` definition                                          |
| `xspublic/xstypes/xssyncfunction.h`     | `XSF_SendLatest` definition                                       |
| `xspublic/xstypes/xssyncsetting.h`      | `XsSyncSetting` struct                                            |
