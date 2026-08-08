#define pr_fmt(fmt) "[" KBUILD_MODNAME "][%s]: " fmt, __func__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>

#include <linux/device.h>
#include <linux/err.h>
 
// 문자 장치의 이름
#define CHAR_DEV_NAME               ("char_test_device")
#define CHAR_DEV_CLASS_NAME         ("char_test_class")

// 생성한 문자 장치의 Major 번호
static int major;

static struct class * chr_test_class;
static struct device * chr_test_device;

// 실습을 위해서 적당히 작은 사이즈로 설정
#define CHAR_BUFFER_SIZE            (32)

// 정적 메모리 사용
static char device_buffer[CHAR_BUFFER_SIZE];
static size_t data_size = 0;

// ------------------------------------------------------------
// 문자 장치 관련 콜백 함수
// ------------------------------------------------------------

// 문자 장치가 open() 호출 시 수행되는 함수
static int charDeviceOpen(struct inode *device_inode, struct file *device_file)
{
    pr_info("device opened\n");

    printk("----inode example----\n");

    // inode 번호 출력
    pr_info("inode number : %lu\n", device_inode->i_ino);

    // inode로 문자 장치의 major, minor 번호를 확인할 수 있음
    pr_info("major : %d\n", imajor(device_inode));
    pr_info("minor : %d\n", iminor(device_inode));

    // inode로 해당 파일의 권한 확인
    pr_info("uid : %u\n", device_inode->i_uid.val);
    pr_info("gid : %u\n", device_inode->i_gid.val);

    // inode를 통해 해당 파일이 어떤 종류인지 확인
    if(S_ISCHR(device_inode->i_mode)){
        pr_info("This is a character device\n");
    }

    printk("----file example----\n");

    // f_flags: 사용자 공간에서 open()에 전달한 플래그를 확인
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

    // f_mode: 커널이 관리하는 현재 파일의 읽기/쓰기 가능 여부를 확인
    pr_info("f_mode : 0x%x\n", device_file->f_mode);
    pr_info("FMODE_READ : %s\n",
            device_file->f_mode & FMODE_READ ? "yes" : "no");
    pr_info("FMODE_WRITE : %s\n",
            device_file->f_mode & FMODE_WRITE ? "yes" : "no");

    // f_inode: 이 열린 파일이 가리키는 inode를 확인
    // 직접 f_inode에 접근하기보다 file_inode() 사용을 권장
    pr_info("f_inode number : %lu\n", file_inode(device_file)->i_ino);
    pr_info("inode arguments are same : %s\n",
            file_inode(device_file) == device_inode ? "yes" : "no");

    return 0;
}

// 문자 장치가 close() 호출 시 수행되는 함수
static int charDeviceRelease(struct inode *device_inode, struct file *device_file)
{
    pr_info("device closed\n");
    return 0;
}

/*
 * 사용자 공간의 데이터를 문자 장치 내부 버퍼에 저장한다.
 *
 * device_file: 현재 write()를 요청한 열린 파일 인스턴스 (사용 안함)
 * buffer:      사용자가 write를 요청한 데이터가 들어 있는 사용자 공간 버퍼
 * count:       사용자가 쓰기를 요청한 바이트 수
 * offset:      현재 파일 데이터 위치. 저장에 성공한 바이트 수만큼 증가시킨다.
 *
 * 반환 값
 * - return value > 0 : 실제로 저장한 바이트 수
 * - return value = 0 : 저장할 데이터가 없는 경우
 * - return value < 0 : 오류 발생
 */
static ssize_t charDeviceWrite(struct file *device_file, const char __user *buffer, size_t count, loff_t *offset){
    size_t write_size = 0;

    // 현재 구현에서는 struct file의 정보를 사용하지 않는다.
    (void)device_file;

    printk("----device write----\n");

    // 사용자 요청 크기와 쓰기를 시작할 현재 파일 위치를 확인한다.
    pr_info("count : %zu\n", count);
    pr_info("offset : %lld\n", (long long)*offset);

    // 음수 위치는 device_buffer의 유효한 인덱스가 될 수 없다.
    if(*offset < 0){
        pr_err("offset is negative\n");
        return -EINVAL;
    }

    // 0바이트 쓰기는 아무 데이터도 변경하지 않고 성공으로 처리한다.
    if(count == 0){
        pr_info("count is zero\n");
        return 0;
    }

    // 현재 위치가 버퍼 끝에 도달했거나 넘어갔다면 저장 공간이 없다.
    if((size_t)*offset >= CHAR_BUFFER_SIZE){
        pr_info("offset is over buffer size(offset : %lld, buffer size : %d)\n", (long long)*offset, CHAR_BUFFER_SIZE);
        return -ENOSPC;
    }

    // 사용자 요청 크기와 버퍼의 남은 공간 중 작은 값을 실제 쓰기 크기로 정한다.
    write_size = min(count, (size_t)CHAR_BUFFER_SIZE - (size_t)*offset);

    // 사용자 공간의 데이터를 현재 offset부터 커널 버퍼로 복사한다.
    // copy_from_user()가 0이 아닌 값을 반환하면 일부 바이트를 복사하지 못한 것이다.
    if(copy_from_user(device_buffer + *offset, buffer, write_size)){
        pr_err("copy_from_user() failed\n");
        return -EFAULT;
    }

    // 중간 위치를 덮어쓸 수도 있으므로 기존 크기와 쓰기가 끝난 위치 중 큰 값을 유지한다.
    data_size = max(data_size, *offset + write_size);

    // 지금까지 저장된 data size 출력
    pr_info("current data_size : %zu\n", data_size);
    pr_info("current data : %.*s\n", (int)data_size, device_buffer);
    // 이번에 저장된 데이터 출력
    pr_info("written data : %.*s\n", (int)write_size, device_buffer + *offset);

    // 다음 쓰기가 이번 데이터 뒤에서 시작하도록 파일 위치를 이동한다.
    *offset += write_size;

    // 사용자 공간에는 실제로 저장한 바이트 수가 write()의 반환값으로 전달된다.
    return write_size;
}

/*
 * 문자 장치 내부 버퍼의 데이터를 사용자 공간으로 전달한다.
 *
 * device_file: 현재 read()를 요청한 열린 파일 인스턴스(사용 안함)
 * buffer:      읽은 데이터를 저장할 사용자 공간 버퍼
 * count:       사용자가 읽기를 요청한 최대 바이트 수
 * offset:      현재 파일 데이터 위치. 읽기에 성공한 바이트 수만큼 증가시킨다.
 *
 * 반환 값
 * - return value > 0 : 실제로 읽은 바이트 수
 * - return value = 0 : 읽을 데이터가 없는 경우 (EOF)
 * - return value < 0 : 오류 발생
 */
static ssize_t charDeviceRead(struct file *device_file, char __user *buffer, size_t count, loff_t *offset){
    size_t read_size = 0;

    // 현재 구현에서는 struct file의 정보를 사용하지 않는다.
    (void)device_file;

    printk("----device read----\n");

    // 사용자 요청 크기와 읽기를 시작할 현재 파일 위치를 확인한다.
    pr_info("count : %zu\n", count);
    pr_info("offset : %lld\n", (long long)*offset);

    // 음수 위치는 device_buffer의 유효한 인덱스가 될 수 없다.
    if(*offset < 0){
        pr_err("offset is negative\n");
        return -EINVAL;
    }

    // 현재 위치가 저장된 데이터의 끝이면 더 읽을 데이터가 없으므로 EOF를 반환한다.
    if((size_t)*offset >= data_size){
        pr_info("offset is over data size(offset : %lld, data_size : %zu)\n", (long long)*offset, data_size);
        return 0;
    }

    // 사용자 요청 크기와 현재 위치부터 남아 있는 데이터 크기 중 작은 값을 선택한다.
    read_size = min(count, data_size - (size_t)*offset);

    // 커널 버퍼의 현재 offset부터 사용자 공간 버퍼로 데이터를 복사한다.
    // copy_to_user()가 0이 아닌 값을 반환하면 일부 바이트를 복사하지 못한 것이다.
    if(copy_to_user(buffer, device_buffer + *offset, read_size)){
        pr_err("copy_to_user() failed\n");
        return -EFAULT;
    }

    // 다음 읽기가 이번에 읽은 데이터 뒤에서 시작하도록 파일 위치를 이동한다.
    *offset += read_size;

    // 이번에 실제 읽은 데이터 크기
    pr_info("read size : %zu\n", read_size);
    pr_info("current data : %.*s\n", (int)data_size, device_buffer);
    // 이번에 실제 읽은 데이터 값
    pr_info("read data : %.*s\n", (int)read_size, device_buffer + *offset - read_size);

    // 사용자 공간에는 실제로 전달한 바이트 수가 read()의 반환값으로 전달된다.
    return read_size;
}

// 생성한 문자 장치을 제어하기 위한 함수를 관리하는 구조체
// 관련 맴버 함수들은 https://elixir.bootlin.com/linux/v6.18.39/source/include/linux/fs.h#L2271 에서 참고
static struct file_operations fops ={
    .owner = THIS_MODULE,
    .open = charDeviceOpen,
    .release = charDeviceRelease,
    .write = charDeviceWrite,
    .read = charDeviceRead,
};
 
static int __init initModule(void){
    int ret = 0;

    // 문자 장치 생성
    major = register_chrdev(0, CHAR_DEV_NAME, &fops);
    // 문자 장치 생성시 에러가 발생한 경우
    if(major < 0){
        pr_err("register_chrdev() failed: %d\n", major);
        
        ret = major;
        goto err_chrdev;
    }

    // 정상적으로 문자 장치 생성됨을 알림 및 major 번호 확인
    pr_info("Success to init char device module\n");
    pr_info("Major : %d\n", major);
    
    // /sys/class에 test_class 폴더 생성
    chr_test_class = class_create(CHAR_DEV_CLASS_NAME);
    if(IS_ERR(chr_test_class)){
        pr_err("class_create() failed: %pe\n", chr_test_class);

        ret = PTR_ERR(chr_test_class);
        goto err_class;
    }

    // 정상적으로 /sys/class/test_class 폴더 생성
    pr_info("%s folder is created in /sys/class\n", CHAR_DEV_CLASS_NAME);

    chr_test_device = device_create(chr_test_class,NULL,MKDEV(major,0),NULL,CHAR_DEV_NAME);
    if(IS_ERR(chr_test_device)){
        pr_err("device_create() failed: %pe\n", chr_test_device);

        ret = PTR_ERR(chr_test_device);
        goto err_device;
    }

    // 정상적으로 /dev/testDevice 생성
    pr_info("%s is created in /dev\n", CHAR_DEV_NAME);

    return 0;

err_device:
    //test class 해제
    class_destroy(chr_test_class);
err_class:
    // 생성된 문자 장치를 해제
    unregister_chrdev(major, CHAR_DEV_NAME);
err_chrdev:
    return ret;
}
 
static void __exit exitModule(void){
    //test device 해제
    device_destroy(chr_test_class,MKDEV(major,0));
    //test class 해제
    class_destroy(chr_test_class);
    // 생성된 문자 장치를 해제
    unregister_chrdev(major, CHAR_DEV_NAME);
    
    pr_info("exit module\n");
}
 
module_init(initModule);
module_exit(exitModule);
 
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("This is a test device driver for character device");
