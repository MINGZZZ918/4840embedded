/*
 * Device driver for the VGA Ball game
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include "vga_ball.h"

#define DRIVER_NAME "vga_ball"

/* Device registers */
#define BG_COLOR(x)      (x)
#define OBJECT_DATA(x,i) ((x) + 1 + (i))  // 每个对象占用一个32位地址空间
#define RANDOM_BG_CTRL(x) ((x) + 21)      // 随机背景控制寄存器地址

/*
 * Information about our device
 */
struct vga_ball_dev {
    struct resource res; /* Resource: our registers */
    void __iomem *virtbase; /* Where registers can be accessed in memory */
    vga_ball_color_t background;
    vga_ball_object_t objects[MAX_OBJECTS];
    int use_random_bg; /* 控制是否使用随机背景 */
} dev;

/*
 * Write background color
 */
static void write_background(vga_ball_color_t *background)
{
    u32 color_data = ((u32)background->red << 16) | 
                     ((u32)background->green << 8) | 
                     background->blue;
    iowrite32(color_data, BG_COLOR(dev.virtbase));
    dev.background = *background;
}

/*
 * Set random background mode
 */
static void set_random_background(int enable)
{
    iowrite32(enable & 0x1, RANDOM_BG_CTRL(dev.virtbase));
    dev.use_random_bg = enable;
    printk(KERN_INFO "VGA Ball: Random background %s\n", 
           enable ? "enabled" : "disabled");
}

/*
 * Write object data
 */
static void write_object(int index, vga_ball_object_t *object)
{
    if (index < 0 || index >= MAX_OBJECTS)
        return;
        
    // 构建32位对象数据
    u32 obj_data = ((u32)(object->x & 0xFFF) << 20) |   // x位置 (12位)
                   ((u32)(object->y & 0xFFF) << 8) |    // y位置 (12位)
                   ((u32)(object->sprite_idx & 0x3F) << 2) | // 精灵索引 (6位)
                   ((u32)(object->active & 0x1) << 1);  // 活动状态 (1位)
                   
    iowrite32(obj_data, OBJECT_DATA(dev.virtbase, index));
    
    dev.objects[index] = *object;
}

/*
 * Write all objects
 */
static void write_all_objects(vga_ball_object_t objects[])
{
    int i;
    for (i = 0; i < MAX_OBJECTS; i++) {
        write_object(i, &objects[i]);
    }
}

/*
 * Update all game state at once
 */
static void update_game_state(vga_ball_arg_t *state)
{
    write_background(&state->background);
    write_all_objects(state->objects);
}

/*
 * Handle ioctl() calls from userspace
 */
static long vga_ball_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    vga_ball_arg_t vb_arg;
    int random_bg_enable;

    switch (cmd) {
        case VGA_BALL_WRITE_BACKGROUND:
            if (copy_from_user(&vb_arg, (vga_ball_arg_t *) arg, sizeof(vga_ball_arg_t)))
                return -EACCES;
            write_background(&vb_arg.background);
            break;

        case VGA_BALL_READ_BACKGROUND:
            vb_arg.background = dev.background;
            if (copy_to_user((vga_ball_arg_t *) arg, &vb_arg, sizeof(vga_ball_arg_t)))
                return -EACCES;
            break;

        case VGA_BALL_WRITE_OBJECTS:
            if (copy_from_user(&vb_arg, (vga_ball_arg_t *) arg, sizeof(vga_ball_arg_t)))
                return -EACCES;
            write_all_objects(vb_arg.objects);
            break;

        case VGA_BALL_READ_OBJECTS:
            memcpy(vb_arg.objects, dev.objects, sizeof(vga_ball_object_t) * MAX_OBJECTS);
            if (copy_to_user((vga_ball_arg_t *) arg, &vb_arg, sizeof(vga_ball_arg_t)))
                return -EACCES;
            break;

        case VGA_BALL_UPDATE_GAME_STATE:
            if (copy_from_user(&vb_arg, (vga_ball_arg_t *) arg, sizeof(vga_ball_arg_t)))
                return -EACCES;
            update_game_state(&vb_arg);
            break;
            
        case VGA_BALL_SET_RANDOM_BG:
            if (copy_from_user(&random_bg_enable, (int *) arg, sizeof(int)))
                return -EACCES;
            set_random_background(random_bg_enable);
            break;

        default:
            return -EINVAL;
    }

    return 0;
}

/* The operations our device knows how to do */
static const struct file_operations vga_ball_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = vga_ball_ioctl,
};

/* Information about our device for the "misc" framework */
static struct miscdevice vga_ball_misc_device = {
    .minor          = MISC_DYNAMIC_MINOR,
    .name           = DRIVER_NAME,
    .fops           = &vga_ball_fops,
};

/*
 * Initialization code: get resources and display initial state
 */
static int __init vga_ball_probe(struct platform_device *pdev)
{
    int i, ret;
    
    // 初始默认值
    vga_ball_color_t background = { 0x00, 0x00, 0x20 }; // 深蓝色
    vga_ball_object_t objects[MAX_OBJECTS] = { 0 };

    /* Register ourselves as a misc device */
    ret = misc_register(&vga_ball_misc_device);

    /* Get the address of our registers from the device tree */
    ret = of_address_to_resource(pdev->dev.of_node, 0, &dev.res);
    if (ret) {
        ret = -ENOENT;
        goto out_deregister;
    }

    /* Make sure we can use these registers */
    if (request_mem_region(dev.res.start, resource_size(&dev.res),
                           DRIVER_NAME) == NULL) {
        ret = -EBUSY;
        goto out_deregister;
    }

    /* Arrange access to our registers */
    dev.virtbase = of_iomap(pdev->dev.of_node, 0);
    if (dev.virtbase == NULL) {
        ret = -ENOMEM;
        goto out_release_mem_region;
    }
        
    /* 初始化对象 */
    // 飞船
    objects[SHIP_INDEX].x = 200;
    objects[SHIP_INDEX].y = 240;
    objects[SHIP_INDEX].sprite_idx = SHIP_SPRITE;
    objects[SHIP_INDEX].active = 1;
    
    // 敌人1
    objects[ENEMY1_INDEX].x = 500;
    objects[ENEMY1_INDEX].y = 150;
    objects[ENEMY1_INDEX].sprite_idx = ENEMY_SPRITE;
    objects[ENEMY1_INDEX].active = 1;
    
    // 敌人2
    objects[ENEMY2_INDEX].x = 500;
    objects[ENEMY2_INDEX].y = 350;
    objects[ENEMY2_INDEX].sprite_idx = ENEMY_SPRITE;
    objects[ENEMY2_INDEX].active = 1;
    
    // 所有子弹初始为不活动
    for (i = BULLET_START_INDEX; i < ENEMY_BULLET_START_INDEX; i++) {
        objects[i].x = 0;
        objects[i].y = 0;
        objects[i].sprite_idx = BULLET_SPRITE;
        objects[i].active = 0;
    }
    
    // 所有敌人子弹初始为不活动
    for (i = ENEMY_BULLET_START_INDEX; i < MAX_OBJECTS; i++) {
        objects[i].x = 0;
        objects[i].y = 0;
        objects[i].sprite_idx = ENEMY_BULLET_SPRITE;
        objects[i].active = 0;
    }
        
    /* 设置初始值 */
    write_background(&background);
    write_all_objects(objects);

    return 0;

out_release_mem_region:
    release_mem_region(dev.res.start, resource_size(&dev.res));
out_deregister:
    misc_deregister(&vga_ball_misc_device);
    return ret;
}


/* Clean-up code: release resources */
static int vga_ball_remove(struct platform_device *pdev)
{
    iounmap(dev.virtbase);
    release_mem_region(dev.res.start, resource_size(&dev.res));
    misc_deregister(&vga_ball_misc_device);
    return 0;
}

/* Which "compatible" string(s) to search for in the Device Tree */
#ifdef CONFIG_OF
static const struct of_device_id vga_ball_of_match[] = {
    { .compatible = "csee4840,vga_ball-1.0" },
    {},
};
MODULE_DEVICE_TABLE(of, vga_ball_of_match);
#endif

/* Information for registering ourselves as a "platform" driver */
static struct platform_driver vga_ball_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(vga_ball_of_match),
    },
    .remove = __exit_p(vga_ball_remove),
};

/* Called when the module is loaded: set things up */
static int __init vga_ball_init(void)
{
    pr_info(DRIVER_NAME ": init\n");
    return platform_driver_probe(&vga_ball_driver, vga_ball_probe);
}

/* Called when the module is unloaded: release resources */
static void __exit vga_ball_exit(void)
{
    platform_driver_unregister(&vga_ball_driver);
    pr_info(DRIVER_NAME ": exit\n");
}

module_init(vga_ball_init);
module_exit(vga_ball_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("VGA Ball Demo");
MODULE_DESCRIPTION("VGA Ball demo driver");

/* 其余内核驱动代码保持不变 */