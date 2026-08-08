# `struct inode`

## 목적

`inode`는 파일 이름이 아니라 파일 시스템에 존재하는 객체의 메타데이터를
나타낸다. 문자 장치에서는 `/dev/char_test_device` 장치 노드의 종류, 장치 번호,
소유자 등의 정보를 담고 있다.

```c
static int charDeviceOpen(struct inode *device_inode,
                          struct file *device_file)
```

여기서 `device_inode`는 열린 장치 노드의 inode이고, `device_file`은 이번
`open()`으로 생성된 열린 파일 인스턴스이다. 같은 장치를 여러 번 열면 각
호출마다 `struct file`은 별도로 만들어질 수 있지만 inode는 같은 장치 노드를
가리킨다.

## 주요 멤버와 함수

### `i_ino`

파일 시스템 내부에서 inode를 식별하는 번호이다.

```c
pr_info("inode number : %lu\n", device_inode->i_ino);
```

inode 번호는 파일 시스템 내부 식별자이며 문자 장치의 Major/Minor 번호와는
다른 값이다.

### `imajor()`와 `iminor()`

문자 장치를 처리할 드라이버와 장치 인스턴스를 구분하는 번호를 얻는다.

```c
pr_info("major : %u\n", imajor(device_inode));
pr_info("minor : %u\n", iminor(device_inode));
```

- Major 번호: 장치에 연결된 드라이버를 식별한다.
- Minor 번호: 같은 드라이버가 관리하는 개별 장치를 구분한다.

현재 드라이버는 다음 코드에서 동적 Major 번호와 Minor 번호 `0`을 사용한다.

```c
major = register_chrdev(0, CHAR_DEV_NAME, &fops);
device_create(chr_test_class, NULL, MKDEV(major, 0),
              NULL, CHAR_DEV_NAME);
```

### `i_uid`와 `i_gid`

장치 노드의 소유자 UID와 그룹 GID를 나타낸다.

```c
pr_info("uid : %u\n", device_inode->i_uid.val);
pr_info("gid : %u\n", device_inode->i_gid.val);
```

이 값은 커널 내부의 `kuid_t`, `kgid_t` 타입이다. 일반 사용자에게 장치 노드의
읽기 또는 쓰기 권한이 없으면 사용자 공간의 `open()`이 실패하며 드라이버의
`.open` 콜백까지 도달하지 않는다. 실제 권한은 사용자 공간에서 다음과 같이
확인할 수 있다.

```bash
ls -l /dev/char_test_device
```

### `i_mode`

파일 종류와 접근 권한 비트를 담고 있다. `S_ISCHR()`를 사용하면 해당 inode가
문자 장치인지 확인할 수 있다.

```c
if (S_ISCHR(device_inode->i_mode))
    pr_info("This is a character device\n");
```

파일 종류를 검사할 때 `i_mode`의 비트를 직접 비교하기보다는 `S_ISCHR()` 같은
매크로를 사용하는 편이 명확하다.

## `inode`와 `file`의 차이

| 구조체 | 의미 | 대표 정보 |
| --- | --- | --- |
| `struct inode` | 파일 시스템의 장치 노드 자체 | inode 번호, Major/Minor, 소유자, 파일 종류 |
| `struct file` | 한 번의 `open()`으로 만들어진 열린 파일 | open 플래그, 읽기/쓰기 모드, 파일 위치, private data |

`struct file`에서 inode가 필요할 때는 멤버에 직접 접근하기보다
`file_inode()`를 사용할 수 있다.

```c
struct inode *inode = file_inode(device_file);

pr_info("same inode : %s\n",
        inode == device_inode ? "yes" : "no");
```

## 사용 예제

현재 문자 장치의 `.open` 콜백에서 다음과 같이 확인한다.

```c
static int charDeviceOpen(struct inode *device_inode,
                          struct file *device_file)
{
    pr_info("inode number : %lu\n", device_inode->i_ino);
    pr_info("major : %u\n", imajor(device_inode));
    pr_info("minor : %u\n", iminor(device_inode));
    pr_info("uid : %u\n", device_inode->i_uid.val);
    pr_info("gid : %u\n", device_inode->i_gid.val);

    if (S_ISCHR(device_inode->i_mode))
        pr_info("This is a character device\n");

    return 0;
}
```

모듈을 다시 적재한 후 테스트 프로그램으로 장치를 열고 커널 로그를 확인한다.

```bash
make
sudo rmmod char_device
sudo insmod build/char_device.ko
sudo ./build/char_test
sudo dmesg | tail -30
```

실시간으로 로그를 보려면 다음 명령을 사용한다.

```bash
sudo dmesg -w
```

## 관련 헤더

```c
#include <linux/fs.h>
```

`struct inode`, `struct file`, `file_inode()`, `imajor()`, `iminor()`와 문자 장치의
`struct file_operations`를 사용할 때 필요한 핵심 헤더이다.
