/*
  3단계 — 자동 스마트팜 (키트 예제 12_SmartFarm_KIT의 D1 R32 버전, 인터넷 없이!)
  강낭콩 발아 기준으로 온도·습도·토양습도를 읽어 팬과 LED를 스스로 제어해요.

  ── 자동 규칙 (기준값은 우리 화분에 맞게 조절!) ──
  온도 26℃ 초과      → 팬 ON (더위 식히기)
  온도 21℃ 미만      → 네오픽셀 따뜻한 빛 (보온)
  습도 80% 초과      → 팬 ON (곰팡이 방지 환기)
  토양습도 70% 초과  → 네오픽셀 따뜻한 빛 (과습 말리기 — 키트 자료 방식)
  토양습도 30% 미만  → 시리얼·LCD에 "물 주세요!"
  🆕 터치센서 누르는 동안 → 워터펌프 급수 ("물 주세요"가 뜨면 터치 한 번!)

  ── 배선 (D1 R32 변환 완료 — README 배선 표 참고) ──
  토양수분 → A2 자리(GPIO35) / DHT11 → D2 자리(GPIO26)
  팬 → D4·D5 자리(17·16, 모터드라이버 MOTOR A) / 네오픽셀 → D9 자리(GPIO13)
  🆕 워터펌프 → D6·D7 자리(27·14, 모터드라이버 MOTOR B — 팬B 자리를 펌프에 양보!)
  🆕 터치센서(TTP223) → D3 자리(GPIO25) · VCC는 꼭 3.3V!
  LCD → SDA·SCL을 보드 핀(21·22)에 직접! (쉴드 A4·A5 줄은 안 돼요)
*/

// 제어에 필요한 라이브러리들을 불러온다
#include <DHT.h>                // 온습도 센서 제어용
#include <Adafruit_NeoPixel.h>  // 네오픽셀 LED 제어용
#include <LiquidCrystal_I2C.h>  // I2C 방식 LCD 제어용

// 보드에 연결된 핀 번호들
#define SOIL   35   // 토양수분 센서 (확장쉴드 A2 줄)
#define DHTPIN 26   // 온습도 센서 (확장쉴드 D2 줄)
#define AA 16       // 모터드라이버 팬A 방향1 (D5 줄)
#define AB 17       // 모터드라이버 팬A 방향2 (D4 줄)
#define PA 27       // 모터드라이버 워터펌프 방향1 (D6 줄 — 팬B 자리)
#define PB 14       // 모터드라이버 워터펌프 방향2 (D7 줄)
#define TOUCH 25    // 터치센서 SIG (D3 줄 — 누르는 동안 HIGH)
#define LEDPIN 13   // 네오픽셀 (D9 줄)
#define NUMLED 12   // 네오픽셀 LED 알갱이 개수

#define SOIL_DRY_RAW 4095  // ★ 01_soil로 잰 '공기 중(마름)' 아날로그 값 (이 키트 센서는 마를수록 커요!)
#define SOIL_WET_RAW 950   // ★ 01_soil로 잰 '물속(젖음)' 아날로그 값

// ── 자동 제어 기준값 (강낭콩 발아 생육정보: 21.6~25.8℃ · 습도 50%) ──
#define TEMP_HIGH 26    // 이보다 더우면 팬 켜기
#define TEMP_LOW  21    // 이보다 추우면 보온등 켜기
#define HUMI_HIGH 80    // 이보다 습하면 환기(팬)
#define SOIL_WET  70    // 이보다 젖었으면 말리기(보온등)
#define SOIL_DRY  30    // 이보다 마르면 급수 알림

// 센서·모듈 객체 생성
DHT dht(DHTPIN, DHT11);                                       // 온습도 센서 (핀, 종류)
Adafruit_NeoPixel led(NUMLED, LEDPIN, NEO_GRB + NEO_KHZ800);  // 네오픽셀
LiquidCrystal_I2C lcd(0x27, 16, 2);                           // LCD (I2C 주소 0x27, 16칸×2줄 — 안 나오면 0x3F)

// 팬을 켜고(true) 끄는(false) 함수
void fan(bool on) {
  // "on ? HIGH : LOW" = 조건 연산자: on이 참이면 HIGH, 거짓이면 LOW
  // 한쪽 핀만 HIGH가 되어야 회전, 둘 다 LOW면 정지
  digitalWrite(AA, on ? HIGH : LOW); digitalWrite(AB, LOW);   // 팬A (MOTOR A)
}

// 워터펌프를 켜고(true) 끄는(false) 함수 — 팬과 원리가 똑같아요! (채널만 B)
void pump(bool on) {
  digitalWrite(PA, on ? HIGH : LOW); digitalWrite(PB, LOW);   // 펌프 (MOTOR B)
}

// 네오픽셀을 따뜻한 빛으로 켜고(true) 끄는(false) 함수
void warmLight(bool on) {
  for (int i = 0; i < NUMLED; i++)                            // LED 12개를 하나씩
    led.setPixelColor(i, on ? led.Color(255, 150, 60) : 0);   // 켜면 따뜻한 주황빛, 끄면 0(꺼짐)
  led.show();                                                 // 실제 LED에 반영
}

// 처음 한 번만 실행되는 초기화 구간
void setup() {
  Serial.begin(115200);                    // 시리얼 통신 시작
  pinMode(AA, OUTPUT); pinMode(AB, OUTPUT);  // 모터드라이버 핀 4개를 출력용으로
  pinMode(PA, OUTPUT); pinMode(PB, OUTPUT);  // (팬 핀 2개 + 펌프 핀 2개)
  pinMode(TOUCH, INPUT);                     // 터치센서는 입력용 (TTP223는 풀업 불필요)
  pump(false);                               // 시작할 땐 펌프 확실히 끄기
  dht.begin();                             // 온습도 센서 시작
  led.begin();                             // 네오픽셀 시작
  led.setBrightness(150);                  // 밝기 150 (최대 255)
  lcd.init();                              // LCD 초기화
  lcd.backlight();                         // LCD 백라이트 켜기
}

// 무한 반복 구간
void loop() {
  // ── ① 세 가지 값 측정 ──────────────────────────
  float t = dht.readTemperature();   // 온도(℃)
  float h = dht.readHumidity();      // 습도(%)
  // 2점 보정: 마른 값→0%, 젖은 값→100% (방향이 반대인 센서도 map이 알아서 처리)
  int soilPct = constrain(map(analogRead(SOIL), SOIL_DRY_RAW, SOIL_WET_RAW, 0, 100), 0, 100);
  Serial.printf("T %.1f℃  H %.0f%%  Soil %d%%  → ", t, h, soilPct);   // 시리얼에 출력

  // ── ② 자동 판단 — 조건을 변수에 담아두기 ────────
  // || 는 "또는" — 둘 중 하나만 참이어도 참
  bool needFan  = (t > TEMP_HIGH) || (h > HUMI_HIGH);       // 덥거나 습하면 팬 필요
  bool needWarm = (t < TEMP_LOW) || (soilPct > SOIL_WET);   // 춥거나 과습이면 보온등 필요

  // ── ③ 판단 결과대로 장치 작동 ──────────────────
  fan(needFan);         // 팬 켜기/끄기
  warmLight(needWarm);  // 보온등 켜기/끄기

  // ── ④ LCD 상황판 — 1줄: 온습도 / 2줄: 토양습도 + 상태 ──
  lcd.clear();                       // 이전 글자 지우기
  lcd.setCursor(0, 0);               // 커서를 첫째 줄 처음으로
  lcd.print("T:");   lcd.print(t, 1);              // 온도 (소수점 1자리)
  lcd.print(" H:");  lcd.print(h, 0); lcd.print("%");   // 습도 (정수)
  lcd.setCursor(0, 1);               // 커서를 둘째 줄 처음으로
  lcd.print("Soil:"); lcd.print(soilPct); lcd.print("% ");   // 토양습도
  // 지금 상태를 한 단어로 표시 (위에서부터 차례로 검사 — 먼저 걸리는 것 하나만)
  if (soilPct < SOIL_DRY)  lcd.print("WATER!");   // 말랐어요 — 물 주세요!
  else if (needFan)        lcd.print("FAN");      // 팬 작동 중
  else if (needWarm)       lcd.print("WARM");     // 보온등 작동 중
  else                     lcd.print("OK");       // 모든 조건 정상

  // ── ⑤ 시리얼에도 상태 출력 ─────────────────────
  if (needFan)  Serial.print("팬ON ");
  if (needWarm) Serial.print("보온등ON ");
  if (soilPct < SOIL_DRY) Serial.print("💧물 주세요! (터치센서를 누르면 급수) ");
  Serial.println();   // 줄바꿈

  // ── ⑥ 2초 쉬는 동안 터치 급수 감시 ──────────────
  // delay(2000) 한 방이면 그 사이 터치를 놓쳐요! 10ms씩 200번 쉬면서
  // 터치센서를 계속 확인 — 누르는 동안만 펌프가 돌아요.
  for (int i = 0; i < 200; i++) {
    pump(digitalRead(TOUCH) == HIGH);   // 터치 중 HIGH → 펌프 ON, 떼면 OFF
    delay(10);
  }
}
