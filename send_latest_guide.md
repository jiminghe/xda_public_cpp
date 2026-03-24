# Send Latest Mode Guide

## Overview

By default, an MTi device in measurement mode continuously streams data at the configured output rate. **Send Latest** mode changes this behavior: the device samples internally at its configured rate but only transmits a packet when it receives an explicit `ReqData` message. This gives the host full control over the transmission rate and timing.

This is useful when:
- You need a precise, software-controlled output rate independent of the device's internal sample rate.
- You are running multiple sensors and need them to deliver data at the same moment.
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

`PeriodicRequestScheduler` (`include/periodic_request_scheduler.h`) manages a single high-accuracy background thread that calls `requestData()` on all registered devices simultaneously at a fixed rate.

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

### API

```cpp
PeriodicRequestScheduler scheduler;

scheduler.registerDevice(device);       // add a device
scheduler.unregisterDevice(device);     // remove a device (safe at any time)

scheduler.start(RequestRate::Hz50);     // start firing at 50 Hz
scheduler.stop();                       // stop the scheduler thread

scheduler.isRunning();                  // bool
scheduler.getRate();                    // RequestRate
```

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
    scheduler->registerDevice(sensor.getDevice());
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

---

## Multi-Sensor Synchronized Example

A single `PeriodicRequestScheduler` fires `requestData()` to all sensors in the same loop iteration, so all sensors deliver their latest packet at the same moment.

```cpp
ImuSensor sensor1, sensor2, sensor3;

sensor1.setSendLatestEnabled(true);
sensor2.setSendLatestEnabled(true);
sensor3.setSendLatestEnabled(true);

sensor1.initialize();
sensor2.initialize();
sensor3.initialize();

PeriodicRequestScheduler scheduler;
scheduler.registerDevice(sensor1.getDevice());
scheduler.registerDevice(sensor2.getDevice());
scheduler.registerDevice(sensor3.getDevice());
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

Every packet is stamped at the earliest point in `CallbackHandler::onLiveDataAvailable()` using `CLOCK_REALTIME` before the mutex is acquired. The timestamp is stored in `TimestampedPacket::receiveTimeNs` (nanoseconds since Unix epoch) and formatted via `utcTimeString()`:

```
UtcTime:1774325614.123456 |SampleTimeFine:4837345 |Roll:-115.23, Pitch:-72.58, Yaw:-0.03
```

```cpp
struct TimestampedPacket {
    XsDataPacket packet;
    uint64_t     receiveTimeNs;   // CLOCK_REALTIME nanoseconds

    std::string utcTimeString();  // returns "seconds.microseconds"
    static uint64_t now();        // current CLOCK_REALTIME in nanoseconds
};
```

---

## File Reference

| File                                    | Role                                                        |
|-----------------------------------------|-------------------------------------------------------------|
| `include/device_configurator.h/cpp`     | `configureSyncSendLatest()`, `setSendLatestEnabled(bool)`   |
| `include/periodic_request_scheduler.h/cpp` | High-accuracy `requestData()` scheduler                  |
| `include/timestamped_packet.h`          | `TimestampedPacket` struct with UTC timestamp              |
| `include/imu_sensor.h`                  | `setSendLatestEnabled(bool)`, `isSendLatestEnabled()`      |
| `xspublic/xstypes/xssyncline.h`         | `XSL_ReqData` definition                                   |
| `xspublic/xstypes/xssyncfunction.h`     | `XSF_SendLatest` definition                                |
| `xspublic/xstypes/xssyncsetting.h`      | `XsSyncSetting` struct                                     |
