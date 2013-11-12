#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <alsa/asoundlib.h>
#include <mpd/client.h>
#include <libnotify/notify.h>

#define COLOR_DARK "#555555"
#define COLOR_DEFAULT "#dddd00"
#define COLOR_CRITICAL "#ff0000"

#define BATLOW 5
#define BATFULL 50

static int is_battery_ok = 1;

static void escape_quotes_nl(char *str) {
    int i, k;
    char *tmp;

    tmp = strdup(str);
    for (i = 0, k = 0; str[i]; ++i) {
        if (str[i] != '"' && str[i] != '\n') {
            tmp[k++] = str[i];
        }
    }

    strncpy(str, tmp, k);
    str[k] = '\0';
    free(tmp);
}

static void print_icon(const char *icon, const char *color, int nocomma) {
    printf("{ \"separator_block_width\" : 0, \"separator\" : false, \"full_text\" : \"\", \"icon\" : \"/usr/share/icons/stlarch_icons/%s.xbm\", \"color\" : \"%s\", \"icon_color\" : \"%s\" }", icon, color, color);
    if (! nocomma) {
        putc(',', stdout);
    }
}

static void print_text(const char *text, const char *color, int nocomma) {
    printf("{ \"separator\" : false, \"full_text\" : \"%s \", \"color\" : \"%s\" }", text, color);
    if (! nocomma) {
        putc(',', stdout);
    }
}

static void print_date() {
    time_t now;
    char datebuf[32];

    now = time(NULL);
    strftime(datebuf, sizeof datebuf, "%d.%m.%Y %H:%M ", localtime(&now));
    print_icon("clock2", COLOR_DEFAULT, 0);
    print_text(datebuf, COLOR_DEFAULT, 1);
}

static void print_battery() {
    FILE *fp;
    int percentage, online;
    char batbuf[3];

    fp = fopen("/sys/class/power_supply/ADP1/online", "r");
    fread(batbuf, 1, sizeof batbuf, fp);
    fclose(fp);

    if (batbuf[0] == '1') {
        online = 1;
    }

    fp = fopen("/sys/class/power_supply/BAT0/capacity", "r");
    fread(batbuf, 1, sizeof batbuf, fp);
    fclose(fp);

    percentage = atoi(batbuf);
    snprintf(batbuf, sizeof batbuf, "%d", percentage);

    if (online) {
        print_icon("ac1", COLOR_DEFAULT, 0);
        print_text(batbuf, COLOR_DEFAULT, 0);
        is_battery_ok = 1;
    }
    else if (percentage <= BATLOW) {
        print_icon("batt1empty", COLOR_CRITICAL, 0);
        print_text(batbuf, COLOR_CRITICAL, 0);

        if (is_battery_ok) {
            NotifyNotification *msg = notify_notification_new("Battery low!", "Battery low!", "dialog-critical");
            notify_notification_show(msg, NULL);
        }

        is_battery_ok = 0;
    }
    else if (percentage < BATFULL) {
        print_icon("batt1half", COLOR_DEFAULT, 0);
        print_text(batbuf, COLOR_DEFAULT, 0);
        is_battery_ok = 1;
    }
    else {
        print_icon("batt1full", COLOR_DEFAULT, 0);
        print_text(batbuf, COLOR_DEFAULT, 0);
        is_battery_ok = 1;
    }
}

static void print_mpd() {
    char title[64];
    struct mpd_connection *conn;
    struct mpd_song *song;
    struct mpd_status *status;

    conn = mpd_connection_new(NULL, 0, 200);
    if (! conn) {
        return;
    }

    song = mpd_run_current_song(conn);
    if (! song) {
        mpd_connection_free(conn);
        return;
    }

    status = mpd_run_status(conn);
    if (! status) {
        mpd_connection_free(conn);
        mpd_song_free(song);
        return;
    }

    snprintf(title, sizeof title, "%s - %s", mpd_song_get_tag(song, MPD_TAG_ARTIST, 0),
        mpd_song_get_tag(song, MPD_TAG_TITLE, 0));
    escape_quotes_nl(title);

    switch (mpd_status_get_state(status)) {
    case MPD_STATE_PLAY:
        print_icon("play2", COLOR_DARK, 0);
        break;
    case MPD_STATE_PAUSE:
        print_icon("pause2", COLOR_DARK, 0);
        break;
    default:
        title[0] = '\0';
    }

    mpd_status_free(status);
    mpd_song_free(song);
    mpd_connection_free(conn);

    print_text(title, COLOR_DARK, 0);
}

static void print_alsa() {
    snd_mixer_elem_t *elem;
    snd_mixer_t *handle;
    snd_mixer_selem_id_t *sid;
    long min, max, volume;
    int unmuted;
    char volbuf[8];

    snd_mixer_open(&handle, 0);
    snd_mixer_attach(handle, "default");
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, "Master");
    elem = snd_mixer_find_selem(handle, sid);
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    snd_mixer_selem_get_playback_volume(elem, 0, &volume);
    snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, &unmuted);
    snd_mixer_close(handle);

    volume = 100 * (volume - min) / (max - min);
    print_icon(unmuted ? "vol1" : "vol3", COLOR_DEFAULT, 0);
    snprintf(volbuf, sizeof volbuf, "%d", volume);
    print_text(volbuf, COLOR_DEFAULT, 0);
}

static void print_line() {
    print_mpd();
    print_alsa();
    print_battery();
    print_date();
}

static void sighandler(int signo) {
}

int main() {
    notify_init("i3status");
    signal(SIGUSR1, &sighandler);
    printf("{\"version\":1}\n [");
    while (1) {
        printf("[");
        print_line();
        printf("],");
        fflush(stdout);
        sleep(1);
    }

    return 0;
}
