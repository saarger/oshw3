
#include <linux/ioctl.h>
#define MAJOR_NUMBER 235
#define BUFFER_SIZE 128
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUMBER, 0, unsigned long)
