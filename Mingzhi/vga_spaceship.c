/*
 * Device driver for the VGA Space Shooter game
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
#include "vga_spaceship.h"

#define DRIVER_NAME "vga_spaceship"

/* Device registers */
#define BG_RED(x)         (x)
#define BG_GREEN(x)       ((x)+1)
#define BG_BLUE(x)        ((x)+2)
#define SHIP1_X_L(x)      ((x)+3)
#define SHIP1_X_H(x)      ((x)+4)
#define SHIP1_Y_L(x)      ((x)+5)
#define SHIP1_Y_H(x)      ((x)+6)
#define SHIP2_X_L(x)      ((x)+7)
#define SHIP2_X_H(x)      ((x)+8)
#define SHIP2_Y_L(x)      ((x)+9)
#define SHIP2_Y_H(x)      ((x)+10)
#define BULLET1_X_L(x)    ((x)+11)
#define BULLET1_X_H(x)    ((x)+12)
#define BULLET1_Y_L(x)    ((x)+13)
#define BULLET1_Y_H(x)    ((x)+14)
#define BULLET1_ACTIVE(x) ((x)+15)
#define BULLET2_X_L(x)    ((x)+16)
#define BULLET2_X_H(x)    ((x)+17)
#define BULLET2_Y_L(x)    ((x)+18)
#define BULLET2_Y_H(x)    ((x)+19)
#define BULLET2_ACTIVE(x) ((x)+20)

/*
 * Information about our device
 */
struct vga_spaceship_dev {
    struct resource res; /* Resource: our registers */
    void __iomem *virtbase; /* Where registers can be accessed in memory */
    vga_spaceship_color_t background;
    vga_spaceship_object_t ship1;
    vga_spaceship_object_t ship2;
    vga_spaceship_object_t bullet1;
    vga_spaceship_object_t bullet2;
} dev;

/*
 * Write background color
 */
static void write_background(vga_spaceship_color_t *background)
{
    iowrite8(background->red, BG_RED(dev.virtbase));
    iowrite8(background->green, BG_GREEN(dev.virtbase));
    iowrite8(background->blue, BG_BLUE(dev.virtbase));
    dev.background = *background;
}

/*
 * Write ship1 position
 */
static void write_ship1(vga_spaceship_object_t *ship)
{
    iowrite8((unsigned char)(ship->position.x & 0xFF), SHIP1_X_L(dev.virtbase));
    iowrite8((unsigned char)((ship->position.x >> 8) & 0x07), SHIP1_X_H(dev.virtbase));
    iowrite8((unsigned char)(ship->position.y & 0xFF), SHIP1_Y_L(dev.virtbase));
    iowrite8((unsigned char)((ship->position.y >> 8) & 0x03), SHIP1_Y_H(dev.virtbase));
    dev.ship1 = *ship;
}

/*
 * Write ship2 position
 */
static void write_ship2(vga_spaceship_object_t *ship)
{
    iowrite8((unsigned char)(ship->position.x & 0xFF), SHIP2_X_L(dev.virtbase));
    iowrite8((unsigned char)((ship->position.x >> 8) & 0x07), SHIP2_X_H(dev.virtbase));
    iowrite8((unsigned char)(ship->position.y & 0xFF), SHIP2_Y_L(dev.virtbase));
    iowrite8((unsigned char)((ship->position.y >> 8) & 0x03), SHIP2_Y_H(dev.virtbase));
    dev.ship2 = *ship;
}

/*
 * Write bullet1 properties
 */
static void write_bullet1(vga_spaceship_object_t *bullet)
{
    iowrite8((unsigned char)(bullet->position.x & 0xFF), BULLET1_X_L(dev.virtbase));
    iowrite8((unsigned char)((bullet->position.x >> 8) & 0x07), BULLET1_X_H(dev.virtbase));
    iowrite8((unsigned char)(bullet->position.y & 0xFF), BULLET1_Y_L(dev.virtbase));
    iowrite8((unsigned char)((bullet->position.y >> 8) & 0x03), BULLET1_Y_H(dev.virtbase));
    iowrite8(bullet->active, BULLET1_ACTIVE(dev.virtbase));
    dev.bullet1 = *bullet;
}

/*
 * Write bullet2 properties
 */
static void write_bullet2(vga_spaceship_object_t *bullet)
{
    iowrite8((unsigned char)(bullet->position.x & 0xFF), BULLET2_X_L(dev.virtbase));
    iowrite8((unsigned char)((bullet->position.x >> 8) & 0x07), BULLET2_X_H(dev.virtbase));
    iowrite8((unsigned char)(bullet->position.y & 0xFF), BULLET2_Y_L(dev.virtbase));
    iowrite8((unsigned char)((bullet->position.y >> 8) & 0x03), BULLET2_Y_H(dev.virtbase));
    iowrite8(bullet->active, BULLET2_ACTIVE(dev.virtbase));
    dev.bullet2 = *bullet;
}

/*
 * Update all game state at once
 */
static void update_game_state(vga_spaceship_arg_t *state)
{
    write_background(&state->background);
    write_ship1(&state->ship1);
    write_ship2(&state->ship2);
    write_bullet1(&state->bullet1);
    write_bullet2(&state->bullet2);
}

/*
 * Handle ioctl() calls from userspace
 */
static long vga_spaceship_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    vga_spaceship_arg_t vs_arg;

    switch (cmd) {
        case VGA_SPACESHIP_WRITE_BACKGROUND:
            if (copy_from_user(&vs_arg, (vga_spaceship_arg_t *) arg, sizeof(vga_spaceship_arg_t)))
                return -EACCES;
            write_background(&vs_arg.background);
            break;

        case VGA_SPACESHIP_READ_BACKGROUND:
            vs_arg.background = dev.background;
            if (copy_to_user((vga_spaceship_arg_t *) arg, &vs_arg, sizeof(vga_spaceship_arg_t)))
                return -EACCES;
            break;

        case VGA_SPACESHIP_WRITE_SHIP1:
            if (copy_from_user(&vs_arg, (vga_spaceship_arg_t *) arg, sizeof(vga_spaceship_arg_t)))
                return -EACCES;
            write_ship1(&vs_arg.ship1);
            break;

        case VGA_SPACESHIP_READ_SHIP1:
            vs_arg.ship1 = dev.ship1;
            if (copy_to_user((vga_spaceship_arg_t *) arg, &vs_arg, sizeof(vga_spaceship_arg_t)))
                return -EACCES;
            break;

        case VGA_SPACESHIP_WRITE_SHIP2:
            if (copy_from_user(&vs_arg, (vga_spaceship_arg_t *) arg, sizeof(vga_spaceship_arg_t)))
                return -EACCES;
            write_ship2(&vs_arg.ship2);
            break;

        case VGA_SPACESHIP_READ_SHIP2:
            vs_arg.ship2 = dev.ship2;
            if (copy_to_user((vga_spaceship_arg_t *) arg, &vs_arg, sizeof(vga_spaceship_arg_t)))
                return -EACCES;
            break;

        case VGA_SPACESHIP_WRITE_BULLET1:
            if (copy_from_user(&vs_arg, (vga_spaceship_arg_t *) arg, sizeof(vga_spaceship_arg_t)))
                return -EACCES;
            write_bullet1(&vs_arg.bullet1);
            break;

        case VGA_SPACESHIP_READ_BULLET1:
            vs_arg.bullet1 = dev.bullet1;
            if (copy_to_user((vga_spaceship_arg_t *) arg, &vs_arg, sizeof(vga_spaceship_arg_t)))
                return -EACCES;
            break;

        case VGA_SPACESHIP_WRITE_BULLET2:
            if (copy_from_user(&vs_arg, (vga_spaceship_arg_t *) arg, sizeof(vga_spaceship_arg_t)))
                return -EACCES;
            write_bullet2(&vs_arg.bullet2);
            break;

        case VGA_SPACESHIP_READ_BULLET2:
            vs_arg.bullet2 = dev.bullet2;
            if (copy_to_user((vga_spaceship_arg_t *) arg, &vs_arg, sizeof(vga_spaceship_arg_t)))
                return -EACCES;
            break;

        case VGA_SPACESHIP_UPDATE_GAME_STATE:
            if (copy_from_user(&vs_arg, (vga_spaceship_arg_t *) arg, sizeof(vga_spaceship_arg_t)))
                return -EACCES;
            update_game_state(&vs_arg);
            break;

        default:
            return -EINVAL;
    }

    return 0;
}

/* The operations our device knows how to do */
static const struct file_operations vga_spaceship_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = vga_spaceship_ioctl,
};

/* Information about our device for the "misc" framework */
static struct miscdevice vga_spaceship_misc_device = {
    .minor          = MISC_DYNAMIC_MINOR,
    .name           = DRIVER_NAME,
    .fops           = &vga_spaceship_fops,
};

/*
 * Initialization code: get resources and display initial state
 */
static int __init vga_spaceship_probe(struct platform_device *pdev)
{
    // Initial values
    vga_spaceship_color_t background = { 0x00, 0x00, 0x20 }; // Dark blue
    vga_spaceship_object_t ship1 = { { 200, 240 }, 1 };      // Ship 1 starting position
    vga_spaceship_object_t ship2 = { { 1000, 240 }, 1 };     // Ship 2 starting position
    vga_spaceship_object_t bullet1 = { { 0, 0 }, 0 };        // Bullet 1 (inactive)
    vga_spaceship_object_t bullet2 = { { 0, 0 }, 0 };        // Bullet 2 (inactive)
    
    int ret;

    /* Register ourselves as a misc device */
    ret = misc_register(&vga_spaceship_misc_device);

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
    write_ship1(&ship1);
    write_ship2(&ship2);
    write_bullet1(&bullet1);
    write_bullet2(&bullet2);

    return 0;

out_release_mem_region:
    release_mem_region(dev.res.start, resource_size(&dev.res));
out_deregister:
    misc_deregister(&vga_spaceship_misc_device);
    return ret;
}

/* Clean-up code: release resources */
static int vga_spaceship_remove(struct platform_device *pdev)
{
    iounmap(dev.virtbase);
    release_mem_region(dev.res.start, resource_size(&dev.res));
    misc_deregister(&vga_spaceship_misc_device);
    return 0;
}

/* Which "compatible" string(s) to search for in the Device Tree */
#ifdef CONFIG_OF
static const struct of_device_id vga_spaceship_of_match[] = {
    { .compatible = "csee4840,vga_spaceship-1.0" },
    {},
};
MODULE_DEVICE_TABLE(of, vga_spaceship_of_match);
#endif

/* Information for registering ourselves as a "platform" driver */
static struct platform_driver vga_spaceship_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(vga_spaceship_of_match),
    },
    .remove = __exit_p(vga_spaceship_remove),
};

/* Called when the module is loaded: set things up */
static int __init vga_spaceship_init(void)
{
    pr_info(DRIVER_NAME ": init\n");
    return platform_driver_probe(&vga_spaceship_driver, vga_spaceship_probe);
}

/* Called when the module is unloaded: release resources */
static void __exit vga_spaceship_exit(void)
{
    platform_driver_unregister(&vga_spaceship_driver);
    pr_info(DRIVER_NAME ": exit\n");
}

module_init(vga_spaceship_init);
module_exit(vga_spaceship_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Space Shooter Demo");
MODULE_DESCRIPTION("VGA Space Shooter demo driver");