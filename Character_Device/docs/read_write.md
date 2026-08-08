# Character Device의 `write()`와 `read()`

## 목적

이 예제는 문자 장치 내부에 32바이트 정적 버퍼를 두고 사용자 공간과 커널
공간 사이에서 데이터를 전달하는 과정을 보여준다.

```c
#define CHAR_BUFFER_SIZE (32)

static char device_buffer[CHAR_BUFFER_SIZE];
static size_t data_size = 0;
```

- `device_buffer`: 실제 데이터를 저장하는 커널 공간의 공유 버퍼
- `data_size`: `device_buffer`에서 현재 유효한 데이터의 크기
- `offset`: 각 열린 파일이 다음 읽기 또는 쓰기를 시작할 위치

사용자 프로그램이 `write()` 또는 `read()`를 호출하면 VFS가 파일
디스크립터에 연결된 `struct file`을 찾고, `file_operations`에 등록된 드라이버
콜백을 호출한다.

```c
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = charDeviceOpen,
    .release = charDeviceRelease,
    .write = charDeviceWrite,
    .read = charDeviceRead,
};
```

## `charDeviceWrite()` 처리 과정

`charDeviceWrite()`는 사용자 공간에서 전달받은 데이터를 `device_buffer`에
저장한다.

```c
static ssize_t charDeviceWrite(struct file *device_file,
                               const char __user *buffer,
                               size_t count,
                               loff_t *offset);
```

각 매개변수의 역할은 다음과 같다.

| 매개변수 | 역할 |
| --- | --- |
| `device_file` | 현재 쓰기를 요청한 열린 파일 인스턴스. 이 예제에서는 사용하지 않는다. |
| `buffer` | 사용자가 쓸 데이터가 들어 있는 사용자 공간 버퍼 |
| `count` | 사용자가 쓰기를 요청한 바이트 수 |
| `offset` | 이번 쓰기를 시작할 파일 위치 |

### 1. offset과 요청 크기 검사

음수 offset은 배열의 유효한 위치가 아니므로 `-EINVAL`을 반환한다.

```c
if (*offset < 0)
    return -EINVAL;
```

사용자가 0바이트를 요청하면 버퍼를 변경하지 않고 `0`을 반환한다.

```c
if (count == 0)
    return 0;
```

offset이 버퍼의 끝에 도달했거나 범위를 넘어갔다면 더 이상 저장할 공간이
없으므로 `-ENOSPC`를 반환한다. 이 검사를 먼저 수행해야 크기 계산에서 unsigned
정수 언더플로가 발생하거나 버퍼 밖에 쓰는 것을 막을 수 있다.

```c
if ((size_t)*offset >= CHAR_BUFFER_SIZE)
    return -ENOSPC;
```

### 2. 실제 쓰기 크기 계산

사용자가 요청한 크기가 버퍼의 남은 공간보다 클 수 있다. 따라서 두 값 중 작은
값을 실제 쓰기 크기로 선택한다.

```c
write_size = min(count,
                 (size_t)CHAR_BUFFER_SIZE - (size_t)*offset);
```

예를 들어 현재 offset이 `20`이고 `count`가 `20`이면 버퍼에는 12바이트만
남아 있으므로 `write_size`는 `12`가 된다.

### 3. 사용자 데이터를 커널 버퍼로 복사

`buffer`는 `__user`로 표시된 사용자 공간 포인터이므로 커널 코드에서 직접
역참조하지 않는다. `copy_from_user()`를 사용해 현재 offset부터 데이터를
복사한다.

```c
if (copy_from_user(device_buffer + *offset,
                   buffer,
                   write_size))
    return -EFAULT;
```

데이터의 이동 방향은 다음과 같다.

```text
사용자 공간 buffer
        │ copy_from_user()
        ▼
커널 공간 device_buffer + offset
```

### 4. 유효 데이터 크기 갱신

현재 구현은 기존 데이터의 중간 위치를 덮어쓰는 경우도 고려한다. 따라서 기존
`data_size`와 이번 쓰기가 끝난 위치 중 큰 값을 새로운 데이터 크기로 선택한다.

```c
data_size = max(data_size, *offset + write_size);
```

예를 들어 `data_size`가 20일 때 offset 4부터 5바이트를 덮어쓰면 데이터 끝은
여전히 20이다. 반대로 offset 20부터 5바이트를 추가하면 `data_size`는 25가
된다.

### 5. offset 갱신 및 결과 반환

복사에 성공하면 다음 쓰기가 방금 저장한 데이터 뒤에서 시작하도록 offset을
증가시킨다.

```c
*offset += write_size;
return write_size;
```

콜백이 반환한 값은 사용자 프로그램의 `write()` 반환값으로 전달된다. 따라서
사용자는 요청한 크기 전체가 아니라 실제로 저장된 크기를 확인해야 한다.

## `charDeviceRead()` 처리 과정

`charDeviceRead()`는 `device_buffer`에 저장된 데이터를 사용자 공간 버퍼로
전달한다.

```c
static ssize_t charDeviceRead(struct file *device_file,
                              char __user *buffer,
                              size_t count,
                              loff_t *offset);
```

각 매개변수의 역할은 다음과 같다.

| 매개변수 | 역할 |
| --- | --- |
| `device_file` | 현재 읽기를 요청한 열린 파일 인스턴스. 이 예제에서는 사용하지 않는다. |
| `buffer` | 읽은 데이터를 저장할 사용자 공간 버퍼 |
| `count` | 사용자가 읽기를 요청한 최대 바이트 수 |
| `offset` | 이번 읽기를 시작할 파일 위치 |

### 1. offset 검사

음수 offset은 유효한 버퍼 위치가 아니므로 `-EINVAL`을 반환한다.

```c
if (*offset < 0)
    return -EINVAL;
```

현재 offset이 `data_size`와 같거나 크면 더 읽을 데이터가 없다. 이때 오류가
아닌 `0`을 반환하며, 사용자 프로그램은 이를 EOF로 해석한다.

```c
if ((size_t)*offset >= data_size)
    return 0;
```

### 2. 실제 읽기 크기 계산

요청 크기와 현재 위치부터 남아 있는 데이터 크기 중 작은 값을 선택한다.

```c
read_size = min(count, data_size - (size_t)*offset);
```

예를 들어 20바이트가 저장되어 있고 현재 offset이 16이라면, 사용자가 10바이트를
요청해도 남은 4바이트만 읽는다.

### 3. 커널 데이터를 사용자 버퍼로 복사

사용자 공간 포인터에 직접 데이터를 쓰지 않고 `copy_to_user()`를 사용한다.

```c
if (copy_to_user(buffer,
                 device_buffer + *offset,
                 read_size))
    return -EFAULT;
```

데이터의 이동 방향은 다음과 같다.

```text
커널 공간 device_buffer + offset
        │ copy_to_user()
        ▼
사용자 공간 buffer
```

### 4. offset 갱신 및 결과 반환

복사에 성공하면 다음 읽기가 이어서 진행되도록 offset을 증가시키고 실제 읽은
바이트 수를 반환한다.

```c
*offset += read_size;
return read_size;
```

예를 들어 10바이트의 데이터를 4바이트씩 읽으면 다음과 같이 동작한다.

| 호출 | 읽기 전 offset | 반환한 데이터 크기 | 읽기 후 offset |
| ---: | ---: | ---: | ---: |
| 첫 번째 `read()` | 0 | 4 | 4 |
| 두 번째 `read()` | 4 | 4 | 8 |
| 세 번째 `read()` | 8 | 2 | 10 |
| 네 번째 `read()` | 10 | 0 (EOF) | 10 |

## 중요한 함수와 요소

### `copy_from_user()`

사용자 공간의 데이터를 커널 공간으로 복사한다.

```c
unsigned long copy_from_user(void *to,
                             const void __user *from,
                             unsigned long n);
```

- `to`: 데이터를 저장할 커널 공간 주소
- `from`: 데이터를 가져올 사용자 공간 주소
- `n`: 복사할 바이트 수

반환값은 성공적으로 복사한 크기가 아니라 **복사하지 못한 바이트 수**이다.
따라서 `0`이면 전체 복사 성공이고, 0이 아니면 일부 또는 전체 복사 실패이다.

### `copy_to_user()`

커널 공간의 데이터를 사용자 공간으로 복사한다.

```c
unsigned long copy_to_user(void __user *to,
                           const void *from,
                           unsigned long n);
```

- `to`: 데이터를 저장할 사용자 공간 주소
- `from`: 데이터를 가져올 커널 공간 주소
- `n`: 복사할 바이트 수

`copy_from_user()`와 마찬가지로 반환값은 복사하지 못한 바이트 수이다. 현재
예제는 반환값이 0이 아니면 `-EFAULT`를 반환한다.

두 함수는 상황에 따라 sleep할 수 있으므로 spinlock을 획득한 상태나 인터럽트가
비활성화된 상태에서 호출하면 안 된다.

### `min()`

두 값 중 작은 값을 반환한다. 현재 예제에서는 사용자 요청이 장치 버퍼 범위를
넘지 않도록 실제 전송 크기를 제한하는 데 사용한다.

```c
write_size = min(count, remaining_buffer_size);
read_size = min(count, remaining_data_size);
```

비교하는 두 값의 타입이 일치해야 하므로 offset과 버퍼 크기 계산 결과를
`size_t`로 맞춘다.

### `max()`

두 값 중 큰 값을 반환한다. 기존 데이터의 중간을 덮어쓸 때 `data_size`가
잘못 줄어들지 않도록 사용한다.

```c
data_size = max(data_size, end_of_write);
```

### `__user`

포인터가 사용자 공간 주소임을 나타내는 커널 어노테이션이다. 사용자 공간
주소는 커널 포인터처럼 직접 역참조하지 않고 `copy_from_user()` 또는
`copy_to_user()` 같은 사용자 메모리 접근 함수를 통해 다룬다.

### `offset`

`offset`은 현재 열린 파일의 읽기·쓰기 위치를 가리킨다. 성공한 작업의 크기만큼
직접 증가시켜야 다음 호출이 이어지는 위치에서 시작된다.

각각 별도로 `open()`한 파일은 서로 다른 `struct file`과 offset을 가질 수 있다.
그러나 이 예제의 `device_buffer`와 `data_size`는 전역 정적 변수이므로 모든 열린
파일이 같은 장치 데이터를 공유한다.

### 주요 오류 코드와 반환값

| 반환값 | 의미 |
| ---: | --- |
| 양수 | 실제로 읽거나 쓴 바이트 수 |
| `0` | `read()`에서는 EOF, 0바이트 `write()`에서는 변경 없음 |
| `-EINVAL` | offset이 음수여서 유효하지 않음 |
| `-ENOSPC` | 쓰기 offset이 버퍼 끝에 도달하여 저장 공간이 없음 |
| `-EFAULT` | 사용자 공간과 커널 공간 사이의 데이터 복사 실패 |

## 현재 예제의 데이터 공유 방식

`device_buffer`가 전역 정적 배열이므로 여러 번 `open()`해도 실제 데이터는
공유된다. 반면 offset은 열린 파일마다 관리되므로 새로 `open()`하면 일반적으로
0부터 읽거나 쓴다.

```text
open A의 offset ─┐
                 ├──> 하나의 device_buffer와 data_size
open B의 offset ─┘
```

현재 코드는 read와 write의 흐름을 학습하기 위한 예제이므로 동시 접근 제어를
구현하지 않았다. 여러 프로세스가 동시에 버퍼에 접근하는 환경으로 확장할 때는
mutex 등을 사용해 `device_buffer`와 `data_size`를 보호해야 한다.
