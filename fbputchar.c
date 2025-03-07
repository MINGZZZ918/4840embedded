/*
 * fbputchar: Framebuffer character generator
 *
 * Assumes 32bpp
 *
 * References:
 *
 * https://web.mit.edu/~firebird/arch/sun4x_59/doc/html/emb-framebuffer-howto.html
 * https://web.archive.org/web/20110515014922/https://web.njit.edu/all_topics/Prog_Lang_Docs/html/qt/emb-framebuffer-howto.html
 *
 * https://web.archive.org/web/20110415224759/http://www.diskohq.com/docu/api-reference/fb_8h-source.html
 */

#include "fbputchar.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

"fbputchar.c" 255L, 15733C                                    1,1           Top

