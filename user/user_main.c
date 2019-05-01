/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2016 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "ets_sys.h"
#include "osapi.h"
#include "mem.h"
#include "user_interface.h"

#include "board.h"
#include "key.h"
#include "smartconfig_api.h"
#include "uart_trans.h"
#include "net.h"
#include "timer.h"


bool is_in_smartconfig = false;
bool last_is_key_long_press = false;
os_timer_t main_timer;
uint32_t smartconfig_start_time;

bool is_first_connect=true;

void ICACHE_FLASH_ATTR send_mac(void)
{
	uint8_t mac[6];
	wifi_get_macaddr(STATION_IF, mac);
	os_printf("%d macaddr:%x %x %x %x %x %x\n"
									,mac[0]
									,mac[1]
									,mac[2]
									,mac[3]
									,mac[4]
									,mac[5]
									);

	uint8_t buf[]={0x46, 0xB9, 0x6A, 0x00, 0x0C, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xF1, 0x68, 0xC6, 0x3A, 0xA8, 0x7D, 0x2C, 0x08, 0x1C, 0x16};
	for(uint8_t i=0; i<6; i++)
	{
		buf[11+i] = mac[i];
	}
	uint16_t sum=0;
	for(uint8_t i=0; i<sizeof(buf)-2-3; i++)
	{
		sum += buf[2+i];
	}
	buf[sizeof(buf)-3] = (uint8_t)(sum>>8);
	buf[sizeof(buf)-2] = (uint8_t)(sum&0xFF);
	// os_printf("send:");
	// for(uint8_t i=0; i<sizeof(buf); i++)
	// {
	// 	os_printf("%02x ", buf[i]);
	// }
	// os_printf("\n");
	uart_trans_send(buf, sizeof(buf));	
}

void ICACHE_FLASH_ATTR net_connect_callback(void* param)
{
	if(is_first_connect) {
		send_mac();
		is_first_connect = false;
	}
}

void ICACHE_FLASH_ATTR smartconfig_callback(void* param)
{
	is_in_smartconfig = false;	
}

void ICACHE_FLASH_ATTR key_func(void)
{
	bool is_key_long_press;

	key_update();

	is_key_long_press = key_is_long_press();
	if(is_key_long_press != last_is_key_long_press) {
		if(is_key_long_press) {
			if(is_in_smartconfig) {
				if(smartconfig_end() == true) {
					is_in_smartconfig = false;	
					wifi_station_connect();
				}
			} else {
				is_first_connect=true;
				if(smartconfig_begin(smartconfig_callback) == true) {
					is_in_smartconfig = true;
					smartconfig_start_time = system_get_time();
				}
			}
		}	
	}
	last_is_key_long_press = is_key_long_press;
}

void ICACHE_FLASH_ATTR net_func(void)
{
	net_update();
	if(is_in_smartconfig) {
		if(system_get_time() - smartconfig_start_time > (1000*SMARTCONFIG_TIMEOUT_MS)) {
			os_printf("smartconfig timeout\n");
			if(smartconfig_end() == true) {
				is_in_smartconfig = false;
				wifi_station_connect();
			}
		}
	}
	// TIMER_DEF(net_send_time)
	// if(timer_check(&net_send_time, 1000*1000)) {
	// 	//u8 data[] = {1,2,3,4,5};
	// 	u8 mavlink_heart[] = {0xfe,0x09,0x11,0x02,0x01,0x00,0x00,0x00,0x00,0x00,0x02,0x0c,0x00,0x03,0x03,0xe3,0x54};
	// 	net_send(mavlink_heart, sizeof(mavlink_heart));
	// }
}

void uart_func(void)
{
	uart_trans_update();
}

void ICACHE_FLASH_ATTR main_func(void* arvg)
{
//	key_func();
	net_func();
	uart_func();
}

void ICACHE_FLASH_ATTR user_init(void)
{
	uart_trans_init();	
    os_printf("SDK version:%s\n", system_get_sdk_version());

	wifi_set_opmode(0x01);
	timer_init();
	key_init();
	net_init(net_connect_callback);

	os_timer_setfn(&main_timer, main_func, NULL);
	os_timer_arm(&main_timer, MAIN_LOOP_MS, 1);	
}
