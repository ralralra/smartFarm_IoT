/*
 * 터치 급수 — 키트 2채널 모터드라이버의 B채널로 미니 워터펌프 돌리기
 * 터치 패드에 손을 대고 있는 동안만 물이 나온다.
 * ---------------------------------------------------------------
 * 배선 (D1 R32 변환 완료 — README 배선 표 참고)
 *   [터치센서 TTP223] VCC → 3.3V   (5V 금지: SIG가 VCC 레벨로 나와서 ESP32 핀 보호)
 *                     GND → GND
 *                     SIG → GPIO25 (확장쉴드 D3 줄 — 블루투스를 안 써서 비어 있어요)
 *   [모터드라이버]    쿨링팬은 MOTOR A 단자 그대로 (D4·D5 줄 = 17·16)
 *                     B-1A → GPIO27 (D6 줄, PWM)
 *                     B-1B → GPIO14 (D7 줄, LOW 고정)
 *                     MOTOR B 단자 → 미니 워터펌프 (팬B 자리를 펌프에 양보!)
 *   ⚠ 펌프가 힘없이 돌면 키트 배터리 홀더로 외부 전원을 연결하세요 (팬과 동일)
 *
 * TTP223 기본(순간) 모드: 터치 중 HIGH, 미터치 LOW
 *   → 모듈 뒷면 TOG 패드를 납땜하면 토글 모드로 바뀌므로 공장 출고 상태를 전제한다.
 *
 * ESP32 Arduino Core 2.x / 3.x 모두 대응 (LEDC 함수 자동 분기)
 */

// ── 핀 정의 ───────────────────────────────────────────────
#define TOUCH_PIN 25     // TTP223 SIG (확장쉴드 D3 줄)
#define PUMP_IA   27     // 모터드라이버 B-1A (D6 줄, PWM)
#define PUMP_IB   14     // 모터드라이버 B-1B (D7 줄, 방향 — LOW 고정)

// ── 동작 파라미터 (필요 시 이 블록만 수정) ────────────────
const int  PUMP_DUTY     = 255;   // 터치 중 가동 듀티 0~255 (255 = 100%)
const int  RAMP_START    = 120;   // 소프트 스타트 시작 듀티 (이보다 낮으면 펌프가 안 돎)
const int  RAMP_STEP_MS  = 8;     // 듀티 1단계 올릴 때 대기 ms (약 0.1초에 걸쳐 상승)

// ── PWM 설정 ──────────────────────────────────────────────
const int PWM_FREQ = 1000;
const int PWM_RES  = 8;
const int PWM_CH   = 0;          // Core 2.x 전용

// ── 상태 변수 ─────────────────────────────────────────────
bool pumpOn     = false;         // 현재 펌프 가동 여부
bool lastTouch  = false;         // 직전 루프의 터치 상태 (전환 감지용)
unsigned long pressStartMs = 0;  // 터치 시작 시각 (가동 시간 기록용)

// ── 저수준 PWM 출력 ───────────────────────────────────────
void setPumpDuty(int duty) {
  duty = constrain(duty, 0, 255);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PUMP_IA, duty);
#else
  ledcWrite(PWM_CH, duty);
#endif
}

// ── 펌프 시동 : 소프트 스타트 ─────────────────────────────
// 기동 순간 전류를 누그러뜨려 5V 라인 강하(ESP32 리셋)를 예방한다.
void pumpStart() {
  for (int d = RAMP_START; d < PUMP_DUTY; d += 5) {
    setPumpDuty(d);
    delay(RAMP_STEP_MS);
  }
  setPumpDuty(PUMP_DUTY);
  pumpOn = true;
}

void pumpStop() {
  setPumpDuty(0);
  pumpOn = false;
}

void setup() {
  Serial.begin(115200);

  pinMode(TOUCH_PIN, INPUT);       // TTP223는 푸시풀 출력 → 풀업/풀다운 불필요
  pinMode(PUMP_IB, OUTPUT);
  digitalWrite(PUMP_IB, LOW);      // 정방향 고정

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PUMP_IA, PWM_FREQ, PWM_RES);
#else
  ledcSetup(PWM_CH, PWM_FREQ, PWM_RES);
  ledcAttachPin(PUMP_IA, PWM_CH);
#endif

  pumpStop();
  Serial.println("준비 완료 — 터치 패드를 누르고 있는 동안 펌프 가동");
}

void loop() {
  bool touched = (digitalRead(TOUCH_PIN) == HIGH);

  // ① 터치 시작 (LOW → HIGH)
  if (touched && !lastTouch) {
    pressStartMs = millis();
    Serial.println("[터치] 펌프 ON");
    pumpStart();
  }

  // ② 터치 해제 (HIGH → LOW)
  else if (!touched && lastTouch) {
    pumpStop();
    Serial.printf("[해제] 펌프 OFF — 가동 %.1f초\n", (millis() - pressStartMs) / 1000.0);
  }

  // ③ 상태 유지 중에는 아무것도 하지 않는다 (이미 ON 또는 OFF 상태)

  lastTouch = touched;
  delay(10);   // 10ms 주기 폴링 — TTP223 내부 디바운스와 함께 충분히 안정적
}
