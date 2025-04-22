/*
* Device driver for the VGA Ball game
*
* A Platform device implemented using the misc subsystem
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
#define BG_RED(x)         (x)
#define BG_GREEN(x)       ((x)+1)
#define BG_BLUE(x)        ((x)+2)
#define SHIP_X_L(x)       ((x)+3)
#define SHIP_X_H(x)       ((x)+4)
#define SHIP_Y_L(x)       ((x)+5)
#define SHIP_Y_H(x)       ((x)+6)
/* 子弹寄存器的定义，我们为每个子弹使用4个寄存器 */
#define BULLET_BASE(x)    ((x)+7)
#define BULLET_X_L(x,i)   (BULLET_BASE(x) + 4*(i))
#define BULLET_X_H(x,i)   (BULLET_BASE(x) + 4*(i) + 1)
#define BULLET_Y_L(x,i)   (BULLET_BASE(x) + 4*(i) + 2)
#define BULLET_Y_H(x,i)   (BULLET_BASE(x) + 4*(i) + 3)
#define BULLET_ACTIVE(x)  ((x)+27)  // 固定地址，对应vga_ball.sv中的27偏移量

/* 敌人寄存器定义 */
#define ENEMY1_X_L(x)      ((x)+28)
#define ENEMY1_X_H(x)      ((x)+29)
#define ENEMY1_Y_L(x)      ((x)+30)
#define ENEMY1_Y_H(x)      ((x)+31)
#define ENEMY2_X_L(x)      ((x)+32)
#define ENEMY2_X_H(x)      ((x)+33)
#define ENEMY2_Y_L(x)      ((x)+34)
#define ENEMY2_Y_H(x)      ((x)+35)
#define ENEMY_ACTIVE(x)    ((x)+36)

/* 敌人子弹寄存器定义 */
#define ENEMY_BULLET_BASE(x)   ((x)+37)
#define ENEMY_BULLET_X_L(x,i)  (ENEMY_BULLET_BASE(x) + 4*(i))
#define ENEMY_BULLET_X_H(x,i)  (ENEMY_BULLET_BASE(x) + 4*(i) + 1)
#define ENEMY_BULLET_Y_L(x,i)  (ENEMY_BULLET_BASE(x) + 4*(i) + 2)
#define ENEMY_BULLET_Y_H(x,i)  (ENEMY_BULLET_BASE(x) + 4*(i) + 3)
#define ENEMY_BULLET_ACTIVE(x) ((x)+61)

/*
* Information about our device
*/
struct vga_ball_dev {
    struct resource res; /* Resource: our registers */
    void __iomem *virtbase; /* Where registers can be accessed in memory */
    background_color background;
    spaceship ship;

} dev;

/*
* Write background color
*/
static void write_background(background_color *background)
{
    iowrite8(background->red, BG_RED(dev.virtbase));
    iowrite8(background->green, BG_GREEN(dev.virtbase));
    iowrite8(background->blue, BG_BLUE(dev.virtbase));
    dev.background = *background;
}

/*
 * Write ship position
 */
 static void write_ship(spaceship *ship)
 {
     iowrite8((unsigned char)(ship->position.x & 0xFF), SHIP_X_L(dev.virtbase));
     iowrite8((unsigned char)((ship->position.x >> 8) & 0x07), SHIP_X_H(dev.virtbase));
     iowrite8((unsigned char)(ship->position.y & 0xFF), SHIP_Y_L(dev.virtbase));
     iowrite8((unsigned char)((ship->position.y >> 8) & 0x03), SHIP_Y_H(dev.virtbase));
     dev.ship = *ship;
 }

/*
* Update all game state at once
*/
static void update_game_state(vga_ball_arg_t *state)
{
    write_background(&state->background);
    write_ship(&state->ship);

}

static vga_ball_arg_t vb_arg;

/*
* Handle ioctl() calls from userspace
*/
static long vga_ball_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{

    switch (cmd) {
        case VGA_BALL_UPDATE_GAME_STATE:
            if (copy_from_user(&vb_arg, (vga_ball_arg_t *) arg, sizeof(vga_ball_arg_t)))
                return -EACCES;
            update_game_state(&vb_arg);
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
    // Initial values
    background_color background = { 0x00, 0x00, 0x20 }; // Dark blue
    spaceship ship = { {400, 400}, 1};      // Ship starting position
    // bullet bullets[MAX_BULLETS] = { 0 };    // All bullets initially inactive
    // enemy enemies[ENEMY_COUNT] = { 0 };     // All enemies initially inactive

    int ret;

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
        
    /* Set initial values */
    write_background(&background);
    write_ship(&ship);
    // write_bullets(bullets);

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