#ifndef _MOXA_MCU_H_
#define _MOXA_MCU_H_

#define MCUD_SOCKET "/tmp/mcud_socket"
#define MCU_PORT "/dev/ttyS2"
#define BUFF_LEN 16
#define MAXLINE 128
#define WAKE_UP_EXE "/sbin/mx-power-mgmt -w"
#define TIMEOUT 1

#define SET_ACTIVE_MODE		"\xf0\x02\xc2\xa0\x62"
#define SET_SLEEP_MODE		"\xf0\x02\xc2\xa1\x63"
#define SET_HIBERNATE_MODE	"\xf0\x02\xc2\xa2\x64"

#define SET_RED_LED_OFF		"\xf0\x02\xc3\x00\xc3"
#define SET_RED_LED_ON		"\xf0\x02\xc3\x01\xc4"
#define SET_GREEN_LED_OFF	"\xf0\x02\xc3\x00\xc3"
#define SET_GREEN_LED_ON	"\xf0\x02\xc3\x02\xc5"

#define GET_MCU_A_VERSION	"\xf0\x01\xc4\xc4"
#define GET_MCU_B_VERSION	"\xf0\x01\xc5\xc5"

#define GET_WAKEUP_MODE		"\xf0\x01\xc6\xc6"

enum ERR_READ_CMD {
	E_SUCCESS = 0,
	E_SYSTEM_ERR = -1,
	E_TIMEOUT = -2,
	E_UNKNOWN_HEADER = -3,
	E_CMD_NOT_VALID = -4,
	E_AGAIN = -5
};

#endif /* _MOXA_MCU_H_ */
