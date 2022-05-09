//------------------------------------------------------------------------------
//
// 2022.04.14 Argument parser app. (chalres-park)
//
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <linux/fb.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h> 
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

//------------------------------------------------------------------------------
#include "typedefs.h"
#include "uartlib.h"
#include "gpioctl.h"

//------------------------------------------------------------------------------
static void tolowerstr (char *p)
{
	int i, c = strlen(p);

	for (i = 0; i < c; i++, p++)
		*p = tolower(*p);
}

//------------------------------------------------------------------------------
static void toupperstr (char *p)
{
	int i, c = strlen(p);

	for (i = 0; i < c; i++, p++)
		*p = toupper(*p);
}

//------------------------------------------------------------------------------
static void print_usage(const char *prog)
{
	printf("Usage: %s [-Dsbd]\n", prog);
	puts("  -D --device        device name. (default /dev/ttyUSB0).\n"
		 "  -b --baudrate      serial baudrate.(default 115200bps n81)\n"
		 "  -s --server        server mode enable (default client mode).\n"
	     "  -d --delay         data r/w delay (default = 1 sec)\n"
	);
	exit(1);
}

//------------------------------------------------------------------------------
static char 	*OPT_DEVICE_NAME = "/dev/ttyUSB0";
static bool		OPT_SERVER_MODE = false;;
static int 		OPT_BAUDRATE = 115200, OPT_DELAY = 1;

//------------------------------------------------------------------------------
static void parse_opts (int argc, char *argv[])
{
	while (1) {
		static const struct option lopts[] = {
			{ "device_name",	1, 0, 'D' },
			{ "server_mode",	0, 0, 's' },
			{ "baudrate",		1, 0, 'b' },
			{ "delay",			1, 0, 'd' },
			{ NULL, 0, 0, 0 },
		};
		int c;

		c = getopt_long(argc, argv, "D:sb:d:", lopts, NULL);

		if (c == -1)
			break;

		switch (c) {
		case 'D':
			OPT_DEVICE_NAME = optarg;
			break;
		case 's':
			OPT_SERVER_MODE = true;
			break;
		case 'b':
			OPT_BAUDRATE = atoi(optarg);
			break;
		case 'd':
			OPT_DELAY = atoi(optarg);
			break;
		default:
			print_usage(argv[0]);
			break;
		}
	}
}

//------------------------------------------------------------------------------
typedef struct gpio_bind__t {
	int		pin;
	int		gpio;
}	gpio_bind_t;

const gpio_bind_t CON1_TEST_GPIOS[] = {
	{  3, 493 },
	{  5, 494 },
	{  7, 473 },
	{  8, 488 },
	{ 10, 489 },
	{ 11, 479 },
	{ 12, 492 },
	{ 13, 480 },
	{ 15, 483 },
	{ 16, 476 },
	{ 18, 477 },
	{ 19, 484 },
	{ 21, 485 },
	{ 22, 478 },
	{ 23, 487 },
	{ 24, 486 },
	{ 26, 464 },
	{ 27, 474 },

	{ 28, 473 },
	{ 29, 473 },
	{ 31, 473 },
	{ 32, 473 },
	{ 33, 473 },
	{ 35, 473 },
	{ 36, 473 },
};

#pragma pack(1)
typedef struct gpio_test__t {
	int		gpio;
	int		value;
}	gpio_test_t;

#pragma pack(1)
/* Total 53 bytes */
typedef struct protocol__t {
	/*
		'@' Start signal
	*/
	char	header;
	/*
		status value = 'S'et, 'A'ck, 'B'usy, 'E'rror, 'W'ait, 'R'eady.
	*/
	char	status;
	/*
		Test group : ADC, GPIO, USB, ...
	*/
	char	msg[10];
	/*
		Data group.
	*/
	__u8	data[40];
	/*
		'#' End signal
	*/
	char	tail;
}	protocol_t;


//------------------------------------------------------------------------------
int chk_func(ptc_var_t *var)
{
	if(var->buf[(var->p_sp + var->size -1) % var->size] != '#')	return 0;
	if(var->buf[(var->p_sp               ) % var->size] != '@')	return 0;

	return 1;
}

//------------------------------------------------------------------------------
bool gpio_check(int gpio, int value)
{
	return 1;
}

//------------------------------------------------------------------------------
int cat_func (ptc_var_t *var)
{
	char 		*r = (char *)var->arg, i;
	protocol_t 	*p = (protocol_t *)var->arg;

	for (i = 0; i < sizeof(protocol_t); i++)
		r[i] = var->buf[(var->p_sp + i) % var->size];

	if (!strncmp(p->msg, "GPIO", strlen("GPIO")) && (p->status == 'A')) {
		gpio_test_t *g = (gpio_test_t *)p->data;
		if (gpio_check (g->gpio, g->value))
			info ("gpio(%d)(%d)_pass\n", g->gpio, g->value);
		else
			info ("gpio(%d)(%d)_error\n", g->gpio, g->value);
	}
	return 1;
}

//------------------------------------------------------------------------------
void send_msg (ptc_grp_t *ptc_grp, void *s)
{
	char *p = (char *)s;
	int i;
	for (i = 0; i < sizeof(protocol_t); i++)
		queue_put (&ptc_grp->dq, p + i);
}

//------------------------------------------------------------------------------
int send_check_gpio (ptc_grp_t *ptc_grp, int gpio, int value)
{
	protocol_t p;
	gpio_test_t	*g;

	memset(&p,  0, sizeof(protocol_t));

	p.header 	= '@';
	p.tail 		= '#';
	p.status	= 'S';

	sprintf(p.msg, "%s", "GPIO");
	g = (gpio_test_t *)p.data;
	g->gpio		= gpio;
	g->value 	= value;

	send_msg (ptc_grp, &p);

	return 1;
}

//------------------------------------------------------------------------------
int send_boot_cmd(ptc_grp_t *ptc_grp)
{
	int w_cnt = 0;

	while (true) {
		// send boot msg
		if ((w_cnt % 5) == 0) {
			protocol_t s;

			memset(&s, 0, sizeof(protocol_t));
			s.header = '@';
			s.tail   = '#';
			s.status = 'W';
			sprintf(s.msg, "%s", "BOOT");

			info ("send boot cmd = %c\n", s.status);
			send_msg (ptc_grp, &s);
			w_cnt = 0;
		}

		if (ptc_grp->p[0].var.pass) {
			protocol_t *r = (protocol_t *)ptc_grp->p[0].var.arg;;
			ptc_grp->p[0].var.pass = false;
			ptc_grp->p[0].var.open = true;

			if (!strncmp(r->msg, "BOOT", strlen("BOOT")) && (r->status == 'R')) {
				info ("ack msg %s %c\n", r->msg, r->status);
				return 1;
			}
		}
		sleep(1);	w_cnt++;
	}
	return 0;
}

//------------------------------------------------------------------------------
int wait_boot_cmd(ptc_grp_t *ptc_grp)
{
	int w_cnt = 0;
	while (true) {
		if (ptc_grp->p[0].var.pass) {
			protocol_t *r = (protocol_t *)ptc_grp->p[0].var.arg;
			ptc_grp->p[0].var.pass = false;
			ptc_grp->p[0].var.open = true;

			if (!strncmp(r->msg, "BOOT", strlen("BOOT")) && (r->status == 'W')) {
				protocol_t s;
				memset(&s, 0, sizeof(protocol_t));
				info ("boot complete. i'm ready...\n");
				s.header = '@';
				s.tail   = '#';
				s.status = 'R';
				sprintf(s.msg, "%s", "BOOT");
				send_msg (ptc_grp, &s);
				return 1;
			}
		}
		sleep(1);	w_cnt++;
		// if (timeout) return 0;
	}
	return 0;
}

//------------------------------------------------------------------------------
int wait_for_ack (ptc_grp_t *ptc_grp)
{
	while (!ptc_grp->p[0].var.pass)
		usleep(100);
	ptc_grp->p[0].var.pass = false;
	ptc_grp->p[0].var.open = true;
}

//------------------------------------------------------------------------------
#define	ARRARY_SIZE(x)	(sizeof(x) / sizeof(x[0]))

int main(int argc, char **argv)
{
	int fd, speed, wait = 0, p_cnt, i;
	ptc_grp_t *ptc_grp;
	protocol_t recv_msg;
	gpio_bind_t *gb = (gpio_bind_t *)&CON1_TEST_GPIOS[0];

    parse_opts(argc, argv);

	if ((fd = uart_init (OPT_DEVICE_NAME, B115200)) < 0)
		return 0;

	if (!(ptc_grp = ptc_grp_init (fd, 1)))
		return 0;

	if (!ptc_func_init (ptc_grp, 0, sizeof(protocol_t), chk_func, cat_func, &recv_msg))
		goto out;

	if (OPT_SERVER_MODE){
		info ("Server mode enable!!\n");
		if (!wait_boot_cmd(ptc_grp))	goto out;
	}
	else {
		info ("Client mode enable!!\n");
		if (!send_boot_cmd(ptc_grp))	goto out;
	}

	while (true) {
		for (p_cnt = 0; p_cnt < ptc_grp->pcnt; p_cnt++) {
			if (OPT_SERVER_MODE) {
				// send boot msg
				for (i = 0; i < ARRARY_SIZE(CON1_TEST_GPIOS); i++) {
					send_check_gpio(ptc_grp, gb[i].gpio, 1);
					wait_for_ack(ptc_grp);
					//check_gpio...
					send_check_gpio(ptc_grp, gb[i].gpio, 0);
					wait_for_ack(ptc_grp);
					//check_gpio...
				}
				sleep(1);
			} else {
				if (ptc_grp->p[p_cnt].var.pass) {
					protocol_t s;
					protocol_t  *r = (protocol_t  *)ptc_grp->p[0].var.arg;
					gpio_test_t	*g = (gpio_test_t *)r->data;

					// gpio ctrl
					gpio_export (g->gpio);
					gpio_direction (g->gpio, 1);
					gpio_set (g->gpio, g->value);

					// ack send...
					memset (&s, 0, sizeof(protocol_t));
					s.header 	= '@';
					s.tail 		= '#';

					// gpio ctrl check
					if (gpio_export_check (g->gpio) && (gpio_get(g->gpio) == g->value))
						s.status = 'A';
					else
						s.status = 'E';

					memcpy(s.msg, r->msg, sizeof(s.msg));
					send_msg(ptc_grp, &s);

					ptc_grp->p[0].var.pass = false;
					ptc_grp->p[0].var.open = true;
					info ("status = %c, gpio = %d, value = %d\n", s.status, g->gpio, g->value);
				}
				usleep(100);
			}
		}
		fflush(stdout);
	}
out:
	uart_close(fd);
	ptc_grp_close(ptc_grp);
	return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
