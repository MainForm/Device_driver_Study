#ifndef CHAR_IOCTL_H
#define CHAR_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define CHAR_IOCTL_MODE0    (0x01u << 0)
#define CHAR_IOCTL_MODE1    (0x01u << 1)
#define CHAR_IOCTL_MODE2    (0x01u << 2)

#define CHAR_IOCTL_MAGIC 'C'

// ioctl 명령어 정의
#define CHAR_IOCTL_CLEAR        _IO     (CHAR_IOCTL_MAGIC, 0)            // 인자을 사용하지 않는 명령어
#define CHAR_IOCTL_SET_MODE     _IOW    (CHAR_IOCTL_MAGIC, 1, __u32)    // 인자를 사용하는 쓰기 명령어
#define CHAR_IOCTL_GET_MODE     _IOR    (CHAR_IOCTL_MAGIC, 2, __u32)    // 인자를 사용하는 읽기 명령어
#define CHAR_IOCTL_SWAP_MODE    _IOWR   (CHAR_IOCTL_MAGIC, 3, __u32)   // 인자를 사용하는 읽기/쓰기 명령어

#endif // CHAR_IOCTL_H