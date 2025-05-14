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
#define BG_COLOR(x)      (x)
#define OBJECT_DATA(x,i) ((x) + (4*(i)))

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
    powerup power_up;
    int score;
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
static void write_object(int idx, unsigned short x, unsigned short y, char sprite_idx, char active)
{
    // 构建32位对象数据
    u32 obj_data = ((u32)(x & 0xFFF) << 20) |   // x位置 (12位)
                ((u32)(y & 0xFFF) << 8) |    // y位置 (12位)
                ((u32)(sprite_idx & 0x3F) << 2) | // 精灵索引 (6位)
                ((u32)(active & 0x1) << 1);  // 活动状态 (1位)
                
    iowrite32(obj_data, OBJECT_DATA(dev.virtbase, idx));
}

static void write_score(int idx, int score)
{
    u32 obj_data = (uint32_t)(score & 0xFFFFFFFF);

    iowrite32(obj_data, OBJECT_DATA(dev.virtbase, idx));

    dev.score = score;
}


static void write_ship(spaceship *ship){

    int sprite, i, active;

    if (ship->sprite == SHIP_EXPLOSION1) sprite = SHIP_EXPLOSION1;

    else if (ship->sprite == SHIP_EXPLOSION2) sprite = SHIP_EXPLOSION2;

    else if (ship->velo_x < 0) sprite = SHIP_LEFT;

    else if (ship->velo_x > 0) sprite = SHIP_RIGHT;

    else sprite = SHIP;

    write_object (2, ship->pos_x,  ship->pos_y, sprite, ship->active);
    
    write_object (3, ship->pos_x,  ship->pos_y+SHIP_HEIGHT, SHIP_FLAME, ship->active);

    dev.ship = *ship;

    for(i = 0; i<LIFE_COUNT; i++){

        if(i<ship->lives) active = 1;
        else active = 0;

        write_object (i+4, i*20+10,  SCREEN_HEIGHT-16, SHIP, active);
    }
}

static void write_ship_bullets(spaceship *ship){

    int i;
    bullet *bul;

    for (i = 0; i < SHIP_BULLETS; i++) {

        bul = &ship->bullets[i];
        write_object (i+LIFE_COUNT+4, bul->pos_x,  bul->pos_y, SHIP_BULLET, bul->active);

        dev.ship.bullets[i] = *bul;
    }
}


/*
 * Write all objects
 */
static void write_enemies(bullet bullets[], enemy enemies[])
{

    int i;
    bullet *bul;
    enemy *enemy;
    char sprite;

    for (i = 0; i < ENEMY_COUNT; i++) {

        enemy = &enemies[i];

        write_object(i+SHIP_BULLETS+LIFE_COUNT+4,  enemy->pos_x,  enemy->pos_y, enemy->sprite, enemy->active);
        dev.enemies[i] = enemies[i];
    }

    for (i = 0; i < MAX_BULLETS; i++) {

        bul = &bullets[i];

        if (bul->velo_x < 0) sprite = ENEMY_BULLET_LEFT;

        else if (bul->velo_x > 0) sprite = ENEMY_BULLET_RIGHT;

        else sprite = ENEMY_BULLET;

        write_object(i+SHIP_BULLETS+LIFE_COUNT+ENEMY_COUNT+4,  bul->pos_x,  bul->pos_y, sprite, bul->active);
        dev.bullets[i] = *bul;
    }
}



static void write_powerup(powerup *power_up){

    write_object (SHIP_BULLETS+LIFE_COUNT+ENEMY_COUNT+MAX_BULLETS+4, power_up->pos_x,  power_up->pos_y, power_up->sprite, power_up->active);

    dev.power_up = *power_up;

}


/*
* Update all game state at once
*/
static void update_enemies(gamestate *game_state)
{
    write_background(&game_state->background);

    write_score(1, game_state->score);

    write_enemies(game_state->bullets, game_state->enemies);
}


static gamestate vb_arg;
static spaceship vb_ship;
static powerup vb_pu;

/*
* Handle ioctl() calls from userspace
*/
static long vga_ball_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
        case UPDATE_ENEMIES:
            if (copy_from_user(&vb_arg, (gamestate *) arg, sizeof(gamestate)))
                return -EACCES;
            update_enemies(&vb_arg);
            break;

        case UPDATE_SHIP:
            if (copy_from_user(&vb_ship, (spaceship *) arg, sizeof(spaceship)))
                return -EACCES;
            write_ship(&vb_ship);
            break;

        case UPDATE_SHIP_BULLETS:
            if (copy_from_user(&vb_ship, (spaceship *) arg, sizeof(spaceship)))
                return -EACCES;
            write_ship_bullets(&vb_ship);
            break;

        case UPDATE_POWERUP:
            if (copy_from_user(&vb_pu, (powerup *) arg, sizeof(powerup)))
                return -EACCES;
            write_powerup(&vb_pu);
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
    background_color background = { 0xFF, 0x00, 0xFF };

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

