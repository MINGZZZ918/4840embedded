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
#define BULLET_ACTIVE(x)  (BULLET_BASE(x) + 4*MAX_BULLETS)


#define ENEMY_BASE(x)     (x + 0x100) // Place it after bullet block
#define ENEMY_X_L(x,i)    (ENEMY_BASE(x) + 5*(i))
#define ENEMY_X_H(x,i)    (ENEMY_BASE(x) + 5*(i) + 1)
#define ENEMY_Y_L(x,i)    (ENEMY_BASE(x) + 5*(i) + 2)
#define ENEMY_Y_H(x,i)    (ENEMY_BASE(x) + 5*(i) + 3)
#define ENEMY_SPRITE(x,i) (ENEMY_BASE(x) + 5*(i) + 4)
#define ENEMY_ACTIVE(x)   (ENEMY_BASE(x) + 5*ENEMY_COUNT)




/*
* Information about our device
*/
struct vga_ball_dev {
    struct resource res; /* Resource: our registers */
    void __iomem *virtbase; /* Where registers can be accessed in memory */
    background_color background;
    spaceship ship;
    bullet bullets[MAX_BULLETS];
    enemy enemies[ENEMY_COUNT];
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
    iowrite8((unsigned char)(ship->pos_x & 0xFF), SHIP_X_L(dev.virtbase));
    iowrite8((unsigned char)((ship->pos_x >> 8) & 0x07), SHIP_X_H(dev.virtbase));
    iowrite8((unsigned char)(ship->pos_y & 0xFF), SHIP_Y_L(dev.virtbase));
    iowrite8((unsigned char)((ship->pos_y >> 8) & 0x03), SHIP_Y_H(dev.virtbase));
    dev.ship = *ship;
}

/*
* Write bullets properties
*/
static void write_bullets(bullet *bullets)
{
    unsigned char active_bits = 0;
    int i;
    
    for (i = 0; i < MAX_BULLETS; i++) {
        // 写入每个子弹的位置
        iowrite8((unsigned char)(bullets[i].pos_x & 0xFF), BULLET_X_L(dev.virtbase, i));
        iowrite8((unsigned char)((bullets[i].pos_x >> 8) & 0x07), BULLET_X_H(dev.virtbase, i));
        iowrite8((unsigned char)(bullets[i].pos_y & 0xFF), BULLET_Y_L(dev.virtbase, i));
        iowrite8((unsigned char)((bullets[i].pos_y >> 8) & 0x03), BULLET_Y_H(dev.virtbase, i));
        
        // 存储子弹状态位
        if (bullets[i].active)
            active_bits |= (1 << i);
            
        dev.bullets[i] = bullets[i];
    }
    
    // 写入子弹活动状态位图
    iowrite8(active_bits, BULLET_ACTIVE(dev.virtbase));
}


static void write_enemies(enemy *enemies)
{

    unsigned char active_bits = 0;
    int i;

    for (i = 0; i < ENEMY_COUNT; i++) {
        iowrite8(enemies[i].pos_x & 0xFF, ENEMY_X_L(dev.virtbase, i));
        iowrite8((enemies[i].pos_x >> 8) & 0x07, ENEMY_X_H(dev.virtbase, i));

        iowrite8(enemies[i].pos_y & 0xFF, ENEMY_Y_L(dev.virtbase, i));
        iowrite8((enemies[i].pos_y >> 8) & 0x03, ENEMY_Y_H(dev.virtbase, i));

        iowrite8(enemies[i].sprite, ENEMY_SPRITE(dev.virtbase, i));

        if (enemies[i].active)
            active_bits |= (1 << i);
    }

    iowrite8(active_bits, ENEMY_ACTIVE(dev.virtbase));
}

/*
* Update all game state at once
*/
static void update_game_state(gamestate *state)
{
    write_background(&state->background);
    write_ship(&state->ship);
    write_bullets(state->bullets);
    write_enemies(state->enemies);
}

/*
* Handle ioctl() calls from userspace
*/
static long vga_ball_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    gamestate vb_arg;

    switch (cmd) {
        case VGA_BALL_WRITE_BACKGROUND:
            if (copy_from_user(&vb_arg, (gamestate *) arg, sizeof(gamestate)))
                return -EACCES;
            write_background(&vb_arg.background);
            break;

        case VGA_BALL_READ_BACKGROUND:
            vb_arg.background = dev.background;
            if (copy_to_user((gamestate *) arg, &vb_arg, sizeof(gamestate)))
                return -EACCES;
            break;

        case VGA_BALL_WRITE_SHIP:
            if (copy_from_user(&vb_arg, (gamestate *) arg, sizeof(gamestate)))
                return -EACCES;
            write_ship(&vb_arg.ship);
            break;

        case VGA_BALL_READ_SHIP:
            vb_arg.ship = dev.ship;
            if (copy_to_user((gamestate *) arg, &vb_arg, sizeof(gamestate)))
                return -EACCES;
            break;

        case VGA_BALL_WRITE_BULLETS:
            if (copy_from_user(&vb_arg, (gamestate *) arg, sizeof(gamestate)))
                return -EACCES;
            write_bullets(vb_arg.bullets);
            break;

        case VGA_BALL_READ_BULLETS:
            memcpy(vb_arg.bullets, dev.bullets, sizeof(bullet) * MAX_BULLETS);
            if (copy_to_user((gamestate *) arg, &vb_arg, sizeof(gamestate)))
                return -EACCES;
            break;

        case VGA_BALL_WRITE_ENEMIES:
            if (copy_from_user(&vb_arg, (gamestate *) arg, sizeof(gamestate)))
                return -EACCES;
            write_enemies(vb_arg.enemies);
            break;

        case VGA_BALL_READ_ENEMIES:
            memcpy(vb_arg.enemies, dev.bullets, sizeof(enemy) * MAX_BULLETS);
            if (copy_to_user((gamestate *) arg, &vb_arg, sizeof(gamestate)))
                return -EACCES;
            break;

        case VGA_BALL_UPDATE_GAME_STATE:
            if (copy_from_user(&vb_arg, (gamestate *) arg, sizeof(gamestate)))
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
    spaceship ship = { { 200, 240 }, 1 };      // Ship starting position
    bullet bullets[MAX_BULLETS] = { 0 };    // All bullets initially inactive
    enemy enemies[ENEMY_COUNT] = { 0 };     // All enemies initially inactive

    int i, ret;

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
    write_bullets(bullets);

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