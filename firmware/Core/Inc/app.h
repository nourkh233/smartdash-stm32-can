#pragma once
#define NODE_A 0   /* 1 = carte capteurs (émet sur le CAN) ; 0 = carte passerelle (CAN -> PC) */
void app_init(void);
void app_loop(void);
