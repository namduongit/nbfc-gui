#include "fan.h"

#include "error.h"
#include "ec.h"
#include "acpi_call.h"

#include <math.h>    // fabs, round
#include <errno.h>   // EINVAL
#include <string.h>  // strlen
#include <stdbool.h>

extern EC_VTable* ec;

Error* Fan_Init(Fan* self, FanConfiguration* cfg, ModelConfig* modelCfg) {
  my.fanConfig            = cfg;
  my.mode                 = Fan_ModeAuto;
  my.criticalTemperature  = modelCfg->CriticalTemperature;
  my.criticalTemperatureOffset = modelCfg->CriticalTemperatureOffset;
  my.readWriteWords       = modelCfg->ReadWriteWords;
  my.minSpeedValueWrite   = cfg->MinSpeedValue;
  my.maxSpeedValueWrite   = cfg->MaxSpeedValue;
  const bool same = ! cfg->IndependentReadMinMaxValues;
  my.minSpeedValueRead    = same ? my.minSpeedValueWrite : cfg->MinSpeedValueRead;
  my.maxSpeedValueRead    = same ? my.maxSpeedValueWrite : cfg->MaxSpeedValueRead;
  my.minSpeedValueReadAbs = min(my.minSpeedValueRead, my.maxSpeedValueRead);
  my.maxSpeedValueReadAbs = max(my.minSpeedValueRead, my.maxSpeedValueRead);
  my.fanSpeedSteps        = my.maxSpeedValueReadAbs - my.minSpeedValueReadAbs;

  return ThresholdManager_Init(&my.threshMan, &cfg->TemperatureThresholds);
}

// ============================================================================
// PRIVATE
// ============================================================================

static inline bool float_eq(const float a, const float b) {
  return fabs(a - b) < 0.06; /* ~ 0.05 */
}

static FanSpeedPercentageOverride* Fan_OverrideByValue(const Fan* self, uint16_t value) {
  for_each_array(FanSpeedPercentageOverride*, o, my.fanConfig->FanSpeedPercentageOverrides)
    if ((o->TargetOperation & OverrideTargetOperation_Read) &&
         o->FanSpeedValue == value)
      return o;

  return NULL;
}

static FanSpeedPercentageOverride* Fan_OverrideByPercentage(const Fan* self, float percentage) {
  for_each_array(FanSpeedPercentageOverride*, o, my.fanConfig->FanSpeedPercentageOverrides)
    if ((o->TargetOperation & OverrideTargetOperation_Write) &&
        float_eq(o->FanSpeedPercentage, percentage))
      return o;

  return NULL;
}

static uint16_t Fan_PercentageToFanSpeed(const Fan* self, float percentage) {
  if (percentage > 100.0f)
    percentage = 100.0f;
  else if (percentage < 0.0f)
    percentage = 0.0f;

  FanSpeedPercentageOverride* override = Fan_OverrideByPercentage(self, percentage);
  if (override)
    return override->FanSpeedValue;

  return round(my.minSpeedValueWrite
      + (((my.maxSpeedValueWrite - my.minSpeedValueWrite) * percentage) / 100.0f));
}

static float Fan_FanSpeedToPercentage(const Fan* self, uint16_t fanSpeed) {
  FanSpeedPercentageOverride* override = Fan_OverrideByValue(self, fanSpeed);
  if (override)
    return override->FanSpeedPercentage;

  // Here we have been preventing a division by zero if both values are
  // the same. This case cannot happen any longer, because it is tested in
  // the config validation code.
  //
  // if (my.minSpeedValueRead == my.maxSpeedValueRead)
  //   return 0.0f;

  return ((float)(fanSpeed - my.minSpeedValueRead) /
     (my.maxSpeedValueRead - my.minSpeedValueRead)) * 100.0f;
}

static Error* Fan_ECWriteValue(Fan* self, uint16_t value) {
  if (my.fanConfig->WriteAcpiMethod) {
    uint64_t out;
    Error* e = AcpiCall_CallTemplate(my.fanConfig->WriteAcpiMethod, value, &out);
    if (e)
      return err_string(e, "WriteAcpiMethod");
    else
      return err_success();
  }

  return my.readWriteWords
    ? ec->WriteWord(my.fanConfig->WriteRegister, value)
    : ec->WriteByte(my.fanConfig->WriteRegister, value);
}

static Error* Fan_ECReadValue(const Fan* self, uint16_t* out) {
  Error* e;

  if (my.fanConfig->ReadAcpiMethod) {
    const ssize_t len = strlen(my.fanConfig->ReadAcpiMethod);
    uint64_t val;
    e = AcpiCall_Call(my.fanConfig->ReadAcpiMethod, len, &val);
    if (e)
      return err_string(e, "ReadAcpiMethod");
    else
      *out = val;
    return e;
  }

  if (my.readWriteWords) {
    uint16_t word;
    e = ec->ReadWord(my.fanConfig->ReadRegister, &word);
    if (!e)
      *out = word;
    return e;
  }
  else {
    uint8_t byte;
    e = ec->ReadByte(my.fanConfig->ReadRegister, &byte);
    if (!e)
      *out = byte;
    return e;
  }
}

// ============================================================================
// PUBLIC
// ============================================================================

float Fan_GetTargetSpeed(const Fan* self) {
  return my.isCritical ? 100.0f : my.targetFanSpeed;
}

float Fan_GetRequestedSpeed(const Fan* self) {
  return my.requestedSpeed;
}

void Fan_SetTemperature(Fan* self, float temperature)
{
  // HandleCritalMode
  if (temperature > my.criticalTemperature)
    my.isCritical = true;
  else if (temperature < (my.criticalTemperature - my.criticalTemperatureOffset))
    my.isCritical = false;

  TemperatureThreshold* threshold = ThresholdManager_AutoSelectThreshold(&my.threshMan, temperature);
  if (my.mode == Fan_ModeAuto)
    my.targetFanSpeed = threshold->FanSpeed;
}

Error* Fan_SetFixedSpeed(Fan* self, float speed) {
  Error* e = NULL;
  my.mode = Fan_ModeFixed;

  if (speed < 0.0f) {
    speed = 0.0f;
    errno = EINVAL;
    e = err_stdlib(0, "speed");
  }
  else if (speed > 100.0f) {
    speed = 100.0f;
    errno = EINVAL;
    e = err_stdlib(0, "speed");
  }

  my.requestedSpeed = speed;
  my.targetFanSpeed = speed;
  return e;
}

void Fan_SetAutoSpeed(Fan* self) {
  my.mode = Fan_ModeAuto;
  my.targetFanSpeed = ThresholdManager_GetCurrentThreshold(&my.threshMan)->FanSpeed;
}

float Fan_GetCurrentSpeed(const Fan* self) {
  return my.currentSpeed;
}

Error* Fan_UpdateCurrentSpeed(Fan* self) {
  uint16_t speed;

  // If the value is out of range 3 or more times,
  // minFanSpeed and/or maxFanSpeed are probably wrong.
  for (range(int, i, 0, 3)) {
    Error* e = Fan_ECReadValue(self, &speed);
    if (e)
      return e;

    if (speed >= my.minSpeedValueReadAbs && speed <= my.maxSpeedValueReadAbs) {
      break;
    }
  }

  my.currentSpeed = Fan_FanSpeedToPercentage(self, speed);
  return err_success();
}

uint16_t Fan_GetSpeedSteps(const Fan* self) {
  return my.fanSpeedSteps;
}

Error* Fan_ECReset(Fan* self) {
  if (! my.fanConfig->ResetRequired)
    return err_success();

  if (my.fanConfig->ResetAcpiMethod) {
    const ssize_t len = strlen(my.fanConfig->ResetAcpiMethod);
    uint64_t out;
    Error* e = AcpiCall_Call(my.fanConfig->ResetAcpiMethod, len, &out);
    if (e)
      return err_string(e, "ResetAcpiMethod");
    else
      return err_success();
  }

  return Fan_ECWriteValue(self, my.fanConfig->FanSpeedResetValue);
}

Error* Fan_ECFlush(Fan* self) {
  const float speed = Fan_GetTargetSpeed(self);
  const uint16_t value = Fan_PercentageToFanSpeed(self, speed);
  return Fan_ECWriteValue(self, value);
}
