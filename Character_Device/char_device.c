#define pr_fmt(fmt) "[" KBUILD_MODNAME "][%s]: " fmt, __func__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>

#include <linux/device.h>
#include <linux/err.h>
 
// 문자 장치의 이름
#define CHAR_DEV_NAME               ("char_test_device")
#define CHAR_DEV_CLASS_NAME         ("char_test_class")

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

 
// 생성한 문자 장치의 Major 번호
static int major;
// 생성한 문자 장치을 제어하기 위한 함수를 관리하는 구조체
// 관련 맴버 함수들은 https://elixir.bootlin.com/linux/v6.18.39/source/include/linux/fs.h#L2271 에서 참고
static struct file_operations fops ={
    .owner = THIS_MODULE,
    .open = charDeviceOpen,
    .release = charDeviceRelease,
};

static struct class * chr_test_class;
static struct device * chr_test_device;
 
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
