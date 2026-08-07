#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>

#include <linux/device.h>
#include <linux/err.h>
 
// 문자 장치의 이름
#define CHAR_DEV_NAME               ("char_device")
#define CHAR_DEV_CLASS_NAME         ("char_test_class")
 
// 생성한 문자 장치의 Major 번호
static int major;
// 생성한 문자 장치을 제어하기 위한 함수를 관리하는 구조체
static struct file_operations fops ={
    .owner = THIS_MODULE,
};

static struct class * chr_test_class;
static struct device * chr_test_device;
 
static int __init initModule(void){
    int ret = 0;

    // 문자 장치 생성
    major = register_chrdev(0, CHAR_DEV_NAME, &fops);
    // 문자 장치 생성시 에러가 발생한 경우
    if(major < 0){
        printk("[%s] Error : register_chrdev()\n", CHAR_DEV_NAME);

        ret = major;
        goto err_chrdev;
    }

    // 정상적으로 문자 장치 생성됨을 알림 및 major 번호 확인
    printk("[%s] Success to init char device module\n", CHAR_DEV_NAME);
    printk("[%s] Major : %d\n", CHAR_DEV_NAME,major);
    
    // /sys/class에 test_class 폴더 생성
    chr_test_class = class_create(CHAR_DEV_CLASS_NAME);
    if(IS_ERR(chr_test_class)){
        printk("[%s] Error : class_create()\n", CHAR_DEV_NAME);

        ret = PTR_ERR(chr_test_class);
        goto err_class;
    }

    // 정상적으로 /sys/class/test_class 폴더 생성
    printk("[%s] %s folder is created in /sys/class\n", CHAR_DEV_NAME, CHAR_DEV_CLASS_NAME);

    chr_test_device = device_create(chr_test_class,NULL,MKDEV(major,0),NULL,CHAR_DEV_NAME);
    if(IS_ERR(chr_test_device)){
        printk("[%s] Error : device_create()\n", CHAR_DEV_NAME);

        ret = PTR_ERR(chr_test_device);
        goto err_device;
    }

    // 정상적으로 /dev/testDevice 생성
    printk("[%s] %s is created in /dev\n", CHAR_DEV_NAME, CHAR_DEV_NAME);

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
 
    printk("[%s] exit module\n", CHAR_DEV_NAME);
}
 
module_init(initModule);
module_exit(exitModule);
 
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("This is a test device driver for character device");