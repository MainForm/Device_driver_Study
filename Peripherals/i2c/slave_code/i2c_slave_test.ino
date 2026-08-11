#include <Wire.h>

// I2C 슬레이브 주소
constexpr uint8_t SLAVE_ADDRESS = 0x10;
// 테스트 장치 ID
constexpr uint8_t DEVICE_ID = 0x42;

// 명령 코드
constexpr uint8_t CMD_DEVICE_ID = 0x01;		// 장치 ID 확인
constexpr uint8_t CMD_LED = 0x02;			// 내장 LED 켜기 및 끄기

// 응답 코드
constexpr uint8_t RESPONSE_ERROR = 0xFF;	// 오류 발생 시 반환하는 값

// 마스터에 반환할 응답 값
volatile uint8_t response = RESPONSE_ERROR;

void onReceive(int byteCount) {
	// 명령과 인자를 처리하려면 최소 2바이트가 필요하다.
	if (byteCount < 2) {
		// I2C 수신 버퍼에 남아 있는 데이터를 비운다.
		while (Wire.available()) Wire.read();
		response = RESPONSE_ERROR;
		return;
	}

	// 수신 데이터의 처음 2바이트를 명령과 인자로 읽는다.
	const uint8_t command = Wire.read();
	const uint8_t argument = Wire.read();
	while (Wire.available()) Wire.read();

	switch (command) {
	case CMD_DEVICE_ID:
		response = (argument == 0x00) ? DEVICE_ID : RESPONSE_ERROR;
		break;

	case CMD_LED:
		if (argument == 0x00 || argument == 0x01) {
		digitalWrite(LED_BUILTIN, argument ? HIGH : LOW);
		response = argument;
		} else {
		response = RESPONSE_ERROR;
		}
		break;

	default:
		response = RESPONSE_ERROR;
		break;
	}
}

// 준비된 응답을 마스터에 반환한다.
void onRequest() {
	Wire.write(response);
}

void setup() {
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, LOW);

	Wire.begin(SLAVE_ADDRESS);
	Wire.onReceive(onReceive);
	Wire.onRequest(onRequest);
}

void loop() {

}
