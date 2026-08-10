#include <stdio.h> 
#include <unistd.h> // sleep(), close()
#include <fcntl.h>  // open(), O_RDONLY, O_RDWR
#include <string.h> // strlen()

#include <stddef.h>

// 테스트할 문자 장치 파일의 경로
#define CHAR_DEV_PATH "/dev/char_test_device"

int main(void)
{
    // -----------------------------------------------------------------
    // 예제 3: 문자 장치에 데이터를 쓰기
    // -----------------------------------------------------------------

    int fd = open(CHAR_DEV_PATH, O_WRONLY);

    if (fd == -1) {
        perror("open");
        return 1;
    }

    // 문자 장치에 "0123456789"를 write
    {
        // write()는 문자 장치내 정상적으로 write된 바이트 수를 반환한다.
        // write()는 실제로는 커널의 file_operations 구조체의 write 콜백 함수를 호출한다.
        const char number_data[] = "0123456789";
        printf("user space data address(number_data) : %p\n", number_data);
        ssize_t write_bytes = write(fd, number_data, strlen(number_data));
        
        // write()가 실패하면 -1을 반환
        if(write_bytes == -1){
            perror("write");
            close(fd);
            return 1;
        }

        const char lower_alpha_data[] = "ABCDEFGHIJ";
        printf("user space data address(lower_alpha_data) : %p\n", lower_alpha_data);
        write_bytes = write(fd, lower_alpha_data, strlen(lower_alpha_data));
        
        // write()가 실패하면 -1을 반환
        if(write_bytes == -1){
            perror("write");
            close(fd);
            return 1;
        }
    }

    close(fd);

    // -----------------------------------------------------------------
    // 예제 4: 문자 장치에 데이터를 읽기
    // -----------------------------------------------------------------

    fd = open(CHAR_DEV_PATH, O_RDONLY);

    // 문자 장치에서 4 bytes씩 읽기
    {
        ssize_t read_bytes = 0;
        char read_buffer[12] = {0,};

        for(;;){
            // read()는 읽은 바이트 수를 반환하고, 더 이상 읽을 데이터가 없으면 0을 반환한다.
            // 즉, device 에서 file_operations 구조체의 read 콜백 함수의 return value를 얻는다.
            printf("user space data address(read_buffer) : %p\n", read_buffer);
            read_bytes = read(fd, read_buffer, 4);
        
            if(read_bytes == -1){
                perror("read");
                close(fd);
                return 1;
            }

            if(read_bytes == 0){
                printf("End of file\n");
                break;
            }

            printf("read %zd bytes: %.*s\n", read_bytes, (int)read_bytes, read_buffer);
        }
    }

    close(fd);

    return 0;
}
