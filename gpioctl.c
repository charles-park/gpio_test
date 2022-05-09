//------------------------------------------------------------------------------------------------------------
//
// GPIO Control Library(sysfs control)
//
// 2022.03.16 (charles.park@hardkernel.com)
//
//------------------------------------------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include "typedefs.h"
#include "gpioctl.h"

//------------------------------------------------------------------------------------------------------------
bool gpio_export_check  (int gpio);
bool gpio_export        (int gpio);
bool gpio_unexport      (int gpio);
bool gpio_direction     (int gpio, bool isout);
bool gpio_set           (int gpio, bool level);
bool gpio_get           (int gpio);

//------------------------------------------------------------------------------------------------------------
bool gpio_export_check (int gpio)
{
    __u8 buf[64];

    memset (buf, 0, sizeof(buf));
    sprintf(buf, "/sys/class/gpio/gpio%d", gpio);

    if (!access(buf, F_OK)) {
        info ("already export %s\n", buf);
        return true;
    }
    return false;
}

//------------------------------------------------------------------------------------------------------------
bool gpio_export (int gpio)
{
    int fd;
    __u8 buf[64];

    if (gpio_export_check (gpio))
        return 1;

    if ((fd = open(GPIO_SYSFS_EXPORT, O_WRONLY)) < 0) {
        err ("%s file open error!\n", GPIO_SYSFS_EXPORT);
        return 0;
    }

    memset (buf, 0, sizeof(buf));
    sprintf(buf, "%d", gpio);
    write  (fd, buf, strlen(buf));
    close  (fd);
    return 1;
}

//------------------------------------------------------------------------------------------------------------
bool gpio_unexport (int gpio)
{
    int fd;
    __u8 buf[64];

    if (gpio_export_check (gpio)) {
        if ((fd = open(GPIO_SYSFS_UNEXPORT, O_WRONLY)) < 0) {
            err ("%s file open error!\n", GPIO_SYSFS_UNEXPORT);
            return 0;
        }
        memset (buf, 0, sizeof(buf));
        sprintf(buf, "%d", gpio);
        write  (fd, buf, strlen(buf));
        close  (fd);
        return 1;
    }
    info ("gpio %d is not export.\n", gpio);
    return 0;
}

//------------------------------------------------------------------------------------------------------------
bool gpio_direction (int gpio, bool isout)
{
    int fd;
    __u8 buf[64];

    if (!gpio_export_check(gpio)) {
        memset (buf, 0, sizeof(buf));
        sprintf(buf, "/sys/class/gpio/gpio%d/direction", gpio);
        if ((fd = open (buf, O_WRONLY)) < 0) {
            err ("%s file open error!\n", buf);
            return 0;
        }
        write (fd, isout ? "out" : "in", isout ? strlen("out") : strlen("in"));
        close (fd);
    }
    return 1;
}
//------------------------------------------------------------------------------------------------------------
bool gpio_set (int gpio, bool level)
{
    int fd;
    __u8 buf[64];

    if (!gpio_export_check(gpio)) {
        memset (buf, 0, sizeof(buf));
        sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio);
        if ((fd = open (buf, O_WRONLY)) < 0) {
            err ("%s file open error!\n", buf);
            return 0;
        }
        write (fd, level ? "1" : "0", 1);
        close (fd);
    }
    return 1;
}

//------------------------------------------------------------------------------------------------------------
bool gpio_get (int gpio)
{
    int fd;
    __u8 buf[64];

    if (!gpio_export_check(gpio)) {
        memset (buf, 0, sizeof(buf));
        sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio);
        if ((fd = open (buf, O_RDONLY)) < 0) {
            err ("%s file open error!\n", buf);
            return 0;
        }
        read (fd, buf, 1);
        close (fd);
    }
    return (buf[0] == '1') ? 1 : 0;
}

//------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------
