
struct serial_state_t {
	int fd;
};

extern struct serial_state_t *serial_init();
extern int serial_write(struct serial_state_t *state, char *s, int len);
extern int serial_close(struct serial_state_t *state);


