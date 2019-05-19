#include "user_interface.h"
#include "osapi.h"
#include "user_interface.h"
#include "espconn.h"
#include "ip_addr.h"
#include "mem.h"

#include "board.h"
#include "uart_trans.h"
#include "net.h"


struct espconn server_conn;
const char server_ip[4] = SERVER_IP;
bool is_wifi_connect=false;
uint32_t wifi_check_timer; 
uint32_t net_check_timer; 
static net_conn_cb* net_connect_cb;
bool is_net_creat = false;

bool net_is_creat(void)
{
	return is_net_creat;
}


void ICACHE_FLASH_ATTR net_send(uint8_t* pdata, uint8_t len)
{
	if(is_net_creat) {
		// os_printf("[tcp][send]");
		// for(uint16_t i=0; i<len; i++) {
		// 	os_printf("%02x ", pdata[i]);
		// }
		// os_printf("\n");	
		memcpy(server_conn.proto.udp->remote_ip, server_ip, 4);
		server_conn.proto.udp->remote_port = SERVER_PORT;  	
		s16 ret = espconn_send(&server_conn, pdata, len);
		//os_printf("send ret:%d\n", ret);	
	}
}

void ICACHE_FLASH_ATTR tcp_recv_cb(void *arg, char *pdata, unsigned short len) 
{
	os_printf("[tcp][recv]");
	for(uint16_t i=0; i<len; i++) {
		os_printf("%02x ", pdata[i]);
	}
	os_printf("\n");

	uart_trans_send(pdata, len);
}

void ICACHE_FLASH_ATTR tcp_send_cb(void *arg)  
{
	//os_printf("tcp send success!\n");
}

void ICACHE_FLASH_ATTR tcp_disconnect_cb(void *arg)  
{
	os_printf("tcp disconnect success!\n");
}

void ICACHE_FLASH_ATTR server_connect_cb(void *arg)  
{
	struct espconn *conn = arg;
	espconn_regist_recvcb(conn, tcp_recv_cb); 
	espconn_regist_sentcb(conn, tcp_send_cb);  
	espconn_regist_disconcb(conn, tcp_disconnect_cb);

	if(net_connect_cb != NULL) {
		net_connect_cb(NULL);
	}

	// uint8_t mac[6];
	// wifi_get_macaddr(STATION_IF, mac);
	// os_printf("%d macaddr:%x %x %x %x %x %x\n"
	// 								,mac[0]
	// 								,mac[1]
	// 								,mac[2]
	// 								,mac[3]
	// 								,mac[4]
	// 								,mac[5]
	// 								);

	// uint8_t buf[]={0x46,0xB9,0x68,0x00,0x0C,0x00,0xF0,0x00,0x00,0x0E,0x01,0x84,0xf3,0xeb,0x74,0x02,0x79,0x04,0xc5,0x16};
	// for(uint8_t i=0; i<6; i++)
	// {
	// 	buf[11+i] = mac[i];
	// }
	// uint16_t sum=0;
	// for(uint8_t i=0; i<15; i++)
	// {
	// 	sum += buf[2+i];
	// }
	// buf[17] = (uint8_t)(sum>>8);
	// buf[18] = (uint8_t)(sum&0xFF);
	// os_printf("send:");
	// for(uint8_t i=0; i<sizeof(buf); i++)
	// {
	// 	os_printf("%02x ", buf[i]);
	// }
	// os_printf("\n");
	// espconn_sent(conn, buf, sizeof(buf));
	
	os_printf("net connect success!\n");
}

void ICACHE_FLASH_ATTR tcp_reconnect_cb(void *arg, sint8 err) 
{
	os_printf("connect err:%d!\n", err);
	espconn_connect((struct espconn *) arg);
}

void ICACHE_FLASH_ATTR net_connect(void) 
{
	struct ip_info local_ip;

	os_printf("net connect...\n");
	wifi_set_broadcast_if(STATION_MODE);
	server_conn.proto.udp = (esp_udp *) os_zalloc(sizeof(esp_udp));  
	server_conn.type = ESPCONN_UDP; 
	memcpy(server_conn.proto.udp->remote_ip, server_ip, 4);
	server_conn.proto.udp->remote_port = SERVER_PORT;  

	wifi_get_ip_info(STATION_IF, &local_ip);
//	os_printf("local ip: %d.%d.%d.%d", local_ip[0],local_ip[1],local_ip[2],local_ip[3]);
	memcpy(server_conn.proto.udp->local_ip, &local_ip, 4);
	os_printf("set local ip: %d.%d.%d.%d\n", server_conn.proto.udp->local_ip[0],
	                                       server_conn.proto.udp->local_ip[1],
										   server_conn.proto.udp->local_ip[2],
										   server_conn.proto.udp->local_ip[3]);
	server_conn.proto.tcp->local_port = LOCAL_PORT;  

	s8 ret = espconn_create(&server_conn);
	os_printf("udp creat ret:%d\n", ret);
	if(ret==0) {
		is_net_creat=true;
		espconn_regist_recvcb(&server_conn, tcp_recv_cb); 
		espconn_regist_sentcb(&server_conn, tcp_send_cb);  
	} 
}

void ICACHE_FLASH_ATTR net_update(void)
{
	if(system_get_time() - wifi_check_timer > (1000*WIFI_CHECK_MS)) {
		if(wifi_station_get_connect_status() == STATION_GOT_IP) {
			if(!is_wifi_connect) {
				os_printf("wifi connect ok!\n");
			}
			is_wifi_connect = true;
		} else {
			is_wifi_connect = false;
		}
		wifi_check_timer = system_get_time();
	}

	if(system_get_time() - net_check_timer > (NET_CHECK_MS*1000)) {
		//os_printf("net state:%d\n", server_conn.state);
		if(!net_is_creat() && is_wifi_connect) {
			net_connect();
		}
		net_check_timer = system_get_time();
	}
}

void ICACHE_FLASH_ATTR net_init(net_conn_cb* cb)
{
	net_connect_cb = cb;
}
