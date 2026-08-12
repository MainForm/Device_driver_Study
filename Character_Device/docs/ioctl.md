# Character Device의 `ioctl()`

## 목적

`read()`와 `write()`는 장치의 데이터를 읽고 쓰는 표준 인터페이스이다.
반면 `ioctl()`(input/output control)은 버퍼 초기화, 모드 설정,
상태 조회처럼 장치마다 다른 **제어 명령**을 사용자 공간에
제공할 때 사용한다.

이 예제는 다음 네 가지 명령을 통해 `ioctl()`의 데이터 전달 방향을
확인한다.

| 명령 | 데이터 방향 | 동작 |
| --- | --- | --- |
| `CHAR_IOCTL_CLEAR` | 없음 | 장치 데이터 크기와 호출한 파일의 offset을 0으로 설정 |
| `CHAR_IOCTL_SET_MODE` | 사용자 → 커널 | 사용자가 전달한 모드를 장치에 저장 |
| `CHAR_IOCTL_GET_MODE` | 커널 → 사용자 | 현재 장치 모드를 사용자에게 전달 |
| `CHAR_IOCTL_SWAP_MODE` | 사용자 ↔ 커널 | 새 모드를 저장하고 이전 모드를 사용자에게 반환 |

## `ioctl()` 호출 과정

사용자 프로그램은 장치 파일을 연 후 파일 디스크립터, 명령,
선택적인 인자를 `ioctl()`에 전달한다.

```c
int ioctl(int fd, unsigned long request, ...);
```

VFS는 `fd`가 가리키는 `struct file`을 찾은 뒤, 해당 장치의
`file_operations` 구조체에 등록된 `unlocked_ioctl` 콜백을 호출한다.

```c
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = charDeviceOpen,
    .release = charDeviceRelease,
    .write = charDeviceWrite,
    .read = charDeviceRead,
    .unlocked_ioctl = charDeviceIOCTL,
};
```

전체 호출 경로는 다음과 같다.

```text
사용자 공간 ioctl(fd, command, argument)
                    │
                    ▼
           fd가 가리키는 struct file
                    │
                    ▼
          file_operations.unlocked_ioctl
                    │
                    ▼
              charDeviceIOCTL()
```

`fd`로 먼저 대상 드라이버가 결정되므로 다른 드라이버가 같은 ioctl
type과 명령 번호를 사용하더라도 콜백이 서로 섞이지는 않는다.

## ioctl 콜백

현재 드라이버의 콜백 형태는 다음과 같다.

```c
static long charDeviceIOCTL(struct file *device_file,
                            unsigned int command,
                            unsigned long argument);
```

| 매개변수 | 역할 |
| --- | --- |
| `device_file` | `ioctl()`을 호출한 열린 파일 인스턴스 |
| `command` | `_IO`, `_IOW`, `_IOR`, `_IOWR`로 만든 ioctl 명령 번호 |
| `argument` | 값 하나 또는 사용자 공간 데이터의 주소를 담는 인자 |

`argument`의 형은 `unsigned long`이지만 이 예제의 `_IOW`, `_IOR`,
`_IOWR` 명령에서는 사용자 공간의 `__u32` 변수를 가리키는
**포인터**로 해석한다. 이 포인터를 커널에서 직접 역참조하지 않고
`copy_from_user()` 또는 `copy_to_user()`를 사용한다.

## ioctl 명령 정의

[`char_ioctl.h`](../char_ioctl.h)에서 커널 드라이버와 사용자
프로그램이 공통으로 사용할 명령을 정의한다.

```c
#define CHAR_IOCTL_MAGIC 'C'

#define CHAR_IOCTL_CLEAR     _IO(CHAR_IOCTL_MAGIC, 0)
#define CHAR_IOCTL_SET_MODE  _IOW(CHAR_IOCTL_MAGIC, 1, __u32)
#define CHAR_IOCTL_GET_MODE  _IOR(CHAR_IOCTL_MAGIC, 2, __u32)
#define CHAR_IOCTL_SWAP_MODE _IOWR(CHAR_IOCTL_MAGIC, 3, __u32)
```

### 명령 인코딩 정보

`_IO*` 매크로는 다음 정보를 하나의 `unsigned int` 명령 값에
인코딩한다.

| 필드 | 의미 | 확인 매크로 |
| --- | --- | --- |
| type | 명령군을 구분하는 식별값. 이 예제에서는 `'C'` | `_IOC_TYPE(command)` |
| number | 같은 type 안에서 각 명령을 구분하는 번호 | `_IOC_NR(command)` |
| direction | 데이터가 이동하는 방향 | `_IOC_DIR(command)` |
| size | 전달하는 데이터 타입의 크기 | `_IOC_SIZE(command)` |

`MAGIC`은 커널의 특별한 키워드가 아니다. 명령군을 식별하는 고정
값을 관례적으로 magic number라고 부르기 때문에 사용한 이름이다.

### `_IO*` 매크로의 방향

방향의 기준은 커널이다.

| 매크로 | 의미 | 드라이버의 일반적인 처리 |
| --- | --- | --- |
| `_IO(type, nr)` | 전달 데이터 없음 | `argument`를 사용하지 않음 |
| `_IOW(type, nr, data_type)` | 사용자가 커널에 데이터를 쓰기 | `copy_from_user()` |
| `_IOR(type, nr, data_type)` | 사용자가 커널에서 데이터를 읽기 | `copy_to_user()` |
| `_IOWR(type, nr, data_type)` | 양방향으로 데이터 전달 | 두 복사 함수 모두 사용 |

세 번째 인자에는 포인터가 아니라 실제 데이터 타입을 적는다.

```c
/* 올바른 예: sizeof(__u32)가 인코딩됨 */
_IOW(CHAR_IOCTL_MAGIC, 1, __u32)

/* 피할 예: 데이터가 아닌 포인터 크기가 인코딩됨 */
_IOW(CHAR_IOCTL_MAGIC, 1, __u32 *)
```

## `CHAR_IOCTL_CLEAR`

`CHAR_IOCTL_CLEAR`는 별도의 인자 없이 장치 데이터를 논리적으로
비우고, 호출한 열린 파일의 offset을 초기화한다.

```c
#define CHAR_IOCTL_CLEAR _IO(CHAR_IOCTL_MAGIC, 0)
```

사용자 프로그램은 세 번째 인자를 전달하지 않는다.

```c
ioctl(fd, CHAR_IOCTL_CLEAR);
```

드라이버는 다음과 같이 처리한다.

```c
case CHAR_IOCTL_CLEAR:
    data_size = 0;
    device_file->f_pos = 0;
    return 0;
```

`data_size = 0`만으로 읽기 로직에서 데이터가 없는 상태가 되므로
`device_buffer`를 `memset()`으로 덮어쓰지 않아도 된다. 버퍼에 이전
바이트는 남아 있지만 유효한 데이터로 취급되지 않는다.

`device_buffer`와 `data_size`는 모든 open 인스턴스가 공유하는 전역
상태이다. 반면 `f_pos`는 `struct file`, 즉 열린 파일 인스턴스의
상태이므로 `ioctl()`을 호출한 인스턴스의 offset만 0으로 변경된다.
별도로 `open()`한 다른 파일의 `f_pos`는 변경되지 않는다.

## `CHAR_IOCTL_SET_MODE`

`CHAR_IOCTL_SET_MODE`는 사용자 공간의 `__u32` 값을 커널로 전달한다.

```c
#define CHAR_IOCTL_SET_MODE _IOW(CHAR_IOCTL_MAGIC, 1, __u32)
```

사용자 프로그램은 값 자체가 아니라 변수의 **주소**를 전달한다.

```c
__u32 set_mode = CHAR_IOCTL_MODE1;

ioctl(fd, CHAR_IOCTL_SET_MODE, &set_mode);
```

드라이버는 `argument`를 사용자 포인터로 해석하고 값을 복사한다.

```c
case CHAR_IOCTL_SET_MODE:
    if (copy_from_user(&ioctl_mode,
                       (const void __user *)argument,
                       sizeof(ioctl_mode)))
        return -EFAULT;

    return 0;
```

다음과 같이 값을 직접 전달하면 안 된다.

```c
/* 잘못된 호출 */
ioctl(fd, CHAR_IOCTL_SET_MODE, CHAR_IOCTL_MODE1);
```

`CHAR_IOCTL_MODE1`의 값이 `2`라면 드라이버는 이를 주소 `0x2`로
해석한다. 유효하지 않은 사용자 주소에서 복사를 시도하므로
`copy_from_user()`가 실패하고 `-EFAULT`가 반환된다.

## `CHAR_IOCTL_GET_MODE`

`CHAR_IOCTL_GET_MODE`는 커널의 현재 모드를 사용자 공간으로 복사한다.

```c
#define CHAR_IOCTL_GET_MODE _IOR(CHAR_IOCTL_MAGIC, 2, __u32)
```

사용자는 결과를 저장할 변수의 주소를 전달한다.

```c
__u32 device_mode = 0;

ioctl(fd, CHAR_IOCTL_GET_MODE, &device_mode);
```

드라이버는 `copy_to_user()`로 결과를 전달한다.

```c
case CHAR_IOCTL_GET_MODE:
    if (copy_to_user((void __user *)argument,
                     &ioctl_mode,
                     sizeof(ioctl_mode)))
        return -EFAULT;

    return 0;
```

## `CHAR_IOCTL_SWAP_MODE`

`CHAR_IOCTL_SWAP_MODE`는 하나의 `__u32` 버퍼를 입력과 출력에 모두
사용하는 양방향 명령이다.

```c
#define CHAR_IOCTL_SWAP_MODE _IOWR(CHAR_IOCTL_MAGIC, 3, __u32)
```

사용자가 새 모드를 넣은 변수의 주소를 전달한다.

```c
__u32 swap_mode = CHAR_IOCTL_MODE2;

ioctl(fd, CHAR_IOCTL_SWAP_MODE, &swap_mode);
```

드라이버는 새 모드를 읽고, 현재 모드를 보관한 뒤, 새 모드를
저장하고 이전 모드를 같은 사용자 버퍼에 복사한다.

```c
case CHAR_IOCTL_SWAP_MODE: {
    __u32 request_mode;
    __u32 previous_mode;

    if (copy_from_user(&request_mode,
                       (const void __user *)argument,
                       sizeof(request_mode)))
        return -EFAULT;

    previous_mode = ioctl_mode;
    ioctl_mode = request_mode;

    if (copy_to_user((void __user *)argument,
                     &previous_mode,
                     sizeof(previous_mode)))
        return -EFAULT;

    return 0;
}
```

예를 들어 장치 모드가 `MODE1`이고 `swap_mode`가 `MODE2`인 상태에서
호출하면 다음과 같이 바뀐다.

| 항목 | 호출 전 | 호출 후 |
| --- | --- | --- |
| 장치의 `ioctl_mode` | `MODE1` | `MODE2` |
| 사용자의 `swap_mode` | `MODE2` | `MODE1` |

## ioctl 모드 값

[`char_ioctl.h`](../char_ioctl.h)에서 모드를 비트 플래그로
정의한다.

```c
#define CHAR_IOCTL_MODE0 (0x01u << 0)
#define CHAR_IOCTL_MODE1 (0x01u << 1)
#define CHAR_IOCTL_MODE2 (0x01u << 2)
```

비트 상수에 `__u32` 캐스트를 추가할 필요는 없다. `u` 접미사를 붙인
unsigned 정수 상수를 사용하고, 실제로 값을 전달하는 ioctl 타입을
`__u32`로 고정하면 된다.

`__u32`는 커널과 사용자 공간 사이의 ABI에서 크기가 항상 32비트임을
나타낸다. 이런 공유 인터페이스에서는 플랫폼에 따라 크기가 달라질
수 있는 `long`, `unsigned long`, `size_t`, 포인터를 데이터 필드로
사용하지 않는 것이 좋다.

## 오류 처리

ioctl 콜백은 성공하면 `0`, 실패하면 음수 errno를 반환한다.
사용자 공간에서는 실패가 `-1`로 보이고 `errno`에 실제 오류 코드가
저장된다.

| 커널 반환값 | 의미 |
| --- | --- |
| `-EFAULT` | 사용자 공간 주소에서 데이터를 복사하거나 그 주소로 복사하지 못함 |
| `-EINVAL` | 인자 값이나 명령이 현재 구현에서 유효하지 않음 |
| `-ENOTTY` | 해당 장치가 인식하지 못하는 ioctl 명령 |

현재 코드는 알 수 없는 명령에 `-EINVAL`을 반환한다. ioctl의
인식할 수 없는 명령에는 관례적으로 `-ENOTTY`를 사용하므로
다음과 같이 변경해도 된다.

```c
default:
    return -ENOTTY;
```

## 32비트 호환성과 `compat_ioctl`

`unlocked_ioctl`은 일반적으로 커널과 같은 ABI를 사용하는 프로그램의
요청을 처리한다. 64비트 커널이 32비트 프로그램의 ioctl을 지원해야
할 때는 `compat_ioctl` 처리가 필요할 수 있다.

이 예제는 포인터나 `unsigned long`을 데이터 구조에 넣지 않고
고정 크기 타입인 `__u32`만 전달한다. 따라서 32비트와 64비트에서
데이터 크기가 달라지는 ABI 문제를 피하기 쉽다. 현재 64비트
테스트 프로그램 실습에서는 `.unlocked_ioctl`만 구현해도 충분하다.

## 빌드 및 테스트

모듈과 테스트 프로그램을 빌드한다.

```bash
make
```

기존 모듈이 적재되어 있다면 제거한 뒤 새 모듈을 적재한다.

```bash
sudo rmmod char_device
sudo insmod build/char_device.ko
```

ioctl 테스트 프로그램을 실행한다.

```bash
sudo ./build/char_ioctl_test
```

다른 터미널에서 커널 로그를 확인하면 각 명령이 드라이버 콜백으로
전달되는 과정과 모드 변경을 볼 수 있다.

```bash
sudo dmesg -w
```

테스트 프로그램은 다음 순서로 동작한다.

1. 장치에 데이터를 쓴 뒤 `CHAR_IOCTL_CLEAR`로 비워졌는지 확인한다.
2. `CHAR_IOCTL_SET_MODE`로 `MODE1`을 설정한다.
3. `CHAR_IOCTL_GET_MODE`로 현재 모드가 `MODE1`인지 확인한다.
4. `CHAR_IOCTL_SWAP_MODE`에 `MODE2`를 전달하고, 반환된 값이 이전 모드인 `MODE1`인지 확인한다.

## 핵심 정리

- ioctl은 장치 데이터 스트림이 아닌 장치 고유의 제어 명령을 제공한다.
- `_IO*` 매크로는 type, number, direction, data size를 명령 번호에 인코딩한다.
- `_IOW`, `_IOR`, `_IOWR`의 세 번째 인자는 포인터가 아닌 실제 데이터 타입이다.
- `argument`는 하나지만 구조체의 주소를 전달하면 여러 필드를 한 번에 주고받을 수 있다.
- 사용자 공간 포인터는 직접 역참조하지 않고 `copy_from_user()`와 `copy_to_user()`로 처리한다.
- 공유 ioctl ABI에서는 `__u32`같은 고정 크기 타입을 사용하는 것이 좋다.
