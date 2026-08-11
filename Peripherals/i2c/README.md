# I2C Device

## 목적

이 예제는 Linux 커널 모듈에서 기존 I2C adapter를 가져오고, 특정 주소의 I2C client를 생성하여 peripheral과 데이터를 송수신하는 과정을 학습하기 위한 코드이다. Arduino Uno R3를 I2C slave로 사용하여 장치 ID를 읽고 내장 LED를 제어함으로써 I2C 쓰기와 읽기 동작을 확인한다.

이 모듈은 peripheral용 Device Tree node나 `struct i2c_driver`의 `probe()`를 사용하지 않는다. 대신 I2C 버스 번호와 slave 주소를 코드에 지정하고 `i2c_get_adapter()` 및 `i2c_new_client_device()`로 통신에 필요한 객체를 직접 준비한다. 따라서 범용 제품 드라이버보다는 I2C core API와 자원 관리 과정을 익히기 위한 테스트 모듈에 가깝다.[^writing-clients]

## I2C란?

I2C(Inter-Integrated Circuit)는 SDA와 SCL 두 신호선을 사용하여 하나의 controller가 여러 target 장치와 통신할 수 있는 직렬 버스이다. SDA는 데이터를 전달하고 SCL은 clock을 전달한다. 각 target은 버스 안에서 고유한 주소를 가지며, controller는 주소를 전송하여 통신할 장치를 선택한다.[^i2c-summary]

이 예제에서는 Linux 시스템이 controller 역할을 하고 Arduino가 target 역할을 한다.

```text
Linux I2C controller
        │
        │ I2C bus 1 (SDA/SCL)
        ▼
Arduino Uno R3
7-bit address: 0x10
```

### Adapter와 Client

Linux I2C subsystem은 controller와 버스를 `struct i2c_adapter`로 표현한다. adapter에는 실제 전송을 수행하는 controller driver의 알고리즘, 지원 기능 및 버스 정보가 포함된다.[^i2c-api]

버스에 연결된 개별 peripheral은 `struct i2c_client`로 표현한다. client에는 해당 장치가 속한 adapter와 7-bit I2C 주소 등이 저장된다. `i2c_master_send()`와 `i2c_master_recv()`는 client에 저장된 adapter와 주소를 사용하므로 호출할 때 주소를 별도로 전달하지 않는다.

| 객체 | 이 예제의 값 | 역할 |
| --- | --- | --- |
| I2C adapter | 버스 `1` | 메시지를 실제 버스로 전송하는 controller 표현 |
| I2C client | 주소 `0x10` | Arduino slave 장치 표현 |
| 장치 타입 | `i2c_test` | 생성하는 client의 장치 타입 이름 |

### I2C와 SMBus

SMBus는 I2C를 기반으로 명령 형식과 시간 제약 등을 더 구체적으로 정의한 규격이다. Linux는 일반 I2C 메시지를 위한 API와 SMBus 프로토콜용 helper API를 모두 제공한다.[^smbus-protocol]

이 예제의 Arduino protocol은 2바이트 명령을 쓰고 1바이트 응답을 읽는 단순 I2C protocol이다. 따라서 `I2C_FUNC_I2C` 지원 여부를 확인하고 `i2c_master_send()`와 `i2c_master_recv()`를 사용한다.

## 동작 과정

이 예제의 I2C client 초기화 과정은 Linux Kernel 6.13 공식 문서인
*How to instantiate I2C devices*의 **Method 2: Instantiate the devices explicitly**를
참고했다.[^instantiating-devices] Device Tree에 peripheral을 선언하는 대신
`struct i2c_board_info`에 장치 타입과 주소를 설정하고,
`i2c_new_client_device()`를 호출해 client를 명시적으로 생성한다. 이 방식으로
client를 생성한 모듈은 정리할 때 `i2c_unregister_device()`를 호출해야 한다.

모듈을 적재하면 `i2c_test_init()`이 다음 순서로 실행된다.

1. `i2c_get_adapter(1)`로 I2C 버스 1의 adapter를 가져온다.
2. `i2c_check_functionality()`로 adapter가 일반 I2C 전송을 지원하는지 확인한다.
3. `i2c_new_client_device()`로 adapter의 `0x10` 주소에 I2C client를 등록한다.
4. `COMMAND_GET_DEVICE_ID` 명령을 보내고 1바이트 장치 ID를 읽는다.
5. `COMMAND_SET_LED` 명령을 보내 Arduino의 내장 LED를 켠다.

초기화 도중 오류가 발생하면 이미 획득한 자원을 역순으로 해제한다.

```text
i2c_unregister_device()
        ↓
i2c_put_adapter()
```

모듈을 제거하면 `i2c_test_exit()`이 다음 순서로 실행된다.

1. `COMMAND_SET_LED` 명령을 보내 Arduino의 내장 LED를 끈다.
2. `i2c_unregister_device()`로 I2C client를 제거한다.
3. `i2c_put_adapter()`로 adapter 참조를 반환한다.

## 주요 함수와 매크로

### `i2c_get_adapter()`

```c
struct i2c_adapter *i2c_get_adapter(int nr);
```

| 매개변수 | 타입 | 설명 |
| --- | --- | --- |
| `nr` | `int` | 가져올 I2C adapter의 버스 번호 |

**반환값**

- 성공: 해당 버스의 `struct i2c_adapter` 포인터
- 실패: `NULL`

**목적**

커널에 이미 등록된 I2C adapter를 버스 번호로 찾고, 해당 adapter에 대한 참조를 얻는다. 이 예제는 `i2c_get_adapter(1)`을 호출해 I2C 버스 1을 사용한다. 성공한 호출은 나중에 반드시 `i2c_put_adapter()`와 짝을 이뤄야 한다.

### `i2c_put_adapter()`

```c
void i2c_put_adapter(struct i2c_adapter *adap);
```

| 매개변수 | 타입 | 설명 |
| --- | --- | --- |
| `adap` | `struct i2c_adapter *` | `i2c_get_adapter()`로 얻은 adapter 포인터 |

**반환값**

없음(`void`).

**목적**

`i2c_get_adapter()`로 증가한 adapter 참조를 반환한다. I2C controller 자체를 제거하는 함수가 아니라, 현재 모듈이 더 이상 adapter를 사용하지 않음을 커널에 알린다.

### `i2c_check_functionality()`

```c
static inline int i2c_check_functionality(struct i2c_adapter *adap,
                                          u32 func);
```

| 매개변수 | 타입 | 설명 |
| --- | --- | --- |
| `adap` | `struct i2c_adapter *` | 기능을 확인할 I2C adapter |
| `func` | `u32` | 필요한 기능을 나타내는 `I2C_FUNC_*` 비트 플래그 |

**반환값**

- 요청한 기능을 모두 지원: `0` 이외의 값
- 필요한 기능 중 하나라도 미지원: `0`

**목적**

adapter가 특정 I2C 또는 SMBus 전송 방식을 제공하는지 확인한다. 이 예제는 `I2C_FUNC_I2C`를 전달해 일반 I2C 메시지 전송을 지원하는지 검사한다.[^functionality]

이 함수는 주소 `0x10`에 실제 장치가 존재하는지 확인하지 않는다. 장치의 존재와 통신 상태는 실제 송수신 결과를 통해 확인해야 한다.

### `I2C_BOARD_INFO()`

```c
#define I2C_BOARD_INFO(dev_type, dev_addr) \
        .type = dev_type, .addr = (dev_addr)
```

| 매개변수 | 설명 |
| --- | --- |
| `dev_type` | I2C 장치 타입을 나타내는 문자열 |
| `dev_addr` | I2C 장치의 7-bit 주소 |

**반환값**

함수가 아닌 초기화 매크로이므로 반환값은 없다. 매크로가 확장되면 `struct i2c_board_info`의 `type`과 `addr` 필드를 초기화한다.

**목적**

I2C 장치 타입과 7-bit 주소로 `struct i2c_board_info`의 필수 필드를 초기화한다. 이 예제는 장치 타입 `i2c_test`와 주소 `0x10`을 사용한다.

### `i2c_new_client_device()`

```c
struct i2c_client *
i2c_new_client_device(struct i2c_adapter *adap,
                      struct i2c_board_info const *info);
```

| 매개변수 | 타입 | 설명 |
| --- | --- | --- |
| `adap` | `struct i2c_adapter *` | client를 생성할 I2C adapter |
| `info` | `const struct i2c_board_info *` | 장치 타입, 주소 등 생성할 I2C 장치의 정보 |

**반환값**

- 성공: 새로 생성된 `struct i2c_client` 포인터
- 실패: 오류 코드가 인코딩된 `ERR_PTR`

**목적**

이미 알고 있는 adapter와 `struct i2c_board_info`를 사용해 I2C client device를 명시적으로 생성한다. 생성된 client는 커널 Driver Model에 등록되며, 나중에 `i2c_unregister_device()`로 제거해야 한다.[^i2c-api]

client 생성 성공은 해당 주소에 물리적인 장치가 존재하거나 ACK를 보냈다는 의미가 아니다. 또한 같은 adapter의 같은 주소에 client가 이미 등록되어 있으면 주소 충돌로 생성이 실패할 수 있다.

### `i2c_unregister_device()`

```c
void i2c_unregister_device(struct i2c_client *client);
```

| 매개변수 | 타입 | 설명 |
| --- | --- | --- |
| `client` | `struct i2c_client *` | `i2c_new_*_device()`가 반환한 client 포인터 |

**반환값**

없음(`void`).

**목적**

`i2c_new_client_device()`로 생성한 I2C client device를 Driver Model에서 제거한다. client를 제거한 뒤에는 해당 포인터로 통신하면 안 된다.

### `i2c_master_send()`

```c
int i2c_master_send(const struct i2c_client *client,
                    const char *buf, int count);
```

| 매개변수 | 타입 | 설명 |
| --- | --- | --- |
| `client` | `const struct i2c_client *` | 데이터를 전송할 I2C slave device |
| `buf` | `const char *` | slave에 전송할 데이터 버퍼 |
| `count` | `int` | 전송할 바이트 수 |

**반환값**

- 성공: 실제로 전송한 바이트 수
- 실패: 음수 오류 코드

**목적**

client에 저장된 adapter와 I2C 주소를 사용해 하나의 I2C write 메시지를 전송한다. 요청한 `count`보다 작은 양수가 반환되면 불완전한 전송이므로, 이 예제는 `-EIO`로 처리한다.[^i2c-api]

### `i2c_master_recv()`

```c
int i2c_master_recv(const struct i2c_client *client,
                    char *buf, int count);
```

| 매개변수 | 타입 | 설명 |
| --- | --- | --- |
| `client` | `const struct i2c_client *` | 데이터를 수신할 I2C slave device |
| `buf` | `char *` | slave에서 수신한 데이터를 저장할 버퍼 |
| `count` | `int` | 수신할 바이트 수 |

**반환값**

- 성공: 실제로 수신한 바이트 수
- 실패: 음수 오류 코드

**목적**

client에 저장된 adapter와 I2C 주소를 사용해 하나의 I2C read 메시지를 수신한다. 이 예제는 요청한 1바이트를 모두 받았을 때만 성공으로 처리하고, 불완전한 수신은 `-EIO`로 처리한다.[^i2c-api]

## Example

[`i2c_test_device.c`](./i2c_test_device.c)는 Arduino slave에 2바이트 명령을 전송하고 1바이트 응답을 읽는다. 명령은 다음 형식으로 구성된다.

```text
Byte 0: 명령 코드
Byte 1: 명령 인자
```

master와 slave 사이의 한 번의 명령 처리는 다음 순서로 진행된다.

```text
Linux master                         Arduino slave (0x10)
     |                                       |
     |--- command + argument (2 bytes) ----->|
     |                                       | 명령 처리
     |<---------- response (1 byte) ---------|
```

`send_command()`은 `i2c_master_send()`와 `i2c_master_recv()`를 별도로 호출한다. 따라서 쓰기와 읽기는 각각 독립된 I2C transfer이며 둘 사이에는 STOP 조건이 발생한다. repeated START가 필요한 peripheral이라면 두 개의 `struct i2c_msg`를 구성하여 한 번의 `i2c_transfer()`로 처리해야 한다.[^i2c-api]

### 예제 1: 장치 ID 확인

다음 명령으로 Arduino와 정상적으로 데이터를 송수신할 수 있는지 확인한다.

| 명령 | 코드 | 인자 | 예상 응답 |
| --- | --- | --- | --- |
| 장치 ID 확인 | `0x01` | `0x00` | `0x42` |

모듈은 명령과 인자를 전송하고 응답으로 받은 장치 ID를 커널 로그에 출력한다.

```text
device ID: 0x42
```

### 예제 2: 내장 LED 제어

장치 ID를 읽은 후 다음 명령으로 Arduino의 내장 LED를 켠다.

| 명령 | 코드 | 인자 | 예상 응답 |
| --- | --- | --- | --- |
| LED 켜기 | `0x02` | `0x01` | `0x01` |
| LED 끄기 | `0x02` | `0x00` | `0x00` |

모듈이 정상적으로 적재되면 LED가 켜지고, 모듈을 제거하면 LED가 꺼진다. Arduino slave 코드와 배선 및 전체 명령 규약은 [`slave_code/README.md`](./slave_code/README.md)에서 확인할 수 있다.

## 빌드 및 실행

먼저 Arduino Uno R3에 [`slave_code/i2c_slave_test.ino`](./slave_code/i2c_slave_test.ino)를 업로드하고 Linux I2C bus에 연결한다. Raspberry Pi와 Arduino 사이의 전압 차이와 배선 방법은 [`slave_code/README.md`](./slave_code/README.md)의 주의사항을 확인한다.

현재 Linux 시스템에 등록된 I2C adapter 번호를 확인한다.

```bash
ls -l /sys/class/i2c-adapter
```

이 예제는 버스 번호 `1`을 사용하므로 `i2c-1`이 존재해야 한다. Raspberry Pi에서 controller가 비활성화되어 있다면 먼저 시스템 설정을 통해 I2C controller를 활성화해야 한다.

모듈을 빌드하고 적재한다.

```bash
make
sudo insmod build/i2c_test_device.ko
```

커널 로그와 생성된 I2C client를 확인한다.

```bash
sudo dmesg | tail
ls -l /sys/bus/i2c/devices/1-0010
```

`1-0010`은 I2C 버스 `1`과 7-bit 주소 `0x10`을 조합한 Linux 장치 이름이다.

모듈을 제거하고 빌드 결과를 정리한다.

```bash
sudo rmmod i2c_test_device
make clean
```

> 모듈이 적재된 동안에는 같은 adapter의 주소 `0x10`에 다른 I2C client를 생성하거나 `i2c-tools`로 동시에 접근하지 않는 것이 안전하다.

## Linux 공식 문서

- [I2C/SMBus Subsystem](https://www.kernel.org/doc/html/v6.13/i2c/index.html): Linux I2C subsystem의 개념, protocol, 장치 생성 및 드라이버 작성 문서 모음
- [How to instantiate I2C devices](https://www.kernel.org/doc/html/v6.13/i2c/instantiating-devices.html): I2C 장치를 생성하는 방법과 **Method 2: Instantiate the devices explicitly** 설명
- [Implementing I2C device drivers](https://www.kernel.org/doc/html/v6.13/i2c/writing-clients.html): `struct i2c_driver`, `struct i2c_client`, 초기화 및 송수신 방법
- [I2C and SMBus Subsystem API](https://www.kernel.org/doc/html/v6.13/driver-api/i2c.html): `i2c_new_client_device()`, `i2c_master_send()`, `i2c_master_recv()` 등 I2C core API
- [I2C/SMBus Functionality](https://www.kernel.org/doc/html/v6.13/i2c/functionality.html): adapter 기능 플래그와 `i2c_check_functionality()` 사용 방법
- [I2C/SMBus Fault Codes](https://www.kernel.org/doc/html/v6.13/i2c/fault-codes.html): I2C 전송 함수에서 사용하는 대표적인 오류 코드
- [The I2C Protocol](https://www.kernel.org/doc/html/v6.13/i2c/i2c-protocol.html): START, STOP, 주소 및 메시지 전송 방식
- [The SMBus Protocol](https://www.kernel.org/doc/html/v6.13/i2c/smbus-protocol.html): Linux에서 제공하는 SMBus protocol과 helper 함수
- [Building External Modules](https://www.kernel.org/doc/html/v6.13/kbuild/modules.html): 외부 커널 모듈의 Kbuild 파일 작성법과 빌드 방법

[^writing-clients]: Linux Kernel 6.13 공식 문서의 [Implementing I2C device drivers](https://www.kernel.org/doc/html/v6.13/i2c/writing-clients.html) 참고.

[^i2c-summary]: Linux Kernel 6.13 공식 문서의 [Introduction to I2C and SMBus](https://www.kernel.org/doc/html/v6.13/i2c/summary.html) 참고.

[^i2c-api]: Linux Kernel 6.13 공식 문서의 [I2C and SMBus Subsystem API](https://www.kernel.org/doc/html/v6.13/driver-api/i2c.html) 참고.

[^functionality]: Linux Kernel 6.13 공식 문서의 [I2C/SMBus Functionality](https://www.kernel.org/doc/html/v6.13/i2c/functionality.html) 참고.

[^smbus-protocol]: Linux Kernel 6.13 공식 문서의 [The SMBus Protocol](https://www.kernel.org/doc/html/v6.13/i2c/smbus-protocol.html) 참고.

[^instantiating-devices]: Linux Kernel 6.13 공식 문서의 [How to instantiate I2C devices - Method 2: Instantiate the devices explicitly](https://www.kernel.org/doc/html/v6.13/i2c/instantiating-devices.html#method-2-instantiate-the-devices-explicitly) 참고.
