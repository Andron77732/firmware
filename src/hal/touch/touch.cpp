#include "touch.h"

#include "esp_log.h"

static const char *TAG = "Touch";

Touch touch;

void Touch::begin(TFT_eSPI &tft, const TouchSettings &settings,
                  uint8_t rotation) {
  _tft = &tft;
  _enabled = settings.enabled;
  _initialized = true;
  _lastPollMs = 0;

  resetState_();
  setRotation(rotation);
  applyCalibration(settings.calibration);

  ESP_LOGI(TAG, "Initialized: enabled=%d calibrated=%d rotation=%u", _enabled,
           _calibrated, (unsigned)_rotation);
}

void Touch::update() {
  if (!_initialized || !_enabled) {
    return;
  }

  const uint32_t now = millis();
  if ((uint32_t)(now - _lastPollMs) < POLL_INTERVAL_MS) {
    return;
  }
  _lastPollMs = now;

  TouchPoint point{};
  const bool hasTouch = _calibrated && readRaw_(point);

  switch (_state) {
  case State::Idle:
    if (hasTouch) {
      _state = State::PressCandidate;
      _stateTsMs = now;
      _candidatePoint = point;
    }
    break;

  case State::PressCandidate:
    if (!hasTouch) {
      _state = State::Idle;
      break;
    }

    _candidatePoint = point;
    if ((uint32_t)(now - _stateTsMs) >= DEBOUNCE_DOWN_MS) {
      _state = State::Pressed;
      _lastStablePoint = point;
      pushEvent_(TouchEventType::Press, _lastStablePoint);
    }
    break;

  case State::Pressed:
    if (!hasTouch) {
      _state = State::ReleaseCandidate;
      _stateTsMs = now;
      break;
    }

    _lastRawPoint = point;
    if (movedEnough_(_lastStablePoint, point)) {
      _lastStablePoint = point;
      pushEvent_(TouchEventType::Move, _lastStablePoint);
    }
    break;

  case State::ReleaseCandidate:
    if (hasTouch) {
      _state = State::Pressed;
      _lastRawPoint = point;
      if (movedEnough_(_lastStablePoint, point)) {
        _lastStablePoint = point;
        pushEvent_(TouchEventType::Move, _lastStablePoint);
      }
      break;
    }

    if ((uint32_t)(now - _stateTsMs) >= DEBOUNCE_UP_MS) {
      _state = State::Idle;
      pushEvent_(TouchEventType::Release, _lastStablePoint);
    }
    break;
  }
}

bool Touch::pollEvent(TouchEvent &out) {
  if (_queueCount == 0) {
    return false;
  }

  out = _queue[_queueHead];
  _queueHead = (_queueHead + 1) % QUEUE_CAPACITY;
  _queueCount--;
  return true;
}

bool Touch::isPressed() const {
  return _state == State::Pressed || _state == State::ReleaseCandidate;
}

void Touch::setRotation(uint8_t rotation) { _rotation = rotation; }

bool Touch::applyCalibration(const TouchCalibration &cal) {
  if (!_tft) {
    _calibrated = false;
    return false;
  }

  if (!cal.valid) {
    _calibrated = false;
    resetState_();
    ESP_LOGW(TAG, "Touch calibration is not valid, input is blocked");
    return false;
  }

  uint16_t data[5];
  for (uint8_t i = 0; i < 5; i++) {
    data[i] = cal.data[i];
  }

  _tft->setTouch(data);
  _calibrated = true;
  resetState_();
  ESP_LOGI(TAG, "Touch calibration applied");
  return true;
}

bool Touch::runCalibrationWizard(TouchCalibration &outCalibration) {
  if (!_tft) {
    return false;
  }

  uint16_t data[5] = {0, 0, 0, 0, 0};
  _tft->fillScreen(TFT_BLACK);
  _tft->setTextColor(TFT_WHITE, TFT_BLACK);
  _tft->setTextSize(2);
  _tft->setCursor(8, 8);
  _tft->print("Touch calibration");
  _tft->setTextSize(1);
  _tft->setCursor(8, 32);
  _tft->print("Tap corners as requested");

  _tft->calibrateTouch(data, TFT_MAGENTA, TFT_BLACK, 15);

  for (uint8_t i = 0; i < 5; i++) {
    outCalibration.data[i] = data[i];
  }
  outCalibration.valid = true;

  return applyCalibration(outCalibration);
}

bool Touch::readRaw_(TouchPoint &point) {
  if (!_tft) {
    return false;
  }

  uint16_t x = 0;
  uint16_t y = 0;
  if (!_tft->getTouch(&x, &y)) {
    return false;
  }

  point.x = x;
  point.y = y;
  point.ts_ms = millis();
  _lastRawPoint = point;
  return true;
}

void Touch::pushEvent_(TouchEventType type, const TouchPoint &point) {
  TouchEvent event;
  event.type = type;
  event.point = point;

  if (_queueCount >= QUEUE_CAPACITY) {
    if (type == TouchEventType::Move) {
      return;
    }

    if (!dropOldestMove_()) {
      _queueHead = (_queueHead + 1) % QUEUE_CAPACITY;
      _queueCount--;
    }
  }

  _queue[_queueTail] = event;
  _queueTail = (_queueTail + 1) % QUEUE_CAPACITY;
  _queueCount++;
}

bool Touch::dropOldestMove_() {
  if (_queueCount == 0) {
    return false;
  }

  int moveOffset = -1;
  for (uint8_t i = 0; i < _queueCount; i++) {
    uint8_t idx = (_queueHead + i) % QUEUE_CAPACITY;
    if (_queue[idx].type == TouchEventType::Move) {
      moveOffset = i;
      break;
    }
  }

  if (moveOffset < 0) {
    return false;
  }

  for (uint8_t i = (uint8_t)moveOffset; i + 1 < _queueCount; i++) {
    uint8_t from = (_queueHead + i + 1) % QUEUE_CAPACITY;
    uint8_t to = (_queueHead + i) % QUEUE_CAPACITY;
    _queue[to] = _queue[from];
  }

  _queueTail = (_queueTail + QUEUE_CAPACITY - 1) % QUEUE_CAPACITY;
  _queueCount--;
  return true;
}

bool Touch::movedEnough_(const TouchPoint &from, const TouchPoint &to) const {
  const uint16_t dx = (from.x > to.x) ? (from.x - to.x) : (to.x - from.x);
  const uint16_t dy = (from.y > to.y) ? (from.y - to.y) : (to.y - from.y);
  return dx >= MOVE_THRESHOLD_PX || dy >= MOVE_THRESHOLD_PX;
}

void Touch::resetState_() {
  _state = State::Idle;
  _stateTsMs = 0;
  _candidatePoint = TouchPoint{};
  _lastStablePoint = TouchPoint{};
  _lastRawPoint = TouchPoint{};
  _queueHead = 0;
  _queueTail = 0;
  _queueCount = 0;
}
