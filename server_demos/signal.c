
#include <stdio.h>
#include <signal.h>
static int glb_daemon_exit = 0;

static void rtsp_daemon_signal_handler(int signum);

static void rtsp_daemon_signal_handler(int signum)
{
    if (signum == SIGINT) {
        fprintf(stderr, "Signal SIGINT caught, rtsp daemon exit....\n");
        glb_daemon_exit = 1;
    } else {
        fprintf(stderr, "Invalid signal %d caught.\n", signum);
    }
}

int main(int argc, char *argv[])
{
	signal(SIGINT , rtsp_daemon_signal_handler);
	glb_daemon_exit = 0;

	while (!glb_daemon_exit) {
		sleep(1);
	}
}
