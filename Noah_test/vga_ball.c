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

#define MAX_OBJECTS 20

/* Device registers */
#define BG_COLOR(x)      (x)
#define OBJECT_DATA(x,i) ((x) + 1 + (i))

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
    u32 color_data = ((u32)background->red << 16) | 
                        ((u32)background->green << 8) | 
                        background->blue;

    iowrite32(color_data, BG_COLOR(dev.virtbase));
    dev.background = *background;
}


/*
 * Write object data
 */
static void write_object(int index, unsigned short x, unsigned short y, char sprite_idx, char active)
{
    // if (index < 0 || index >= MAX_OBJECTS)
    //     return;
        
    // 构建32位对象数据
    u32 obj_data = ((u32)(x & 0xFFF) << 20) |   // x位置 (12位)
                ((u32)(y & 0xFFF) << 8) |    // y位置 (12位)
                ((u32)(sprite_idx & 0x3F) << 2) | // 精灵索引 (6位)
                ((u32)(active & 0x1) << 1);  // 活动状态 (1位)
                
    iowrite32(obj_data, OBJECT_DATA(dev.virtbase, index));
}

/*
 * Write all objects
 */
static void write_all(spaceship *ship, bullet bullets[], enemy enemies[])
{
    int i;
    bullet *bul;
    enemy *enemy;

    write_object(0,  ship->pos_x,  ship->pos_y, ship->sprite, ship->active);
    dev.ship = *ship;

    printk(KERN_INFO "%d, %d, %d \n", ship->pos_x, ship->pos_y, ship->active);


    for (i = 0; i < MAX_BULLETS; i++) {

        bul = &bullets[i];
        write_object(i+1,  bul->pos_x,  bul->pos_y, bul->sprite, bul->active);
        
        dev.bullets[i] = bullets[i];
    }

    for (i = 0; i < ENEMY_COUNT; i++) {

        enemy = &enemies[i];
        write_object(i+MAX_BULLETS+1,  enemy->pos_x,  enemy->pos_y, enemy->sprite, enemy->active);

        dev.enemies[i] = enemies[i];
    }

    for (i = 0; i < ENEMY_COUNT; i++) {

        enemy = &enemies[i];
        bul = &enemy->bul;
        write_object(i+MAX_BULLETS+ENEMY_COUNT+1,  bul->pos_x,  bul->pos_y, bul->sprite, bul->active);

        dev.enemies[i].bul = enemies[i].bul;
    }
}

/*
* Update all game state at once
*/
static void update_game_state(gamestate *game_state)
{
    write_background(&game_state->background);

    write_all(&game_state->ship, game_state->bullets, game_state->enemies);

}


static gamestate vb_arg;

/*
* Handle ioctl() calls from userspace
*/
static long vga_ball_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
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
    spaceship ship = { .pos_x = 300, .pos_y = 400, .active = 1};
    bullet bullets[MAX_BULLETS] = { 0 };    // All bullets initially inactive
    enemy enemies[ENEMY_COUNT] = { 0 };     // All enemies initially inactive

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
    write_all(&ship, bullets, enemies);
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
