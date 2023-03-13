#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/module.h>

/*linux中的用户态内存交互函数，copy_from_user(),copy_to_user()等*/
#include <asm/uaccess.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
/*misc混合设备注册与注销*/
#include <linux/miscdevice.h>
//模块参数，GPIO组号、组内偏移、方向、输出时的输出初始值

// pps
static unsigned int gpio_chip_num = 8;
module_param(gpio_chip_num, uint, S_IRUGO);
MODULE_PARM_DESC(gpio_chip_num, "gpio chip num");
static unsigned int gpio_offset_num = 1;
module_param(gpio_offset_num, uint, S_IRUGO);
MODULE_PARM_DESC(gpio_offset_num, "gpio offset num");


static unsigned int gpio_dir = 0;
module_param(gpio_dir, uint, S_IRUGO);
MODULE_PARM_DESC(gpio_dir, "gpio dir");
static unsigned int gpio_out_val = 0;
module_param(gpio_out_val, uint, S_IRUGO);
MODULE_PARM_DESC(gpio_out_val, "gpio out val");
//模块参数，中断触发类型
/*
 * 0 - disable irq
 * 1 - rising edge triggered
 * 2 - falling edge triggered
 * 3 - rising and falling edge triggered
 * 4 - high level triggered
 * 8 - low level triggered
 */


static unsigned int gpio_num0;

// static unsigned int gpio_irq_type = 1;
// module_param(gpio_irq_type, uint, S_IRUGO);
// MODULE_PARM_DESC(gpio_irq_type, "gpio irq type");

static unsigned int key = 0;
/*定义一个整形变量，判断按键是否按下*/
static volatile int ev_press = 0;
/*定义和初始化一个等待队列头*/
static DECLARE_WAIT_QUEUE_HEAD(button_waitq);
#define DEVICE_NAME "gpio_dev"

spinlock_t lock;
static int gpio_dev_test_in(unsigned int gpio_num)
{
        //设置方向为输入
        if (gpio_direction_input(gpio_num))
        {
                pr_err("[%s %d]gpio_direction_input fail!\n",
                       __func__, __LINE__);
                return -EIO;
        }
        //读出GPIO输入值
        pr_info("[%s %d]gpio%d_%d in %d\n", __func__, __LINE__,
                gpio_num / 8, gpio_num % 8,
                gpio_get_value(gpio_num));
        return 0;
}
//中断处理函数
static irqreturn_t gpio_dev_test_isr(int irq, void *dev_id)
{
        
        if (gpio_to_irq(gpio_num0) == irq){
                key = 1;
                ev_press = 1;
                wake_up_interruptible(&button_waitq);

                
                //mdelay(10);
                // if(gpio_get_value(gpio_num0) == 0){
                //         key = 1;
                //         ev_press = 1;
                //         wake_up_interruptible(&button_waitq);
                //         //pr_info("in key: %d\n", key);
                // }
                // else{
                //         key = 0;
                // }
        }
        // pr_info("in key: %d\n", key);

        return IRQ_HANDLED;
}

/*
 *read调用的具体函数，由它读取键盘输入的结果，
 *实质上就是读取key_values数组的值
 *完成键盘输入设备的核心功能，根据标志位ev_press判断是否可读
 *如果可读，则读取数据到用户buffer中，如果不可读，
 *则进程进入等待队列等待，直到数组可读为止
 *等待队列机制，所中断管理中常用的机制。
 */
static int gpio_dev_test_irq_read(struct file *filp,
                                  char __user *buff,
                                  size_t count, loff_t *offp)
{
        // pr_info("read now\n", __func__, __LINE__);

        unsigned long err;

        if (!ev_press) /*ev_press=0,则表示没有数据可读*/
        {
                if (filp->f_flags & O_NONBLOCK)
                        return -EAGAIN;
                else /*无数据可读时，进程休眠，放进button_waitq等待队列*/
                        wait_event_interruptible(button_waitq, ev_press);
                /*
                 *wait_event_interruptible()函数将进程置为可中断的挂起状态
                 *反复检查ev_press＝1是否成立，如果不成立，则继续休眠。
                 *条件满足后，即把本程序置为运行态，
                 */
        }
        /*ev_press=1之后，进程退出等待队列。从此处开始运行*/
        ev_press = 0; /*置0标志位，表明本次中断已经处理*/
        err = copy_to_user(buff, &key, sizeof(key));
        /*把按键值传回用户空间*/

        return 0;
}

static int gpio_dev_test_irq(unsigned int gpio_num, unsigned int gpio_irq_type)
{
        unsigned int irq_num;
        unsigned int irqflags = 0;
        //设置方向为输入
        if (gpio_direction_input(gpio_num))
        {
                pr_err("[%s %d]gpio_direction_input fail!\n",
                       __func__, __LINE__);
                return -EIO;
        }
        switch (gpio_irq_type)
        {
        case 1:
                irqflags = IRQF_TRIGGER_RISING;
                break;
        case 2:
                irqflags = IRQF_TRIGGER_FALLING;
                break;
        case 3:
                irqflags = IRQF_TRIGGER_RISING |
                           IRQF_TRIGGER_FALLING;
                break;
        case 4:
                irqflags = IRQF_TRIGGER_HIGH;
                break;
        case 8:
                irqflags = IRQF_TRIGGER_LOW;
                break;
        default:
                pr_info("[%s %d]gpio_irq_type error!\n",
                        __func__, __LINE__);
                return -1;
        }
        irqflags |= IRQF_SHARED;
        //根据GPIO编号映射中断号
        irq_num = gpio_to_irq(gpio_num);

        //注册中断
        if (request_irq(irq_num, gpio_dev_test_isr, irqflags,
                        "gpio_dev_test", &gpio_irq_type))
        {
                pr_info("[%s %d]request_irq error!\n", __func__, __LINE__);
                return -1;
        }

        pr_info("[%d]request_irq [%d]!\n", gpio_num, gpio_irq_type);

        return 0;
}
static void gpio_dev_test_irq_exit(unsigned int gpio_num, unsigned int gpio_irq_type)
{
        unsigned long flags;
        pr_info("[%s %d]\n", __func__, __LINE__);
        //释放注册的中断
        spin_lock_irqsave(&lock, flags);
        free_irq(gpio_to_irq(gpio_num), &gpio_irq_type);
        spin_unlock_irqrestore(&lock, flags);
}
static int gpio_dev_test_out(unsigned int gpio_num, unsigned int
                                                        gpio_out_val)
{
        //设置方向为输出，并输出一个初始值
        if (gpio_direction_output(gpio_num, !!gpio_out_val))
        {
                pr_err("[%s %d]gpio_direction_output fail!\n",
                       __func__, __LINE__);
                return -EIO;
        }
        pr_info("[%s %d]gpio%d_%d out %d\n", __func__, __LINE__,
                gpio_num / 8, gpio_num % 8, !!gpio_out_val);
        return 0;
}

static int gpio_dev_test_irq_open(struct inode *inode, struct file *file)
{
        return 0;
}
static int gpio_dev_test_irq_close(struct inode *inode, struct file *file)
{
        return 0;
}

static struct file_operations dev_fops = {
    .owner = THIS_MODULE,
    .open = gpio_dev_test_irq_open,
    .release = gpio_dev_test_irq_close,
    .read = gpio_dev_test_irq_read,

};

/*
 *misc混合设备注册和注销
 */
static struct miscdevice misc = {
    .minor = MISC_DYNAMIC_MINOR, /*次设备号*/
    .name = DEVICE_NAME,         /*设备名*/
    .fops = &dev_fops,           /*设备文件操作结构体*/
};

static int __init gpio_dev_test_init(void)
{
        unsigned int gpio_irq_typepps = 2;
        int ret = -1;
        ret = misc_register(&misc);
        if (0 != ret)
        {
                printk("register device failed!\n");
                return -1;
        }
        printk("register gpio_device success !\n");

        int status = 0;
        pr_info("gpiodev_[%s %d]\n", __func__, __LINE__);
        spin_lock_init(&lock);

        gpio_num0 = gpio_chip_num * 8 + gpio_offset_num;
        //注册要操作的GPIO编号
        if (gpio_request(gpio_num0, NULL))
        {
                pr_err("gpio_num0 [%s %d]gpio_request fail! gpio_num=%d \n", __func__,
                       __LINE__, gpio_num0);
                return -EIO;
        }
        status = gpio_dev_test_irq(gpio_num0, gpio_irq_typepps);
        if (status)
                gpio_free(gpio_num0);
        return status;
}
module_init(gpio_dev_test_init);
static void __exit gpio_dev_test_exit(void)
{
        unsigned int gpio_irq_typepps = 2;
        misc_deregister(&misc);
        printk("unregister device success !\n");
        pr_info("[%s %d]\n", __func__, __LINE__);
        gpio_dev_test_irq_exit(gpio_num0, gpio_irq_typepps);

        //释放注册的GPIO编号
        gpio_free(gpio_num0);
}
module_exit(gpio_dev_test_exit);
MODULE_DESCRIPTION("GPIO device test Driver sample");
MODULE_LICENSE("GPL");
