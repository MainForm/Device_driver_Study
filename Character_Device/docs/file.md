# `struct file`

## 목적

`struct file`은 디스크에 존재하는 파일 자체가 아니라 한 번의 `open()`으로
생성된 **열린 파일 인스턴스(open file description)** 를 나타내는 커널 구조체다.
사용자 애플리케이션이 지정한 open 플래그, 커널이 설정한 접근 모드, 연결된
inode 등의 정보를 가지고 있다.

```c
static int charDeviceOpen(struct inode *device_inode,
                          struct file *device_file)
```

여기서 `device_file`은 현재 `open()` 요청에 해당하는 `struct file`이다. 같은
장치 노드를 여러 번 열면 같은 inode를 가리키더라도 각각 별도의 `struct file`이
생성될 수 있다.

## 주요 멤버와 함수

### `f_flags`

사용자 애플리케이션이 `open()`에 전달한 플래그를 확인할 수 있다.

```c
int fd = open("/dev/char_test_device", O_RDWR);
```

드라이버에서는 다음과 같이 접근 모드를 판별한다.

```c
pr_info("f_flags : 0x%x\n", device_file->f_flags);

switch (device_file->f_flags & O_ACCMODE) {
case O_RDONLY:
    pr_info("open mode : O_RDONLY\n");
    break;
case O_WRONLY:
    pr_info("open mode : O_WRONLY\n");
    break;
case O_RDWR:
    pr_info("open mode : O_RDWR\n");
    break;
}
```

`O_ACCMODE`는 `f_flags`에서 읽기·쓰기 접근 모드에 해당하는 비트만 추출한다.
특히 `O_RDONLY`의 값은 `0`이므로 다음과 같이 단순 비트 검사하면 안 된다.

```c
/* 올바른 O_RDONLY 검사 방법이 아님 */
if (device_file->f_flags & O_RDONLY)
    pr_info("read only\n");
```

접근 모드는 반드시 `f_flags & O_ACCMODE`의 결과를 비교하는 방식으로 확인한다.
`O_NONBLOCK`, `O_APPEND` 같은 개별 상태 플래그는 비트 연산으로 검사할 수 있다.

### `f_mode`

커널이 현재 열린 파일에 허용한 동작을 나타낸다. `FMODE_READ`와
`FMODE_WRITE`를 사용하면 읽기·쓰기 가능 여부를 확인할 수 있다.

```c
pr_info("f_mode : 0x%x\n", device_file->f_mode);
pr_info("FMODE_READ : %s\n",
        device_file->f_mode & FMODE_READ ? "yes" : "no");
pr_info("FMODE_WRITE : %s\n",
        device_file->f_mode & FMODE_WRITE ? "yes" : "no");
```

`f_flags`는 사용자 공간에서 전달된 open 플래그를 중심으로 나타내고,
`f_mode`는 커널 내부에서 해당 열린 파일이 지원하는 동작을 나타낸다는 차이가
있다.

### `f_inode`와 `file_inode()`

`struct file`이 가리키는 inode를 얻을 수 있다. `f_inode` 멤버에 직접 접근하기보다
`file_inode()` 접근 함수를 사용하는 것이 권장된다.

```c
struct inode *inode = file_inode(device_file);

pr_info("f_inode number : %lu\n", inode->i_ino);
pr_info("inode arguments are same : %s\n",
        inode == device_inode ? "yes" : "no");
```

`open()` 콜백에 함께 전달된 `device_inode`와 `file_inode(device_file)`은 현재
열린 장치 노드의 같은 inode를 가리킨다.

## `file`과 파일 디스크립터의 관계

사용자 공간에서 `open()`이 반환하는 파일 디스크립터(fd)는 `struct file` 내부에
저장된 값이 아니다. fd는 프로세스별 파일 디스크립터 테이블의 인덱스이며,
그 테이블의 항목이 `struct file`을 가리킨다.

```text
사용자 공간의 fd
       │
       ▼
프로세스의 fd 테이블
       │
       ▼
  struct file
       │
       ▼
  struct inode
```

따라서 드라이버가 `struct file`만으로 애플리케이션의 fd 번호 하나를 알아내는
것은 일반적으로 불가능하다. `dup()`이나 `fork()`로 하나의 `struct file`을 여러
fd가 공유할 수도 있기 때문이다.

fd 번호는 사용자 애플리케이션에서 확인한다.

```c
int fd = open(CHAR_DEV_PATH, O_RDWR);

if (fd == -1) {
    perror("open");
    return 1;
}

printf("device opened: fd=%d\n", fd);
```

## `inode`와 `file`의 차이

| 구조체 | 의미 | 대표 정보 |
| --- | --- | --- |
| `struct inode` | 파일 시스템의 장치 노드 자체 | inode 번호, Major/Minor, 소유자, 파일 종류 |
| `struct file` | 한 번의 `open()`으로 만들어진 열린 파일 | open 플래그, 읽기/쓰기 모드, 연결된 inode |

같은 장치를 두 번 `open()`하면 일반적으로 두 개의 fd와 두 개의 `struct file`이
만들어지지만, 두 `struct file`은 같은 장치 노드의 inode를 가리킬 수 있다.

## 사용 예제

현재 문자 장치의 `.open` 콜백에서 다음과 같이 확인한다.

```c
static int charDeviceOpen(struct inode *device_inode,
                          struct file *device_file)
{
    pr_info("f_flags : 0x%x\n", device_file->f_flags);

    switch (device_file->f_flags & O_ACCMODE) {
    case O_RDONLY:
        pr_info("open mode : O_RDONLY\n");
        break;
    case O_WRONLY:
        pr_info("open mode : O_WRONLY\n");
        break;
    case O_RDWR:
        pr_info("open mode : O_RDWR\n");
        break;
    }

    pr_info("f_mode : 0x%x\n", device_file->f_mode);
    pr_info("FMODE_READ : %s\n",
            device_file->f_mode & FMODE_READ ? "yes" : "no");
    pr_info("FMODE_WRITE : %s\n",
            device_file->f_mode & FMODE_WRITE ? "yes" : "no");

    pr_info("f_inode number : %lu\n",
            file_inode(device_file)->i_ino);
    pr_info("inode arguments are same : %s\n",
            file_inode(device_file) == device_inode ? "yes" : "no");

    return 0;
}
```

`release()`에서도 닫히기 직전의 `struct file`에 접근할 수 있다.

```c
static int charDeviceRelease(struct inode *device_inode,
                             struct file *device_file)
{
    pr_info("release f_flags : 0x%x\n", device_file->f_flags);
    pr_info("release f_mode : 0x%x\n", device_file->f_mode);
    pr_info("release inode : %lu\n",
            file_inode(device_file)->i_ino);

    return 0;
}
```

모듈을 다시 적재한 후 테스트 프로그램으로 `O_RDWR`와 `O_RDONLY`의 로그 차이를
확인한다.

```bash
make
sudo rmmod char_device
sudo insmod build/char_device.ko
sudo ./build/char_test
sudo dmesg | tail -50
```

실시간으로 로그를 보려면 다음 명령을 사용한다.

```bash
sudo dmesg -w
```

## 관련 헤더

```c
#include <linux/fs.h>
```

`struct file`, `struct inode`, `file_inode()`, `FMODE_READ`, `FMODE_WRITE`와
`struct file_operations`를 사용할 때 필요한 핵심 헤더이다.
