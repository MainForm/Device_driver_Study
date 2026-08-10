# Character Device

## 목적

이 예제는 Linux 커널 모듈에서 문자 장치(character device)를 등록하고 해제하는 기본 과정을 학습하기 위한 코드이다. `open()`, `read()`, `write()`, `ioctl()`을 사용해 사용자 공간의 파일 연산이 드라이버의 `file_operations` 콜백으로 전달되는 과정을 확인한다. 문자 장치를 포함한 장치 파일은 Major 번호로 드라이버를 식별하고 Minor 번호로 해당 드라이버가 관리하는 장치를 구분한다.[^device-numbers]

## Character Device란?

Character Device는 데이터를 고정 크기의 블록이 아닌 **바이트 단위의 흐름**으로 다루는 장치이다. 터미널, 시리얼 포트, 키보드와 같은 장치가 대표적인 예이며, 일반적으로 `/dev` 아래의 장치 파일을 통해 사용자 공간에 노출된다.

사용자 프로그램이 장치 파일에 파일 연산을 요청하면 VFS(Virtual File System)는 드라이버가 `struct file_operations`에 등록한 콜백으로 요청을 전달한다.[^vfs] 따라서 사용자 공간에서는 일반 파일과 비슷한 인터페이스로 장치에 접근할 수 있지만, 실제 동작은 드라이버와 하드웨어의 특성에 따라 결정된다.

| 구분 | Character Device | Block Device |
| --- | --- | --- |
| 데이터 처리 단위 | 바이트 또는 데이터 스트림 | 고정 크기 블록 또는 섹터 |
| 일반적인 접근 방식 | 순차 접근 | 임의 위치 접근 |
| 커널 I/O 계층 | 요청을 장치 드라이버의 파일 연산으로 전달 | Block I/O 계층에서 요청을 큐잉하고 처리 |
| 대표적인 예 | 터미널, 시리얼 포트, 키보드 | HDD, SSD, eMMC |

### 버퍼와 캐시

Character Device가 **항상 버퍼 없이 데이터를 즉시 교환하는 것은 아니다**. 블록 장치처럼 Block I/O 계층과 블록 단위 캐시를 기본 전제로 하지 않는다는 의미에 가깝다. 필요하다면 문자 장치 드라이버나 하드웨어가 자체 버퍼, FIFO 또는 링 버퍼를 사용할 수 있다. 실제 버퍼링 방식은 장치와 드라이버 구현에 따라 달라진다.[^char-buffer]

### 순차 접근과 랜덤 접근

Character Device는 터미널이나 시리얼 포트처럼 순차적으로 데이터가 흐르는 장치에 주로 사용되므로 일반적으로 임의 위치 접근을 지원하지 않는다. 그러나 이것은 절대적인 제한이 아니다. 장치에 위치 개념이 있고 드라이버가 `file_operations`의 `llseek` 동작을 제공한다면 위치 이동을 지원할 수 있다.[^vfs] 반대로 위치 개념이 없는 장치는 현재 위치를 옮기는 동작에 의미가 없다.

모듈을 적재하면 커널로부터 Major 번호를 동적으로 할당받고, 장치 클래스와 장치 파일을 생성한다. Linux Driver Core에서는 장치를 `struct device`로 표현하며, 클래스와 장치 등록 정보를 sysfs에 노출한다.[^driver-core]

- 장치 이름: `char_test_device`
- 장치 클래스: `/sys/class/char_test_class`
- 장치 파일: `/dev/char_test_device`
- Minor 번호: `0`

### 여러 번 열기

이 Character Device는 같은 장치 파일을 여러 번 `open()`할 수 있다. 드라이버의 `open` 콜백에는 이미 열린 파일이 있는지 검사하거나 추가 열기를 거부하는 로직이 없으므로, 각 `open()` 호출은 독립된 파일 디스크립터와 `struct file`을 만든다. 각 `struct file`은 서로 다른 접근 모드와 상태를 가질 수 있지만, 모두 같은 장치 파일의 inode를 가리킨다.

## 동작 과정

모듈을 적재할 때 `initModule()`이 다음 순서로 실행된다.

1. `register_chrdev()`로 문자 장치를 등록한다.
2. `class_create()`로 `/sys/class/char_test_class` 클래스를 생성한다.
3. `device_create()`로 `/dev/char_device` 장치 파일을 생성한다.

모듈 초기화 도중 오류가 발생하면 이미 생성된 자원을 역순으로 해제한다.

모듈을 제거할 때는 `exitModule()`이 다음 순서로 자원을 정리한다.

1. `device_destroy()`로 장치를 제거한다.
2. `class_destroy()`로 장치 클래스를 제거한다.
3. `unregister_chrdev()`로 문자 장치 등록을 해제한다.

## 주요 함수와 매크로

### `register_chrdev()`

문자 장치를 커널에 등록한다. Major 번호로 `0`을 전달하면 커널이 사용 가능한 번호를 동적으로 할당하며, 성공하면 할당된 Major 번호를 반환한다.[^char-source]

### `unregister_chrdev()`

등록된 문자 장치를 커널에서 해제한다. 모듈 초기화 실패 시와 모듈 제거 시 호출된다.

### `class_create()`

장치 클래스를 생성한다. 이 예제에서는 `CHAR_DEV_CLASS_NAME`에 정의된 이름을 사용하여 `/sys/class/char_test_class`가 생성된다. 생성된 클래스는 이후 `device_create()`에 전달한다.[^driver-core]

### `class_destroy()`

`class_create()`로 생성한 장치 클래스를 제거한다.

### `device_create()`

Major 번호와 Minor 번호를 조합한 장치 번호를 사용하여 장치를 생성한다. 이 함수는 장치를 해당 클래스에 등록하고 sysfs에 장치 정보를 만든다.[^driver-core] 사용자 공간에서는 `/dev/char_device`를 통해 장치에 접근할 수 있다.

### `device_destroy()`

`device_create()`로 생성한 장치를 제거한다.

### `MKDEV()`

Major 번호와 Minor 번호를 하나의 장치 번호인 `dev_t` 값으로 조합한다.

### `IS_ERR()`와 `PTR_ERR()`

`class_create()`와 `device_create()`가 반환한 포인터의 오류 여부를 확인하고, 오류 포인터에서 실제 오류 코드를 가져온다.

### `module_init()`와 `module_exit()`

모듈을 적재할 때 실행할 초기화 함수와 모듈을 제거할 때 실행할 종료 함수를 커널에 등록한다.

## Example

[`char_open_test.c`](./char_open_test.c)는 `/dev/char_test_device`를 여러 방식으로 열어 드라이버의 `open` 및 `release` 콜백 동작을 확인하는 사용자 공간 테스트 프로그램이다.

### 예제 1: 접근 모드 비교

장치를 먼저 `O_RDWR`로 열고 닫은 뒤 다시 `O_RDONLY`로 연다. 이 예제를 통해 사용자 공간의 `open()`에 전달한 접근 모드가 드라이버에서 다음과 같이 표현되는 것을 확인할 수 있다.

- `struct file`의 `f_flags`: `open()`에 전달된 플래그
- `struct file`의 `f_mode`: 커널이 관리하는 읽기 및 쓰기 가능 여부

각 호출 사이에 대기 시간이 있으므로 `dmesg`에서 두 접근 모드에 따른 커널 로그를 구분해 볼 수 있다.

### 예제 2: 같은 장치를 동시에 두 번 열기

같은 장치를 `O_RDWR`와 `O_RDONLY`로 각각 열고 두 파일 디스크립터를 동시에 유지한다. 이 예제의 목적은 Character Device를 여러 번 열 수 있으며, 각 `open()`이 서로 다른 파일 디스크립터와 `struct file`을 생성하는 것을 확인하는 것이다. 두 열린 파일은 독립된 접근 모드를 가지지만 같은 장치 노드의 inode를 가리킨다. 또한 두 번째 파일 디스크립터부터 닫으면서 드라이버의 `release` 콜백이 각 열린 파일마다 호출되는 것도 확인할 수 있다.

테스트 프로그램은 `make`를 실행할 때 모듈과 함께 `build/char_open_test`로 빌드된다. 모듈을 적재한 후 다음과 같이 실행하고, 다른 터미널에서 커널 로그를 확인한다.

```bash
sudo ./build/char_open_test
sudo dmesg -w
```

### 예제 3: 문자 장치에 데이터 쓰기

[`char_read_write_test.c`](./char_read_write_test.c)는 `/dev/char_test_device`를 `O_WRONLY`로 열고 `write()`를 두 번 호출한다.

```c
write(fd, "0123456789", 10);
write(fd, "ABCDEFGHIJ", 10);
```

첫 번째 `write()`는 장치 버퍼의 offset `0`부터 10바이트를 저장하고, 성공하면 offset이 `10`으로 이동한다. 두 번째 `write()`는 변경된 offset부터 이어서 10바이트를 저장한다. 따라서 두 호출이 모두 성공하면 32바이트 장치 버퍼에는 총 20바이트의 데이터가 다음과 같이 저장된다.

```text
0123456789ABCDEFGHIJ
```

이 예제를 통해 다음 동작을 확인할 수 있다.

- 사용자 공간의 `write()`가 드라이버의 `charDeviceWrite()` 콜백을 호출하는 과정
- `copy_from_user()`를 사용해 사용자 공간의 데이터를 커널 버퍼로 복사하는 과정
- 요청한 크기와 장치 버퍼의 남은 공간을 비교하여 실제 쓰기 크기를 정하는 방법
- 성공적으로 쓴 바이트 수만큼 offset과 `data_size`가 증가하는 동작
- 콜백이 반환한 실제 쓰기 크기가 사용자 공간 `write()`의 반환값으로 전달되는 과정

테스트 프로그램은 `make`를 실행할 때 `build/char_read_write_test`로 빌드된다. 모듈을 적재한 후 다음과 같이 실행하고 커널 로그에서 쓰기 크기, offset 및 저장된 데이터를 확인한다.

```bash
sudo ./build/char_read_write_test
sudo dmesg -w
```

`charDeviceWrite()`와 `charDeviceRead()`의 자세한 처리 과정은 [`docs/read_write.md`](./docs/read_write.md)에서 확인할 수 있다.

### 예제 4: `ioctl()`로 장치 제어

[`char_ioctl_test.c`](./char_ioctl_test.c)는 `/dev/char_test_device`를 `O_RDWR`로 열고 `ioctl()` 명령으로 장치 버퍼와 모드를 제어한다. ioctl 명령은 드라이버와 사용자 프로그램이 공통으로 포함하는 [`char_ioctl.h`](./char_ioctl.h)에 정의되어 있다.

| 명령 | 정의 | 데이터 방향 | 동작 |
| --- | --- | --- | --- |
| `CHAR_IOCTL_CLEAR` | `_IO` | 없음 | `data_size`와 호출한 열린 파일의 offset을 0으로 설정 |
| `CHAR_IOCTL_SET_MODE` | `_IOW` | 사용자 → 커널 | 사용자가 전달한 모드를 장치에 저장 |
| `CHAR_IOCTL_GET_MODE` | `_IOR` | 커널 → 사용자 | 현재 장치 모드를 사용자에게 전달 |
| `CHAR_IOCTL_SWAP_MODE` | `_IOWR` | 사용자 ↔ 커널 | 새 모드를 저장하고 이전 모드를 반환 |

`SET_MODE`, `GET_MODE`, `SWAP_MODE`는 고정 크기 타입인 `__u32`를 사용한다. 사용자 프로그램은 값을 담은 변수의 주소를 `ioctl()`에 전달하고, 드라이버는 `copy_from_user()`와 `copy_to_user()`로 커널 공간과 사용자 공간 사이의 데이터를 복사한다.

```c
__u32 mode = CHAR_IOCTL_MODE1;

ioctl(fd, CHAR_IOCTL_SET_MODE, &mode);
ioctl(fd, CHAR_IOCTL_GET_MODE, &mode);
```

`CHAR_IOCTL_CLEAR`는 장치가 공유하는 전역 `data_size`를 0으로 설정하면서 `ioctl()`을 호출한 `struct file`의 `f_pos`만 0으로 설정한다. 따라서 별도로 `open()`한 다른 열린 파일의 offset은 변경되지 않는다.

테스트 프로그램은 `make`를 실행할 때 `build/char_ioctl_test`로 빌드된다. 모듈을 적재한 뒤 다음과 같이 실행한다.

```bash
sudo ./build/char_ioctl_test
sudo dmesg -w
```

ioctl 명령 인코딩, `unlocked_ioctl` 콜백, 데이터 전달 방향과 각 명령의 자세한 처리 과정은 [`docs/ioctl.md`](./docs/ioctl.md)에서 확인할 수 있다.

## 빌드 및 실행

```bash
make
sudo insmod build/char_device.ko
```

커널 로그와 생성된 장치를 확인한다.

```bash
sudo dmesg | tail
ls -l /dev/char_test_device
ls -l /sys/class/char_test_class
```

모듈을 제거하고 빌드 결과를 정리한다.

```bash
sudo rmmod char_device
make clean
```

## Linux 공식 문서

- [Linux Device Drivers Infrastructure](https://docs.kernel.org/driver-api/infrastructure.html): `struct device`, `class_create()`, `device_create()`와 Driver Core의 장치 관리 API
- [Overview of the Linux Virtual File System](https://docs.kernel.org/filesystems/vfs.html): 장치 파일을 사용자 공간의 파일 연산과 연결하는 VFS 및 `struct file_operations`
- [Multi-Queue Block IO Queueing Mechanism](https://docs.kernel.org/block/blk-mq.html): 블록 장치 요청이 Block I/O 계층에서 큐잉되고 처리되는 구조
- [Industrial I/O Device Buffers](https://docs.kernel.org/iio/iio_devbuf.html): 문자 장치에서도 드라이버 특성에 따라 버퍼를 사용할 수 있는 실제 사례
- [Linux Allocated Devices](https://docs.kernel.org/admin-guide/devices.html): 문자·블록 장치의 Major 번호와 Minor 번호 할당 체계
- [Building External Modules](https://docs.kernel.org/kbuild/modules.html): 외부 커널 모듈의 Kbuild 파일 작성법과 빌드 방법
- [Linux Kernel Source: `fs/char_dev.c`](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/fs/char_dev.c): `register_chrdev()`와 문자 장치 등록 코드의 실제 구현

[^device-numbers]: Linux Kernel 공식 문서의 [Linux Allocated Devices](https://docs.kernel.org/admin-guide/devices.html) 참고.

[^vfs]: Linux Kernel 공식 문서의 [Overview of the Linux Virtual File System](https://docs.kernel.org/filesystems/vfs.html) 중 `struct file_operations` 설명 참고.

[^char-buffer]: Linux Kernel 공식 문서의 [Industrial I/O Device Buffers](https://docs.kernel.org/iio/iio_devbuf.html)는 문자 장치가 자체 버퍼를 사용하는 실제 사례를 설명한다.

[^driver-core]: Linux Kernel 공식 문서의 [Device Drivers Infrastructure](https://docs.kernel.org/driver-api/infrastructure.html) 참고.

[^char-source]: Linux Kernel 공식 소스의 [`fs/char_dev.c`](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/fs/char_dev.c) 참고.
