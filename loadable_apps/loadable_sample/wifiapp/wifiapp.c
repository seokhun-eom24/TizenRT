/****************************************************************************
 *
 * Copyright 2019 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <tinyara/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#ifdef CONFIG_BINARY_MANAGER
#include <binary_manager/binary_manager.h>
#endif

#include "wifiapp_internal.h"




#include <tinyara/config.h>
#include <tinyara/lcd/lcd_dev.h>
#include <tinyara/rtc.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#define LCD_DEV_PATH "/dev/lcd%d"

#define RED   0xF800
#define WHITE 0xFFFF
#define BLACK 0x0000
#define GREEN 0xE007
#define BLUE  0x00F8
#define SIZE  40

#define COLINDEX 10
#define ROWINDEX 10
#define NOPIXELS 200
static int xres;
static int yres;

#ifdef CONFIG_EXAMPLE_LCD_FPS_TEST
#define EXAMPLE_LCD_FPS_TEST CONFIG_EXAMPLE_LCD_FPS_TEST
#else
#define EXAMPLE_LCD_FPS_TEST 5000
#endif
/****************************************************************************
 * Public Functions
 ****************************************************************************/
static void putarea(int x1, int x2, int y1, int y2, int color)
{
	struct lcddev_area_s area;
	char port[20] = { '\0' };
	int fd = 0;
	int p = 0;
	size_t len;
	len = xres * yres * 2 + 1;
	uint8_t *lcd_data = (uint8_t *)malloc(len);
	if (lcd_data == NULL) {
		printf("malloc failed for lcd data : %d\n", len);
		return;
	}
	sprintf(port, LCD_DEV_PATH, p);
	fd = open(port, O_RDWR | O_SYNC, 0666);
	if (fd < 0) {
		printf("ERROR: Failed to open lcd port : %s error:%d\n", port, fd);
		free(lcd_data);
		return;
	}
	area.planeno = 0;
	area.row_start = x1;
	area.row_end = x2;
	area.col_start = y1;
	area.col_end = y2;
	area.stride = 2 * xres;
	for (int i = 0; i < xres * yres * 2; i += 2) {
		lcd_data[i] = (color & 0xFF00) >> 8;
		lcd_data[i + 1] = color & 0x00FF;
	}

	area.data = lcd_data;
	ioctl(fd, LCDDEVIO_PUTAREA, (unsigned long)(uintptr_t)&area);
	close(fd);
	free(lcd_data);
}

static void test_init(void)
{
	int ret;
	int fd = 0;
	int p = 0;
	char port[20] = { '\0' };
	sprintf(port, LCD_DEV_PATH, p);
	fd = open(port, O_RDWR | O_SYNC, 0666);
	if (fd < 0) {
		printf("ERROR: Failed to open lcd port : %s error:%d\n", port, fd);
		return;
	}
	ioctl(fd, LCDDEVIO_INIT, &ret);
	close(fd);
}

static void test_orientation(void)
{
	int fd = 0;
	int p = 0;
	char port[20] = { '\0' };
	sprintf(port, LCD_DEV_PATH, p);
	fd = open(port, O_RDWR | O_SYNC, 0666);
	if (fd < 0) {
		printf("ERROR: Failed to open lcd port : %s error:%d\n", port, fd);
		return;
	}
	ioctl(fd, LCDDEVIO_SETORIENTATION, LCD_RLANDSCAPE);

	test_put_run();

	sleep(1);
	/* resolution should be swapped now as orientation is changed */
#if defined(CONFIG_LCD_PORTRAIT) || defined(CONFIG_LCD_RPORTRAIT)
	putarea(0, xres - 1, 0, yres - 1, RED);
#else
	putarea(0, yres - 1, 0, xres - 1, RED);
#endif
	/* fill square with side  = OFFSET */
	putarea(0, SIZE, 0, SIZE, WHITE);

	sleep(1);
	/* resetting original orientation - the one defined in config */
#if defined(CONFIG_LCD_PORTRAIT)
	ioctl(fd, LCDDEVIO_SETORIENTATION, LCD_PORTRAIT);
#elif defined(CONFIG_LCD_LANDSCAPE)
	ioctl(fd, LCDDEVIO_SETORIENTATION, LCD_LANDSCAPE);
#elif defined(CONFIG_LCD_RLANDSCAPE)
	ioctl(fd, LCDDEVIO_SETORIENTATION, LCD_RLANDSCAPE);
#elif defined(CONFIG_LCD_RPORTRAIT)
	ioctl(fd, LCDDEVIO_SETORIENTATION, LCD_RPORTRAIT);
#else
	ioctl(fd, LCDDEVIO_SETORIENTATION, LCD_LANDSCAPE);
#endif
	close(fd);
}

static void test_put_area_pattern(void)
{
	int fd = 0;
	int p = 0;
	char port[20] = { '\0' };
	sprintf(port, LCD_DEV_PATH, p);
	fd = open(port, O_RDWR | O_SYNC, 0666);
	if (fd < 0) {
		printf("ERROR: Failed to open lcd port : %s error:%d\n", port, fd);
		return;
	}
	struct fb_videoinfo_s vinfo;
	ioctl(fd, LCDDEVIO_GETVIDEOINFO, (unsigned long)(uintptr_t)&vinfo);
	xres = vinfo.xres;
	yres = vinfo.yres;
	printf("xres : %d, yres:%d\n", xres, yres);
	close(fd);
	putarea(0, yres - 1, 0, xres - 1, BLUE);
	sleep(3);
	putarea(0, yres - 1, 0, xres - 1, GREEN);
	sleep(3);
	putarea(0, yres - 1, 0, xres - 1, RED);
	sleep(3);
	putarea(0, yres - 1, 0, xres - 1, BLACK);
	sleep(3);
	putarea(0, yres - 1, 0, xres - 1, WHITE);
	sleep(3);
}

static unsigned short generate_color_code(int red, int green, int blue)
{
	// Ensure that RGB values are within the valid range (0-31)
	red = red % 31;
	green = green % 31;
	blue = blue % 31;
	// Combine RGB components into a 16-bit hex color code
	unsigned short colorCode = (red << 11) | (green << 5) | blue;
	return colorCode;
}

static void test_bit_map(void)
{
	int fd = 0;
	int p = 0;
	char port[20] = { '\0' };
	struct lcddev_area_s area;
	size_t len;
	int idx = 0;
	sprintf(port, LCD_DEV_PATH, p);
	fd = open(port, O_RDWR | O_SYNC, 0666);
	if (fd < 0) {
		printf("ERROR: Failed to open lcd port : %s error:%d\n", port, fd);
		return;
	}
	struct fb_videoinfo_s vinfo;
	ioctl(fd, LCDDEVIO_GETVIDEOINFO, (unsigned long)(uintptr_t)&vinfo);
	xres = vinfo.xres;
	yres = vinfo.yres;
	len = xres * yres * 2 + 1;
        uint8_t *lcd_data = (uint8_t *)malloc(len);
        if (lcd_data == NULL) {
                printf("malloc failed for lcd data : %d\n", len);
		close(fd);
                return;
        }
	area.planeno = 0;
	area.row_start = 0;
	area.row_end = yres - 1;
	area.col_start = 0;
	area.col_end = xres - 1;
	area.stride = 2 * xres;
	area.data = lcd_data;
	uint16_t color;
	for (int y = 0; y < yres / SIZE * 2; y++) {
		for (int x = 0; x < xres / SIZE; x++) {
			color = generate_color_code(rand() % 31, rand() % 31, rand() % 31);
			for (int i = 0; i < SIZE; i++) {
				for (int j = 0; j < SIZE; j++) {
					int pixel_x = x * SIZE + i;
					int pixel_y = y * SIZE + j;
					lcd_data[pixel_y * xres + pixel_x] = (color & 0xFF00) >> 8;
				}
			}
		}
	}
	ioctl(fd, LCDDEVIO_PUTAREA, (unsigned long)(uintptr_t)&area);
	close(fd);
	free(lcd_data);
}

static void test_quad(void)
{
	int fd = 0;
	int p = 0;
	char port[20] = {'\0'};
	struct lcddev_area_s area;
	size_t len;
	snprintf(port, sizeof(port) / sizeof(port[0]), LCD_DEV_PATH, p);
	fd = open(port, O_RDWR | O_SYNC, 0666);
	if (fd < 0) {
		printf("ERROR: Failed to open lcd port : %s error:%d\n", port, fd);
		return;
	}
	struct fb_videoinfo_s vinfo;
	ioctl(fd, LCDDEVIO_GETVIDEOINFO, (unsigned long)(uintptr_t)&vinfo);
	xres = vinfo.xres;
	yres = vinfo.yres;
	len = xres * yres * 2 + 1;
	uint8_t *lcd_data = (uint8_t *)malloc(len);
	if (lcd_data == NULL) {
		printf("malloc failed for lcd data : %d\n", len);
		close(fd);
		return;
	}
	area.planeno = 0;
	area.row_start = 0;
	area.row_end = yres - 1;
	area.col_start = 0;
	area.col_end = xres - 1;
	area.stride = 2 * xres;
	area.data = lcd_data;
	int pixel_index = 0;
	uint16_t color;

	for (int y = 0; y < yres; y++) {
		for (int x = 0; x < xres; x++) {
			pixel_index = ((y * xres) + x) * 2;
			if (x < xres / 2) {
				if (y < yres / 2) {
					color = RED;
				} else {
					color = BLUE;
				}
			} else {
				if (y < yres / 2) {
					color = GREEN;
				} else {
					color = WHITE;
				}
			}
			lcd_data[pixel_index] = (color & 0xFF00) >> 8;
			lcd_data[pixel_index + 1] = color & 0x00FF;
		}
	}
	ioctl(fd, LCDDEVIO_PUTAREA, (unsigned long)(uintptr_t)&area);
	close(fd);
	free(lcd_data);
}

static void test_fps(void)
{
	int fd_rtc = 0;
	int fd_lcd = 0;
	int p = 0;
	char port[20] = { '\0' };
	size_t len;

	fd_rtc = open("/dev/rtc0", O_RDWR);
	if (fd_rtc < 0) {
		printf("ERROR: LCD FPS test, Fail to open rtc.\n");
		return;
	}

	len = xres * yres * 2 + 1;
	uint8_t *lcd_data_red = (uint8_t *)malloc(len);
	if (lcd_data_red == NULL) {
		printf("FPS TEST, malloc failed for lcd data red : %d\n", len);
		close(fd_rtc);
		return;
	}

	uint8_t *lcd_data_blue = (uint8_t *)malloc(len);
	if (lcd_data_blue == NULL) {
		printf("FPS TEST, malloc failed for lcd data blue: %d\n", len);
		free(lcd_data_red);
		close(fd_rtc);
		return;
	}
	for (int i = 0; i < len - 1; i += 2) {
		lcd_data_red[i] = (RED & 0xFF00) >> 8;
		lcd_data_red[i + 1] = RED & 0x00FF;
		lcd_data_blue[i] = (BLUE & 0xFF00) >> 8;
		lcd_data_blue[i + 1] = BLUE & 0x00FF;
	}

	struct lcddev_area_s area_red;
	struct lcddev_area_s area_blue;
	area_red.planeno = 0;
	area_red.row_start = 0;
	area_red.row_end = yres - 1;
	area_red.col_start = 0;
	area_red.col_end = xres - 1;
	area_red.stride = 2 * xres;
	area_red.data = lcd_data_red;
	area_blue.planeno = 0;
	area_blue.row_start = 0;
	area_blue.row_end = yres - 1;
	area_blue.col_start = 0;
	area_blue.col_end = xres - 1;
	area_blue.stride = 2 * xres;
	area_blue.data = lcd_data_blue;

	sprintf(port, LCD_DEV_PATH, p);
	fd_lcd = open(port, O_RDWR | O_SYNC, 0666);
	if (fd_lcd < 0) {
		printf("ERROR: FPS TEST, Failed to open lcd port : %s error:%d\n", port, fd_lcd);
		free(lcd_data_red);
		free(lcd_data_blue);
		close(fd_rtc);
		return;
	}

	struct rtc_time start_time = RTC_TIME_INITIALIZER(1970, 1, 1, 0, 0, 0);
	struct rtc_time end_time;

	bool is_red = true;
	//Start Test
	ioctl(fd_rtc, RTC_RD_TIME, (unsigned long)&start_time);
	for (int itr = 0; itr < EXAMPLE_LCD_FPS_TEST; itr++) {
		if (is_red) {
			ioctl(fd_lcd, LCDDEVIO_PUTAREA, (unsigned long)(uintptr_t)&area_red);
			is_red = false;
		} else {
			ioctl(fd_lcd, LCDDEVIO_PUTAREA, (unsigned long)(uintptr_t)&area_blue);
			is_red = true;
		}
	}
	ioctl(fd_rtc, RTC_RD_TIME, (unsigned long)&end_time);
	//End test
	
	close(fd_rtc);
	close(fd_lcd);
	free(lcd_data_red);
	free(lcd_data_blue);

	time_t start;
	time_t end;
	start = mktime((FAR struct tm *)&start_time);
	end = mktime((FAR struct tm *)&end_time);

	int time_elapsed = difftime(end, start);
	if (time_elapsed != 0) {
		float fps = EXAMPLE_LCD_FPS_TEST / time_elapsed;
		printf("FPS Test: %d frames executed in %d sec, FPS: %.2f\n", EXAMPLE_LCD_FPS_TEST, time_elapsed, fps);
		return;
	} else {
		printf("FPS calculation failed! Please increase the number of frames execution using CONFIG_EXAMPLE_LCD_FPS_TEST!\n");
	}
}

bool is_valid_power(char *power)
{
	int power_val_size = strlen(power);

	if (power_val_size < 1 || power_val_size > 3) {		/* Length of Power val should be 1, 2, or 3 */
		return false;
	}
	for (int i = 0; i < power_val_size; i++) {
		if (!isdigit(power[i])) {	/* If not a digit */
			return false;
		}
	}
	return true;
}

static void display_test_scenario(void)
{
	printf("\nSelect Test Scenario.\n");
#ifdef CONFIG_EXAMPLES_MESSAGING_TEST
	printf("\t-Press M or m : Messaging F/W Test\n");	
#endif
#ifdef CONFIG_EXAMPLES_RECOVERY_TEST
	printf("\t-Press R or r : Recovery Test\n");
#endif
#ifdef CONFIG_EXAMPLES_BINARY_UPDATE_TEST
	printf("\t-Press U or u : Binary Update Test\n");
#endif
	printf("\t-Press X or x : Terminate Tests.\n");
}

extern int preapp_start(int argc, char **argv);

#ifdef CONFIG_EXAMPLES_SMARTFS_POWERCUT
extern int smartfs_powercut_main(int argc, char *argv[]);
#endif

#ifdef CONFIG_APP_BINARY_SEPARATION
int main(int argc, char **argv)
#else
int wifiapp_main(int argc, char **argv)
#endif
{
#ifdef CONFIG_EXAMPLES_LOADABLE_MANUAL_TEST
	int ch;
	bool is_testing = true;
#endif

#if defined(CONFIG_SYSTEM_PREAPP_INIT) && defined(CONFIG_APP_BINARY_SEPARATION)
	preapp_start(argc, argv);
#endif

	printf("This is WIFI App\n");

#if defined(CONFIG_BINARY_MANAGER) && !defined(CONFIG_EXAMPLES_MICOM_TIMER_TEST)
	int ret;
	ret = binary_manager_notify_binary_started();
	if (ret < 0) {
		printf("WIFI notify 'START' state FAIL\n");
	}
#endif

#ifdef CONFIG_EXAMPLES_SMARTFS_POWERCUT
	smartfs_powercut_main(0, NULL);
#endif

#ifdef CONFIG_EXAMPLES_LOADABLE_MANUAL_TEST
	while (is_testing) {
		display_test_scenario();
		ch = 'X';
		switch (ch) {
#ifdef CONFIG_EXAMPLES_MESSAGING_TEST
		case 'M':
		case 'm':
			messaging_test();
			break;
#endif
#ifdef CONFIG_EXAMPLES_RECOVERY_TEST
		case 'R':
		case 'r':
			recovery_test();
			is_testing = false;
			break;
#endif
#ifdef CONFIG_EXAMPLES_BINARY_UPDATE_TEST
		case 'U':
		case 'u':
			binary_update_test();
			break;
#endif
		case 'X':
		case 'x':
			printf("Test will be finished.\n");
			is_testing = false;
			break;
		default:
			printf("Invalid Scenario.\n");
			break;
		}
	}

#elif defined(CONFIG_EXAMPLES_LOADABLE_AUTOMATIC_TEST)
#ifdef CONFIG_EXAMPLES_RECOVERY_AGING_TEST
	recovery_test();
#elif defined(CONFIG_EXAMPLES_UPDATE_AGING_TEST)
	binary_update_aging_test();
#endif
#endif

	while (1) {
		int count = 0;
		int fd = 0;
		int p = 0;
		char port[20] = { '\0' };

		sprintf(port, LCD_DEV_PATH, p);
		fd = open(port, O_RDWR | O_SYNC, 0666);
		if (fd < 0) {
			printf("ERROR: Failed to open lcd port : %s error:%d\n", port, fd);
			return ERROR;	
		}

		/* LCD Power test */
		if (argc >= 2 && !strncmp(argv[1], "power", 5)) {
			if (argc > 2 && is_valid_power(argv[2])) {
				ioctl(fd, LCDDEVIO_SETPOWER, atoi(argv[2]));
			} else {
				printf("ERROR: Value of power should be int in range [0, 100]\n");
				printf("Usage: lcd_test power <value>\n");
				printf("0 --> LCD Power OFF\n");
				printf("100 --> LCD Power ON\n");
			}
			close(fd);
			return OK;
		}

		while (count < 5) {
			test_put_area_pattern();
			test_quad();
			sleep(3);
			test_bit_map();
			sleep(3);
			ioctl(fd, LCDDEVIO_SETPOWER, 0);
			sleep(15);
			ioctl(fd, LCDDEVIO_SETPOWER, 100);
			count++;
			printf("count :%d\n", count);
		}
		// test_fps();
		close(fd);
		sleep(5);
		// printf("[%d] WIFI ALIVE\n", getpid());
	}
	return 0;
}
