#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <rtinfo.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void color_reset(WINDOW *root) {
    wattrset(root, COLOR_PAIR(1));
}

void color_set_white(WINDOW *root) {
    wattrset(root, A_BOLD | COLOR_PAIR(1));
}

void color_set_blue(WINDOW *root) {
    wattrset(root, A_BOLD | COLOR_PAIR(2));
}

void color_set_yellow(WINDOW *root) {
    wattrset(root, A_BOLD | COLOR_PAIR(3));
}

void color_set_red(WINDOW *root) {
    wattrset(root, A_BOLD | COLOR_PAIR(4));
}

void color_set_black(WINDOW *root) {
    wattrset(root, A_BOLD | COLOR_PAIR(5));
}

void color_set_cyan(WINDOW *root) {
    wattrset(root, A_BOLD | COLOR_PAIR(6));
}

void color_set_green(WINDOW *root) {
    wattrset(root, A_BOLD | COLOR_PAIR(7));
}

void color_set_magenta(WINDOW *root) {
    wattrset(root, A_BOLD | COLOR_PAIR(8));
}

typedef struct rtdata_t {
    char hostname[32];

    rtinfo_cpu_t *cpu;
    rtinfo_network_t *net;
    rtinfo_disk_t *disk;
    rtinfo_memory_t memory;
    rtinfo_loadagv_t loadavg;
    rtinfo_temp_cpu_t temp_cpu;
    rtinfo_uptime_t uptime;
    struct tm * ti; // timeinfo
    uint64_t timestamp;

} rtdata_t;

void diep(char *str) {
    perror(str);
    exit(EXIT_FAILURE);
}

void sysupdate_summary(WINDOW *root, rtdata_t *rtdata) {
    int height, width;
    getmaxyx(root, height, width);

    color_set_magenta(root);
    wmove(root, 1, width - 10);
    wprintw(root, "%02d:%02d:%02d\n", rtdata->ti->tm_hour, rtdata->ti->tm_min, rtdata->ti->tm_sec);
    color_reset(root);

    wmove(root, 2, 2);

    color_set_cyan(root);
    wprintw(root, "Zero-OS");

    color_reset(root);
    wprintw(root, " version ");

    color_set_yellow(root);
    wprintw(root, "release-threefold-edge.nodes-0001 (1a2b3c4d)");

    wmove(root, 3, 2);
    color_reset(root);
    wprintw(root, "This node contains ");

    color_set_cyan(root);
    wprintw(root, "%d CPUs", rtdata->cpu->nbcpu - 1);

    color_reset(root);
    wprintw(root, " and ");

    color_set_cyan(root);
    wprintw(root, "%llu GB RAM", rtdata->memory.ram_total / (1024 * 1024));
    clrtoeol();

    color_reset(root);
}

void sysupdate_overall_usage(WINDOW *root, rtdata_t *rtdata) {
    float rampc = ((double) rtdata->memory.ram_used / rtdata->memory.ram_total) * 100.0;

    wmove(root, 5, 2);

    wprintw(root, "System load: ");

    for(int i = 0; i < 3; i++) {
        color_set_blue(root);
        wprintw(root, "%.2f", rtdata->loadavg.load[i]);
        color_reset(root);
        wprintw(root, ", ");
    }

    color_reset(root);

    wprintw(root, "CPU: ");
    color_set_blue(root);
    wprintw(root, "% 3d%%", rtdata->cpu->dev[0].usage);
    color_reset(root);

    wprintw(root, ", RAM: ");
    color_set_blue(root);
    wprintw(root, "% 3.0f%% (%llu MB)", rampc, rtdata->memory.ram_used / 1024);
    clrtoeol();
    color_reset(root);

    color_reset(root);
}

void sysupdate_core_usage(WINDOW *root, rtdata_t *rtdata) {
    wmove(root, 7, 2);
    wprintw(root, "Containers running: ");
    color_set_cyan(root);
    wprintw(root, "0");
    color_reset(root);

    wmove(root, 8, 2);
    wprintw(root, "Virtual Machines running: ");
    color_set_cyan(root);
    wprintw(root, "0");
    color_reset(root);
}

int file_read(char *filename, char *buffer, size_t bufflen) {
    ssize_t readval;
    int fd;

    if((fd = open(filename, O_RDONLY)) < 0) {
        perror(filename);
        return 1;
    }

    if((readval = read(fd, buffer, bufflen)) < 0) {
        perror(filename);
        close(fd);
        return 1;
    }

    buffer[readval - 1] = '\0';
    close(fd);

    return 0;
}

void sysupdate_network_macaddr(WINDOW *root, char *interface) {
    char filename[PATH_MAX];
    char buffer[512];

    sprintf(filename, "/sys/class/net/%s/address", interface);

    if(file_read(filename, buffer, sizeof(buffer)))
        return;

    wprintw(root, "%s", buffer);
}

void sysupdate_network_usage(WINDOW *root, rtdata_t *rtdata) {
    wmove(root, 10, 2);
    wprintw(root, "Internet: ");
    color_set_green(root);
    wprintw(root, "connected");
    color_reset(root);

    wmove(root, 11, 2);
    wprintw(root, "Zerotier: ");
    color_set_cyan(root);
    wprintw(root, "ab12cd34");
    color_reset(root);
    wprintw(root, ", ");
    color_set_red(root);
    wprintw(root, "connecting");
    color_reset(root);

    wmove(root, 13, 2);
    wprintw(root, "System network: ");

    for(int i = 0; i < rtdata->net->nbiface; i++) {
        float kbps = ((float) rtdata->net->net[i].up_rate + rtdata->net->net[i].down_rate) / 1024;

        wmove(root, 14 + i, 4);
        wprintw(root, "%-15s: ", rtdata->net->net[i].name);
        sysupdate_network_macaddr(root, rtdata->net->net[i].name);
        wprintw(root, ", %-15s [%.2f KB/s]", rtdata->net->net[i].ip, kbps);
        clrtoeol();
    }
}

void sysupdate(WINDOW *root, rtdata_t *rtdata) {
    sysupdate_summary(root, rtdata);
    sysupdate_overall_usage(root, rtdata);
    sysupdate_core_usage(root, rtdata);
    sysupdate_network_usage(root, rtdata);
}

void rtdata_init(rtdata_t *rtdata) {
    // pre-initialize everything to zero
    memset(rtdata, 0, sizeof(rtdata_t));

    rtdata->cpu = rtinfo_init_cpu();
    rtdata->net = rtinfo_init_network();

	if(gethostname(rtdata->hostname, sizeof(rtdata->hostname)))
		diep("gethostname");

    rtinfo_get_cpu(rtdata->cpu);
    rtinfo_get_network(rtdata->net);
    rtdata->ti = rtinfo_get_time();
}

int rtdata_update(rtdata_t *rtdata) {
    rtinfo_get_cpu(rtdata->cpu);
    rtinfo_mk_cpu_usage(rtdata->cpu);

    rtinfo_get_network(rtdata->net);
    rtinfo_mk_network_usage(rtdata->net, 1000);

    rtdata->ti = rtinfo_get_time();

    if(!rtinfo_get_memory(&rtdata->memory))
        return 1;

    if(!rtinfo_get_loadavg(&rtdata->loadavg))
        return 1;

    if(!rtinfo_get_uptime(&rtdata->uptime))
        return 1;

    return 0;
}

int main(void) {
    WINDOW *screen;
    rtdata_t rtdata;

    rtdata_init(&rtdata);
    rtdata_update(&rtdata);

    screen = initscr();

    clear();
    noecho();
    cbreak();
    timeout(0);
    start_color();
    use_default_colors();
    curs_set(0);

    init_pair(1, COLOR_WHITE, -1);
    init_pair(2, COLOR_BLUE, -1);
    init_pair(3, COLOR_YELLOW, -1);
    init_pair(4, COLOR_RED, -1);
    init_pair(5, COLOR_BLACK, -1);
    init_pair(6, COLOR_CYAN, -1);
    init_pair(7, COLOR_GREEN, -1);
    init_pair(8, COLOR_MAGENTA, -1);
    init_pair(9, COLOR_WHITE, -1);


    wrefresh(screen);

    while(1) {
        sysupdate(screen, &rtdata);
        box(screen, 0, 0);
        wrefresh(screen);

        usleep(1000 * 1000);
        rtdata_update(&rtdata);
    }


    endwin();

    return 0;
}
