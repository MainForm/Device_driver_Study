# I2C Slave 테스트 코드

해당 코드는 리눅스 I2C 디바이스 드라이버 실습을 위한 Arduino slave 프로그램입니다.
Arduino는 I2C 주소 `0x10`에서 master의 명령을 수신하고, 장치 ID 확인과 내장 LED
제어 기능을 제공합니다.

## 지원 보드

- Arduino Uno R3

## 동작 방법

1. Arduino IDE에서 `i2c_slave_test.ino`를 엽니다.
2. 보드를 `Arduino Uno`로 선택하고 코드를 업로드합니다.
3. I2C master와 Arduino의 SDA, SCL, GND를 연결합니다.
4. master에서 slave 주소 `0x10`으로 2바이트 명령을 전송합니다.
5. 같은 주소에서 1바이트를 읽어 명령 처리 결과를 확인합니다.

Arduino Uno R3의 I2C 핀은 다음과 같습니다.

| I2C 신호 | Arduino Uno R3 |
| --- | --- |
| SDA | SDA 핀 또는 A4 |
| SCL | SCL 핀 또는 A5 |
| GND | GND |

> Arduino Uno R3는 5V 로직을 사용하고 Raspberry Pi GPIO는 3.3V 로직을 사용합니다.
> Raspberry Pi와 연결할 때는 양방향 I2C 레벨 시프터를 사용하고, SDA와 SCL이 5V로
> pull-up되지 않도록 확인해야 합니다. 두 보드의 GND는 반드시 공통으로 연결합니다.

### 통신 형식

master가 전송하는 명령은 2바이트로 구성됩니다.

```text
Byte 0: 명령 코드
Byte 1: 명령 인자
```

slave는 명령을 처리한 후 1바이트 응답을 준비합니다. master가 읽기를 요청하면
`onRequest()` 콜백이 호출되어 준비된 응답을 전송합니다.

```text
Master                             Arduino slave (0x10)
  |                                       |
  |--- command + argument (2 bytes) ----->|
  |                                       | 명령 처리
  |<---------- response (1 byte) ---------|
```

## 명령어

| 명령 | 코드 | 인자 | 응답 | 동작 |
| --- | --- | --- | --- | --- |
| 장치 ID 확인 | `0x01` | `0x00` | `0x42` | 테스트 장치 ID를 반환합니다. |
| LED 끄기 | `0x02` | `0x00` | `0x00` | 내장 LED를 끄고 적용한 값을 반환합니다. |
| LED 켜기 | `0x02` | `0x01` | `0x01` | 내장 LED를 켜고 적용한 값을 반환합니다. |

다음 경우에는 오류 응답 `0xFF`를 반환합니다.

- 2바이트보다 짧은 명령을 수신한 경우
- 지원하지 않는 명령 코드를 수신한 경우
- 장치 ID 명령의 인자가 `0x00`이 아닌 경우
- LED 명령의 인자가 `0x00` 또는 `0x01`이 아닌 경우

2바이트를 초과하는 데이터가 들어오면 처음 2바이트만 명령과 인자로 사용하고,
나머지 데이터는 수신 버퍼에서 제거합니다.
