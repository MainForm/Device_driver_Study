#include <stdio.h> 
#include <unistd.h> // sleep(), close()
#include <fcntl.h>  // open(), O_RDONLY, O_RDWR

#include <sys/ioctl.h> // ioctl()

#include "char_ioctl.h"

// 테스트할 문자 장치 파일의 경로
#define CHAR_DEV_PATH "/dev/char_test_device"

int main(void)
{
    // 읽기와 쓰기가 모두 가능한 모드로 장치를 연다.
    printf("Opening character devie with O_RDWR flag\n");
    int fd = open(CHAR_DEV_PATH, O_RDWR);

    // -----------------------------------------------------------------
    // 예제 1:  IOCTL의 CHAR_IOCTL_CLEAR 명령어 테스트
    // -----------------------------------------------------------------

    printf("----example 1----\n");

    if(write(fd, "0123456789", 10) == -1){
        perror("write");
        close(fd);
        return 1;
    }

    if(ioctl(fd, CHAR_IOCTL_CLEAR) == -1){
        perror("ioctl");
        close(fd);
        return 1;
    }

    char read_buffer[12] = {0,};
    ssize_t read_bytes = read(fd, read_buffer, 10);

    if(read_bytes == -1){
        perror("read");
        close(fd);
        return 1;
    }

    if(read_bytes == 0){
        printf("CHAR_IOCTL_CLEAR test success\n");
    }

    // -----------------------------------------------------------------
    // 예제 2:  IOCTL로 문자장치의 모드 쓰고 읽기
    // -----------------------------------------------------------------
    printf("----example 2----\n");

    __u32 set_mode = CHAR_IOCTL_MODE1;

    printf("set device mode to CHAR_IOCTL_MODE1\n");
    if(ioctl(fd, CHAR_IOCTL_SET_MODE, &set_mode) == -1){
        perror("ioctl");
        close(fd);
        return 1;
    }

    __u32 device_mode = 0;

    if(ioctl(fd, CHAR_IOCTL_GET_MODE, &device_mode) == -1){
        perror("ioctl");
        close(fd);
        return 1;
    }

    printf("current device mode : 0x%02x\n",device_mode);

    // -----------------------------------------------------------------
    // 예제 3:  IOCTL로 문자장치의 모드 교환
    // -----------------------------------------------------------------
    printf("----example 3----\n");
    __u32 swap_mode = CHAR_IOCTL_MODE2;

    printf("befre swapping mode : 0x%02x\n", swap_mode);

    if(ioctl(fd, CHAR_IOCTL_SWAP_MODE, &swap_mode) == -1){
        perror("ioctl");
        close(fd);
        return 1;
    }

    printf("after swapping mode : 0x%02x\n", swap_mode);

    if (close(fd) == -1) {
        perror("close");
        return 1;
    }

    return 0;
}
