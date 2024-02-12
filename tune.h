// test stubs imiplemented in impedance.c
extern void delay_ms(int ms);
extern void get_SWR();
extern void Relay_set(char l, char c, char i);

void tune();

extern int SWR;
extern char ind, cap, SW;