#include <stdio.h> 
#include <unistd.h> // sleep(), close()
#include <fcntl.h>  // open(), O_RDONLY, O_RDWR

// 테스트할 문자 장치 파일의 경로
#define CHAR_DEV_PATH "/dev/char_test_device"

int main(void)
{
    // -----------------------------------------------------------------
    // 예제 1: 접근 모드를 바꾸어 장치를 각각 한 번씩 열기
    // 드라이버의 struct file에서 f_flags와 f_mode의 차이를 확인한다.
    // -----------------------------------------------------------------
    printf("example1 : char device open\n");

    // 읽기와 쓰기가 모두 가능한 모드로 장치를 연다.
    printf("Opening character devie with O_RDWR flag\n");
    int fd = open(CHAR_DEV_PATH, O_RDWR);

    // open()은 실패하면 -1을 반환하고 errno를 설정한다.
    if (fd == -1) {
        perror("open");
        return 1;
    }

    // fd는 현재 프로세스의 파일 디스크립터 테이블 인덱스이다.
    // 드라이버에는 fd가 아니라 이 fd가 가리키는 struct file이 전달된다.
    printf("device opened: fd=%d\n", fd);

    // 장치가 열린 상태를 유지하여 커널 로그를 확인할 시간을 확보한다.
    sleep(5);

    // close() 시 드라이버의 release 콜백이 호출된다.
    if (close(fd) == -1) {
        perror("close");
        return 1;
    }

    // 같은 장치를 읽기 전용 모드로 다시 연다.
    // 앞선 O_RDWR 호출과 f_flags 및 f_mode 값이 어떻게 다른지 확인한다.
    printf("Opening character devie with O_RDONLY flag\n");
    fd = open(CHAR_DEV_PATH, O_RDONLY);

    printf("device opened: fd=%d\n", fd);
    sleep(5);

    if (close(fd) == -1) {
        perror("close");
        return 1;
    }

    // -----------------------------------------------------------------
    // 예제 2: 같은 문자 장치를 동시에 두 번 열기
    // 각 open()은 서로 다른 fd와 struct file을 만들 수 있지만,
    // 두 struct file은 같은 장치 노드의 inode를 가리킨다.
    // -----------------------------------------------------------------
    printf("example2 : multiple char device open\n");

    // 첫 번째 열린 파일 인스턴스: 읽기/쓰기 모드
    int fd1 = open(CHAR_DEV_PATH, O_RDWR);
    if (fd1 == -1) {
        perror("open");
        return 1;
    }

    // 두 open 콜백의 로그를 시간상 구분하기 위한 대기
    sleep(1);

    // 두 번째 열린 파일 인스턴스: 읽기 전용 모드
    int fd2 = open(CHAR_DEV_PATH, O_RDONLY);
    if (fd2 == -1) {
        perror("open");
        return 1;
    }

    // 두 fd가 동시에 열린 상태를 유지한다.
    sleep(5);

    // 두 번째 fd부터 닫아 release 콜백 호출 순서를 확인한다.
    close(fd2);
    sleep(1);
    close(fd1);

    return 0;
}
