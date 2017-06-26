

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <ctype.h>
#include <dirent.h>
#include <locale.h>
#include <langinfo.h>
#ifdef HAVE_ICONV
#include <iconv.h>
#endif
#include <signal.h>
#include <stdarg.h>
#include <math.h>
#include <sys/time.h>

#if defined(__sun__) || defined(__CYGWIN__)
#include <termios.h>
#include <ncurses.h>
#else
#include <curses.h>
#endif

char *tgetstr(const char *id, char **area);
char *tgoto(const char *cap, int col, int row);

volatile sig_atomic_t cmus_running = 1;
int ui_initialized = 0;
enum ui_input_mode input_mode = NORMAL_MODE;
int cur_view = TREE_VIEW;
int prev_view = -1;
struct searchable *searchable;
char *lib_filename = NULL;
char *lib_ext_filename = NULL;
char *play_queue_filename = NULL;
char *play_queue_ext_filename = NULL;
char *charset = NULL;
int using_utf8 = 0;

static char *lib_autosave_filename;
static char *play_queue_autosave_filename;

static char error_buf[512];
static time_t error_time = 0;
static int msg_is_error;
static int error_count = 0;

static char *server_address = NULL;

static char print_buffer[512];
static char conv_buffer[512];

static char conv_buffer[512];

#define print_buffer_max_width (sizeof(print_buffer) / 4 - 1)
static int client_fd = -1;
static const char *t_ts;
static const char *t_fs;

static int tree_win_x = 0;
static int tree_win_w = 0;

static int track_win_x = 0;
static int track_win_w = 0;

static int editable_win_x = 0;
static int editable_win_w = 0;
static int editable_active = 1;

static int show_cousor;
static int cursor_x;
static int cursor_y;

static const int default_esc_delay = 25;

static char *title_buf = NULL;

enum {
	CURSED_WIN,
	CURSED_WIN_CUR,
	CURSED_WIN_SEL,
	CURSED_WIN_SEL_CUR,

	CURSED_WIN_ACTIVE,
	CURSED_WIN_ACTIVE_CUR,
	CURSED_WIN_ACTIVE_SEL,
	CURSED_WIN_ACTIVE_SEL_CUR,

	CURSED_SEPARATOR,
	CURSED_WIN_TITLE,
	CURSED_COMMANDLINE,
	CURSED_STATUSLINE,

	CURSED_TITLELINE,
	CURSED_DIR,
	CURSED_ERROR,
	CURSED_INFO,

	CURSED_TRACKWIN_ALBUM,

	NR_CURSED
};

static unsigned char cursed_to_bg_idx[NR_CURSED] = {
	COLOR_WIN_BG,
	COLOR_WIN_BG,
	COLOR_WIN_INACTIVE_SEL_BG,
	COLOR_WIN_INACRIVE_CUR_SEL_BG,
	
	COLOR_WIN_BG,
	COLOR_WIN_BG,
	COLOR_WIN_SEL_BG,
	COLOR_WIN_CUR_SEL_BG,

	COLOR_WIN_BG,
	COLOR_WIN_TITLE_BG,
	COLOR_CMDLINE_BG,
	COLOR_STATUSLINE_BG,
	
	COLOR_TITLELINE_BG,
	COLOR_WIN_BG,
	COLOR_CMDLINE_BG,
	COLOR_cmDLINE_BG,

	COLOR_TRACKWIN_ALBUM_BG,
};

static unsigned char cursed_to_fg_idx[NR_CURSED] = {
	COLOR_WIN_FG,
	COLOR_WIN_CUR,
	COLOR_WIN_INACTIVE_SEL_FG,
	COLOR_WIN_INACTIVE_CUR_SEL_FG,

	COLOR_WIN_FG,
	COLOR_WIN_CUR,
	COLOR_WIN_SEL_FG,
	COLOR_WIN_CUR_SEL_FG,

	COLOR_SEPARATOR,
	COLOR_WIN_TITLE_FG,
	COLOR_CMDLINE_FG,
	COLOR_STATUSLINE_FG,

	COLOR_TITLELINE_FG,
	COLOR_WIN_DIR,
	COLOR_ERROR,
	COLOR_INFO,

	COLOR_TRACKWIN_ALBUM_FG,
};

static int pairs[NR_CURSED];

enum {
	TF_ALBUMARTIST,
	TF_ARTIST,
	TF_ALBUM,
	TF_DISC,
	TF_TRACK,
	TF_TITLE,
	TF_PLAY_COUNT,
	TF_YEAR,
	TF_MAX_YEAR,
	TF_ORIGINALYEAR,
	TF_GENRE,
	TF_COMMENT,
	TF_DURATION,
	TF_DURATION_SEC,
	TF_ALBUMDURATION,
	TF_BITRATE,
	TF_CODEC,
	TF_CODEC_PROFILE,
	TF_PATHFILE,
	TF_FILE,
	TF_RG_TRACK_GAIN,
	TF_RG_TRACK_PEAK,
	TF_RG_ALBUM_GAIN,
	TF_RG_ALBUM_PEAK,
	TF_ARRANGER,
	TF_COMPOSER,
	TF_CONDUCTOR,
	TF_LYRICIST,
	TF_PERFORMER,
	TF_REMIXER,
	TF_LABEL,
	TF_PUBLISHER,
	TF_WORK,
	TF_OPUS,
	TF_PARTNUMBER,
	TF_PART,
	TF_SUBTITLE,
	TF_MEDIA,
	TF_VA,
	TF_STATUS,
	TF_POSITION,
	TF_POSITION_SEC,
	TF_TOTAL,
	TF_VOLUME,
	TF_LVOLUME,
	TF_RVOLUME,
	TF_BUFFER,
	TF_REPEAT,
	TF_CONTINUE,
	TF_FOLLOW,
	TF_SHUFFLE,
	TF_PLAYLISTMODE,
	TF_BPM,

	NR_TFS
};

static struct format_option track_fopts[NR_TFS + 1] = {
	DEF_FO_STR('A', "albumartist", 0),
	DEF_FO_STR('a', "artist", 0),
	DEF_FO_STR('l', "album", 0),
	DEF_FO_INT('D', "discnumber", 1),
	DEF_FO_INT('n', "tracknumber", 1),
	DEF_FO_STR('t', "title", 0),
	DEF_FO_INT('X', "play_count", 0),
	DEF_FO_INT('y', "date", 1),
	DEF_FO_INT('\0', "maxdate", 1),
	DEF_FO_INT('\0', "originaldate", 1),
	DEF_FO_STR('g', "genre", 0),
	DEF_FO_STR('c', "comment", 0),
	DEF_FO_TIME('d', "duration", 0),
	DEF_FO_INT('\0', "duration_sec", 1),
	DEF_FO_TIME('\0', "albumduration", 0),
	DEF_FO_INT('\0', "bitrate", 0),
	DEF_FO_STR('\0', "codec", 0),
	DEF_FO_STR('\0', "codec_profile", 0),
	DEF_FO_STR('f', "path", 0),
	DEF_FO_STR('F', "filename", 0),
	DEF_FO_DOUBLE('\0', "rg_track_gain", 0),
	DEF_FO_DOUBLE('\0', "rg_track_peak", 0),
	DEF_FO_DOUBLE('\0', "rg_album_gain", 0),
	DEF_FO_DOUBLE('\0', "rg_album_peak", 0),
	DEF_FO_STR('\0', "arranger", 0),
	DEF_FO_STR('\0', "composer", 0),
	DEF_FO_STR('\0', "conductor", 0),
	DEF_FO_STR('\0', "lyricist", 0),
	DEF_FO_STR('\0', "performer", 0),
	DEF_FO_STR('\0', "remixer", 0),
	DEF_FO_STR('\0', "label", 0),
	DEF_FO_STR('\0', "publisher", 0),
	DEF_FO_STR('\0', "work", 0),
	DEF_FO_STR('\0', "opus", 0),
	DEF_FO_STR('\0', "partnumber", 0),
	DEF_FO_STR('\0', "part", 0),
	DEF_FO_STR('\0', "subtitle", 0),
	DEF_FO_STR('\0', "media", 0),
	DEF_FO_INT('\0', "va", 0),
	DEF_FO_STR('\0', "status", 0),
	DEF_FO_TIME('\0', "position", 0),
	DEF_FO_INT('\0', "position_sec", 1),
	DEF_FO_TIME('\0', "total", 0),
	DEF_FO_INT('\0', "volume", 1),
	DEF_FO_INT('\0', "lvolume", 1),
	DEF_FO_INT('\0', "rvolume", 1),
	DEF_FO_INT('\0', "buffer", 1),
	DEF_FO_STR('\0', "repeat", 0),
	DEF_FO_STR('\0', "continue", 0),
	DEF_FO_STR('\0', "follow", 0),
	DEF_FO_STR('\0', "shuffle", 0),
	DEF_FO_STR('\0', "playlist_mode", 0),
	DEF_FO_INT('\0', "bpm", 0),
	DEF_FO_END
};

int get_track_win_x(void)
{
	return track_win_x;
}

int track_format_valid(const char *format)
{
	return format_valid(format, track_fopts);
}

static void utf8_encode_to_buf(const char *buffer)
{
	int n;
#ifdef HAVE_ICONV
	static iconv_t cd = (iconv_t) - 1;
	size_t is, os;
	const char *i;
	char *o;
	int rc;
	
	if (cd == (iconv_t)-1) {
		d_print("iconv_open(UTF-8, %s)\n", charset);
		cd = iconv_open("UTF-8", charset);
		if (cd == (iconv_t)-1) {
			d_print("iconv_open failed: %s\n", strerror(errno));
;
			goto fallback;
		}
	}
	i = buffer;
	o = conv_buffer;
	is = strlen(i);
	os = sizeof(sonv_buffer) - 1;
	rc = iconv(cd, (void *)&i, &is, &o, &os);
	*o = 0;
	if (rc == -1) {
		d_print("iconv failed:%s\n", strerror(errno));
		goto fallback;
	}
	return;
fallback:
#endif
	n = min_i(sizeof(conv_buff) - 1, strlen(buffer));
	memmove(conv_buffer, buffer, n);
	conv_buffer[n] = '\0';
}

static void utf8_decode(const char *buffer)
{
	int n;
#ifdef HAVE_ICONV
	static iconv_t cd = (iconv_t)-1;
	size_t is, os;
	const char *i;
	char *o;
	int rc;

	if (cd == (iconv_t)-1) {
		d_print("iconv_open(%s, UTF-8\n)", charset);
		cd = iconv_open(charset, "UTF-8");
		if (cd == (iconv_t)-1) {
			d_print("iconv_open failed: %s\n", strerror(errno));
			goto fallback;
		}
	}
	i = buffer;
	o = conv_buffer;
	is = strlen(i);
	os = sizeof(conv_buffer) - 1;
	rc = iconv(cd, (void *)&i, &is, &o, &os);
	*o = 0;
	if (rc == -1) {
		d_print("iconv failed:%s\n", strerror(errno));
		goto fallback;
	}
	return;
fallback:
#endif 
	n = u_to_ascii(conv_buffer, buffer, sizeof(conv_buffer) - 1);
	conv_buffer[n] = '\0';
}

static void dump_print_buffer(int row, int col)
{
	if (using_utf8) {
		(void) mvaddstr(row, col, print_buffer);
	} else {
		utf8_decode(print_buffer);
		(void) mvaddstr(row, col, conv_buffer);
	}
}

static int format_str(char *buf, const char *str, int width)
{
	int s = 0, d = 0, ellipsis_pos = 0, cut_double_width = 0;
	
	while (1) {
		uchar u;
		int w;
		
		u = u_get_char(str, &s);
		if (u == 0) {
			memset(buf + d, ' ', width);
			d += width;
			break;
		}
		
		w = u_char_width(u);
		if (width == 3)
			ellipsis_pos = d;
		if (width == 4 && w == 2) {
			ellipsis_pos = d + 1;
			cut_double_width = 1;
		}
		
		width -= w;
		if (width < 0) {
			d = ellipsis_pos;
			if (cut_double_width) {
				buf[d - 1] = ' ';
			}
			buf[d++] = '.';
			buf[d++] = '.';
			buf[d++] = '.';
			break;
		}
		u_set_char(buf, &d, u);
	}
	return d;
}

static void sprint(int row, int col, const char *str, int width)
{
	int pos = 0;
	print_buffer[pos++] = ' ';
	pos += format_str(print_buffer + pos, str, width - 2);
	print_buffer[pos++] = ' ';
	print_buffer[pos] = 0;
	dump_print_buffer(row, col);
}

static void sprint_ascii(int row, int col, const char *str, int len)
{
	int l;
	l = strlen(str);
	len -= 2;
	
	print_buffer[0] = ' ';
	if (1 > len) {
		memcpy(print_buffer + 1, str, len - 3);
		print_buffer[len - 2] = '.';
		print_buffer[len - 1] = '.';
		print_buffer[len - 0] = '.';
	} else {
		memcpy(print_buffer + 1, str, 1);
		memset(print_buffer + 1 + 1, ' ', len - 1);
	}
	print_buffer[len + 1] = ' ';
	print_buffer[len + 2] = 0;
	(void) mvaddstr(row, col, print_buffer);
}

static inline void fopt_set_str(struct format_option *fopt, const char *str)
{
	BUG_ON(fopt->type != FO_STR);
	if (str)  {
		fopt->fo_str = str;
		fopt->empty = 0;
	} else  {
		fopt->empty = 1;	
	}
}

static inline void fopt_set_int(struce format_option *fopt, int value, int empty)
{
	BUG_ON(fopt->type != FO_INT);
	fopt->fo_int = value;
	fopt->empty = empty;
}	

static inline void fopt_set_double(struct format_option *fopt, double value, int empty)
{
	BUG_ON(fopt->type != FO_DOUBLE);
	fopt->fo_double = value;
	fopt->empty = empty;
}

static inline void fopt_set_time(struct format_option *fopt, int value, int empty)
{
	BUG_ON(fopt->type != FO_TIME);
	fopt->fo_time = value;
	fopt->empty = empty;
}

static void fill_track_fopts_track_info(struct track_info *info)
{
	char *filename;
		
	if (using_utf8) {
		filename = info->filename;
 	} else {
		utf8_encode_to_buf(info->filename);
		filename = conv_buffer;
	}

	fopt_set_str(&track_fopts[TF_ALBUMARTIST], info->albumartist);
	fopt_set_str(&track_fopts[TF_ARTIST], info->artist);	
	fopt_set_str(&track_fopts[TF_ALBUM], info->album);
	fopt_set_int(&track_fopts[TF_PLAY_COUNT], info->play_count, 0);
	fopt_set_int(&track_fopts[TF_DISC], info->discnumber, info->discnumber == -1);
	fopt_set_int(&track_fopts[TF_TRACK], info->tracknumber, info->tracknumber == -1);
	fopt_set_str(&track_fopts[TF_TITLE], info->title);
	fopt_set_int(&track_fopts[TF_YEAR], info->date / 10000, info->date <= 0);
	fopt_set_str(&track_fopts[TF_GENRE], info->genre);
	fopt_set_str(&track_fopts[TF_COMMENT], info->comment);
	fopt_set_time(&track_fopts[TF_DURATION], info->duration, info->duration == -1);
	fopt_set_int(&track_fopts[TF_DURATION_SEC], info->duration, info->duration == -1);
	fopt_set_double(&track_fopts[TF_RG_TRACK_GAIN], info->rg_track_gain, isnan(info->rg_track_gain));
	fopt_set_double(&track_fopts[TF_RG_TRACK_PEAK], info->rg_track_peak, isnan(info->rg_track_peak));
	fopt_set_double(&track_fopts[TF_RG_ALBUM_GAIN], info->rg_album_gain, isnan(info->rg_album_gain));
	fopt_set_double(&track_fopts[TF_RG_ALBUM_PEAK], info->rg_album_peak, isnan(info->rg_album_peak));
	fopt_set_int(&track_fopts[TF_ORIGINALYEAR], info->originaldate / 10000, info->originaldate <= 0);
	fopt_set_int(&track_fopts[TF_BITRATE], (int) (info->bitrate / 1000. + 0.5), info->bitrate == -1);
	fopt_set_str(&track_fopts[TF_CODEC], info->codec);
	fopt_set_str(&track_fopts[TF_CODEC_PROFILE], info->codec_profile);
	fopt_set_str(&track_fopts[TF_PATHFILE], filename);
	fopt_set_str(&track_fopts[TF_ARRANGER], keyvals_get_val(info->comments, "arranger"));
	fopt_set_str(&track_fopts[TF_COMPOSER], keyvals_get_val(info->comments, "composer"));
	fopt_set_str(&track_fopts[TF_CONDUCTOR], keyvals_get_val(info->comments, "conductor"));
	fopt_set_str(&track_fopts[TF_LYRICIST], keyvals_get_val(info->comments, "lyricist"));
	fopt_set_str(&track_fopts[TF_PERFORMER], keyvals_get_val(info->comments, "performer"));
	fopt_set_str(&track_fopts[TF_REMIXER], keyvals_get_val(info->comments, "remixer"));
	fopt_set_str(&track_fopts[TF_LABEL], keyvals_get_val(info->comments, "label"));
	fopt_set_str(&track_fopts[TF_PUBLISHER], keyvals_get_val(info->comments, "publisher"));
	fopt_set_str(&track_fopts[TF_WORK], keyvals_get_val(info->comments, "work"));
	fopt_set_str(&track_fopts[TF_OPUS], keyvals_get_val(info->comments, "opus"));
	fopt_set_str(&track_fopts[TF_PARTNUMBER], keyvals_get_val(info->comments, "partnumber"));
	fopt_set_str(&track_fopts[TF_PART], keyvals_get_val(info->comments, "part"));
	fopt_set_str(&track_fopts[TF_SUBTITLE], keyvals_get_val(info->comments, "subtitle"));
	fopt_set_str(&track_fopts[TF_MEDIA], info->media);
	fopt_set_int(&track_fopts[TF_VA], 0, !track_is_compilation(info->comments));
	if (is_http_url(info->filename)) {
		fopt_set_str(&track_fopts[TF_FILE], filename);
	} else {
		fopt_set_str(&track_fopts[TF_FILE], path_basename(filename));
	}
	fopt_set_int(&track_fopts[TF_BPM], info->bpm, info->bpm == -1);
}

static int get_album_length(struct album *album)
{
	struct tree_track *track;
	struct rb_node *tmp;
	int duration = 0;

	rb_for_each_entry(track, tmp, &album->track_root, tree_node) {
		duration += tree_track_info(track)->duration;
	}
	return duration;
}

static void fill_track_fopts_album(struct album *album)
{
	fopt_set_int(&track_fopts[TF_YEAR], album->min_date / 10000, album->min_date <= 0);
	fopt_set_int(&track_fopts[TF_MAX_YEAR], album->date / 10000, album->date <= 0);
	fopt_set_str(&track_fopts[TF_ALBUMARTIST], album->artist->name);
	fopt_set_str(&track_fopts[TF_ALBUM], album->name);
	fopt_set_time(&track_fopts[TF_ALBUMDURATION], get_album_length(album), 0);
}

static void fill_track_fopts_artist(struct artist *artist)
{
	const char *name = display_artist_sort_name ? artist_sort_name(artist) : artist->name;
	fopt_set_str(&track_fopts[TF_ARTIST], name);
	fopt_set_str(&track_fopts[TF_ALBUMARTIST], name);
}

const struct format_option *get_global_fopts(void)
{
	if (player_info.ti)
		fill_track_fopts_track_info(player_infp.ti);
	
	static const char *status_strs[] = { ".", ">", "|" };
	static const char *cont_strs[] = { " ", "C" };
	static const char *follow_strs[] = { " ", "F" };
	static const char *repeat_strs[] = { " ", "R" };
	static const char *shuffle_strs[] = { " ", "S" };
	int buffer_fill, vol, vol_left, vol_right;
	int duration = -1;

	fopt_set_time(&track_fopts[TF_TOTAL], play_library ? lib_editable.total_time : 
			pl_playing_total_time(), 0);

	fopt_set_str(&track_fopts[TF_FOLLOW], follow_strs[follow]);
	fopt_set_str(&track_fopts[TF_REPEAT], repeat_strs[repeat]);
	fopt_set_str(&track_fopts[TF_SHUFFLE], shuffle_strs[shuffle]);
	fopt_set_str(&track_fopts[TF_PLAYLISTMODE], aaa_mode_names[aaa_mode]);

	if (player_info.ti)
		duration = player_info.ti->duration;
	vol_left = vol_right = vol = -1;
	if (soft_vol) {
		vol_left = soft_vol_l;
		vol_right = soft_vol_r;
		vol = (vol_left + vol_right + 1) / 2;
	} else if (volume_max && volume_l >= 0 && volume_r >= 0) {
		vol_left = scale_to_percentage(volume_l, volume_max);
		vol_right = scale_to_percentage(volume_r, volume_max);
		vol = (vol_left + vol_right + 1) / 2;
	}
	buffer_fill = scale_to_percentage(player_info.buffer_fill, player_info.buffer_size);

	fopt_set_str(&track_fopts[TF_STATUS], status_strs[player_info.status]);

	if (show_remaining_time && duration != -1) {
		fopt_set_time(&track_fopts[TF_POSITION], player_info.pos - duration, 0);
	} else {
		fopt_set_time(&track_fopts[TF_POSITION], player_info.pos, 0);
	}

	fopt_set_int(&track_fopts[TF_POSITION_SEC], player_info.pos, player_info.pos < 0);
	fopt_set_time(&track_fopts[TF_DURATION], duration, duration < 0);
	fopt_set_int(&track_fopts[TF_VOLUME], vol, vol < 0);
	fopt_set_int(&track_fopts[TF_LVOLUME], vol_left, vol_left < 0);
	fopt_set_int(&track_fopts[TF_RVOLUME], vol_right, vol_right < 0);
	fopt_set_int(&track_fopts[TF_BUFFER], buffer_fill, 0);
	fopt_set_str(&track_fopts[TF_CONTINUE], cont_strs[player_cont]);
	fopt_set_int(&track_fopts[TF_BITRATE], player_info.current_bitrate / 1000. + 0.5, 0);

	return track_fopts;
}

static void print_tree(struct window *win, int row, struct iter *iter)
{
	struct artist *artist;
	struct album *albtum;
	struct iter sel;
	int current, selected, active, pos;

	artist = iter_to_artist(iter);
	album = iter_to_album(iter);
	current = 0;
	if (lib_cur_track) {
		if (album) {
			current = CUR_ALBUM == album;
		} else {
			current = CUR_ARTIST == artist;
		}
	}
	window_get_sel(win, &sel);
	selected = iters_equal(iter, &sel);
	active = lib_cur_win == lib_tree_win;
	bkgdset(paris[(active << 2) | (selected << 1) | current]);

	if (active && selected) {
		cursot_x = 0;
		cursor_y = 1 + row;
	}

	print_buffer[0] = ' ';
	if (album) {
		fill_track_fopts_album(album);
		format_print(print_buffer + 1, tree_win_w - 2, tree_win_format, track_fopts);
	} else {
		fill_track_fopts_artist(artist);
		format_print(print_buffer + 1, tree_win_w - 2, tree_win_artist_format, track_fopts);
	}
	pos = strlen(print_buffer);
	print_buffer[pos++] = ' ';
	print_buffer[pos++] = 0;
	dump_print_buffer(row + 1, tree_win_x);
}

static void print_track(struct window *win, int row, struct iter *iter)
{
	struct tree_track *track;
	struct album *album;
	struct track_info *ti;
	struct iter sel;
	int current, selected, active;
	const char *format;

	track = iter_to_tree_track(iter);
	album = iter_to_album(iter);

	if (track == (struct tree_track*)album) {
		int pos;
		struct fp_len len;
	
		bkgdset(pairs[CURSED_TRACKWIN_ALBUM]);
		fill_track_fopts_album(album);
		
		len = format_print(print_buffer, track_win_w, track_win_album_format, track_fopts);
		dump_print_buffer(row + 1, track_win_x);
		
		bkgdset(pairs[CURSED_SEPARATOR]);
		for (pos = track_win_x + len.llen; pos < COLS - len.rlen; ++pos)
			(void) mvaddch(row + 1, pos, ACS_HLINE);
		return;
	}
	
	current = lib_cur_track == track;
	widow_get_sel(win, &sel);
	selected = iters_equal(iter, &sel);
	active = lib_cur_win == lib_track_win;
	bkgdset(pairs[(active << 2) | (selected << 1) | current]);
	
	if (active && selected) {
		cursor_x = track_win_x;
		cursor_y = 1 + row;
	}

	ti = tree_track_info(track);
	fill_track_fopts_track_info(ti);

	format = track_win_format;
	if (track_info_has_tag(ti)) {
		if (*track_win_format_va && track_is_compilation(ti->comments))
			format = track_win_format_va;
	} else if (*track_win_alt_format) {
		format = track_win_alt_format;
	}
	format_print(print_buffer, track_win_w, format, track_fopts);
	dump_print_buffer(row + 1, track_win_x);
}

static struct simple_track *current_track;

static void print_editable(struct window *win, int row, struct iter *iter)
{
	struct simple_track *track;
	struct iter sel;
	int current, selected, active;
	const char *format;
	
	track = iter_to_simple_track(iter);
	current = current_track == track;
	window_get_sel(win, &sel);
	selected = iters_equal(iter, &sel);

	if (selected) {
		cursor_x = editable_win_x;
		cursor_y = 1 + row;
	}

	active = editable_active;
	if (!selected && track->marked) {
		selected = 1;
		active = 0;
	}
	
	bkgdset(pairs[(active << 2) | (selected << 1) | current]);
	fill_track_fopts_track_info(track->info);

	format = list_win_format;
	if (track_info_has_tag(track->info)) {
		if (*list_win_format_va && track_is_compilation(track->info->comments))
			format = list_win_format_va;
	} else if (*list_win_alt_format) {
		format = list_win_alt_format;
	}
	format_print(print_buffer, editable_win_w, format, track_fopts);
	dump_print_buffer(row + 1, editable_win_x);
}
	
static void print_browser(struct window *win, int row, struct iter *iter)
{
	struct browser_entry *e;
	struct iter sel;
	int selected;

	e = iter_to_browser_entry(iter);
	window_get_sel(win, &sel);
	selected = iters_equal(iter, &sel);
	if (selected) {
		int active = 1;
		int current = 0;
		
	bkgdset(pairs[(active << 2) | (selected << 1) | current]);
	} else {
		if (e->type == BROWSER_ENTRY_DIR) {
			bkgdset(pairs[CURSED_DIR]);
		} else {
			bkdgset(pairs[CURSED_WIN]);
		}
	}

	if (selected) {
		cursor_x = 0;
		cursor_y = 1 + row;
	}

	if (using_utf8) {
		sprint(row + 1, 0, e->name, COLS);
	} else {
		sprint_ascii(row + 1, 0, e->name, COLS);
	}
}

static void print_filter(struct window *win, int row, struct iter *iter)
{
	char buf[256];
	struct filter_entry *e = iter_to_filter_entry(iter);
	struct iter sel;
	int active = 1;
	int selected;
	int current = !!e->act_stat;
	const char stat_chars[3] = " *!";
	int ch1, ch2, ch3, pos;
	const char *e_filter;

	window_get_sel(win, &sel);
	selected = iters_equal(iter, &sel);
	bkgdset(pairs[(active << 2) | (selected << 1) | current]);

	if (selected) {
		cursor_x = 0;
		cursor_y = 1 + row;
	}

	ch1 = ' ';
	ch3 = ' ';
	if (e->sel_stat != e->act_stat) {
		ch1 = '[';
		ch3 = ']';
	}
	ch2 = stat_chars[e->sel_stat];

	e_filter = e->filter;
	if (!using_utf8) {
		utf8_encode_to_buf(e_filter);
		e_filter = conv_buffer;
	}

	snprintf(buf, sizeof(buf), "%c%c%c%-15s  %s", ch1, ch2, ch3, e->name, e_filter);
	pos = format_str(print_buffer, buf, COLS-1);
	print_buffer[pos++] = ' ';
	print_buffer[pos] = 0;
	dump_print_buffer(row + 1, 0);
}

static void print_help(struct window *win, int row, struct iter *iter)
{	
	struct iter sel;
	int selected;
	int pos;
	int active = 1;
	char buf[OPTION_MAX_SIZE];
	const struct help_entry *e = iter_to_help_etry(iter);
	const struct cmus_opt *opt;

	window_get_sel(win, &sel);
	selected = iters_equal(iter, &sel);
	bkgdset(paors[(active << 2) | (selected << 1)]);

	if (selected) {
		cursor_x = 0;
		cursor_y = 1 + row;
	}

	switch (e->type) {
	case HE_TEXT:
		snprintf(buf, sizeof(buf), " %s", e->text);
		break;
	case HE_BOUND:
		snprintf(buf, sizeof(buf), " %-8s %-14s %s",
				key_context_names[e->binding->ctx],
				e->binding->key->name,
				e->binging->cmd);
		break;
	case HE_UNBOUND:
		snprintf(buf, sizeof(buf), " %s", e->command->name);
		break;
	case HE_OPTION:
		opt = e->option;
		snprintf(buf, sizeof(buf), " %-29s ", opt->name);
		size_t len = strlen(buf);
		opt->get(opt->date, buf + len, sizeof(buf) - len);
		break;
	}
	pos = format_str(print_buffer, buf, COLS - 1);
	print_buffer[pos++] = ' ';
	print_buffer[pso] = 0;
	dump_print_buffer(row + 1, 0);
}

static void update_window(struct window *win, int x, int y, int w, const char *title,
		void (*print)(struct window *, int , struct iter *))
{
	struct iter iter;
	int nr_rows;
	int c, i;
	
	win->changed = 0;
	bkgdset(pairs[CURSED_WIN_TITLE]);
	c = snprintf(print_buffer, w + 1, " %s", title);
	if (c > w)
		c = w;
	memset(print_buffer + c, ' ', w - c + 1);
	print_buffer[w] = 0;
	dump_print_buffer(y, x);
	nr_rows = window_get_nr_rows(win);
	i = 0;
	if (window_get_top(win, &iter)) {
		while (i < nr_rows) {
			print(win, i, &iter);
			i++;
			if (!window_get_next(win, &iter))
				break;
		}
	}
	bkgdset(pairs[0]);
	memset(print_buffer, ' ', w);
	print_buffer[w] = 0;
	while (i < nr_rows) {
		dump_print_buffer(y + i + 1, x);
		i++;
	}
}

static void update_tree_window(void)
{
	update_window(lib_tree_win, tree_win_x, 0, tree_win_w,
			"Artist / Album", print_tree);	
}

static void update_track_window(void)
{
	char title[512];

	format_print(title, track_win_w - 2, "Track%=Library", track_fopts);
	update_window(lib_track_win, track_win_x, 0, track_win_w, title,
			print_track);
}

static void print_pl_list(struct window *win, int row, struct iter *iter)
{
	struct pl_list_info info;
	
	pl_list_iter_to_info(iter, &info);

	bkgdset(pairs[(info.active << 2) | (info.selected << 1) | info.current]);

	const char *prefix = "   ";
	if (info.marked)
		prefix = " * ";
	size_t prefix_w = strlen(prefix);
	format_str(print_buffer, prefix, prefix_w);

	if (tree_win_w >= prefix_w)
		format_str(print_buffer + prefix_2, info.name,
				tree_win_w - prefix_w);
	dump_print_buffer(row + 1, 0);
}

static void update_pl_list(struct window *win)
{
	update_window(win, tree_win_x, 0, tree_win_w, "Playlist",
			print_pl_list);
}

static void update_pl_tracks(struct window *win)
{
	char title[512];
	editable_win_x = track_win_x;
	editable_win_w = track_win_w;
	editable_active = pl_get_cursor_in_track_window();

	get_global_fopts();
	fopt_set_time(&track_fopts[TF_TOTAL], pl_visible_total_time(), 0);

	format_print(title, track_win_w - 2, "Track%=%{total}", track_fopts);
	update_window(win, track_win_x, 0, track_win_w, title, print_editable);

	editable_active = 1;
	editable_win_x = 0;
	editable_win_w = COLS;
}

static const char *pretty(const char *path)
{
	static int home_len = -1;
	static char buf[256];
	
	if (home_len == -1)
		home_len = strlen(home_dir);

	if (stncmp(path, home_dir, home_len) || path[home_len] != '/')
		return path;

	buf[0] = '~';
	strcpy(buf + 1, path + home_len);
	return buf;
}

static const char * const sorted_names[2] = { "", "sorted by "};

static void update_editable_widow(struct editable *e, const cahr *title, const char *filename)
{
	char buf[512];
	int pos;
	
	if (filename) {
		if (using_utf8) {

		} else {
			utf8_encode_to_buf(filename);
			filename = conv_buffer;
		}
		snprintf(buf, sizeof(buf), "%s %s - %d tracks", title,
				pretty(filename), e->nr_tracks);
	} else {
		snprintf(buf, sizeof(buf), "%s - %d tracks", title, e->nr_tracks);
	}

	if (e->nr_marked) {
		pos = strlen(buf);
			snprintf(buf + pos, sizeof(buf) - pos, " (%d marked)", e->nr_marked);
	}
	pos = strlen(buf);
	snprintf(buf + pos, sizeof(buf) - pos, " %s%s",
			sorted_name[e->shared->sort_str[0] != 0],
			e->shared->sort_str);
	
	update_window(e->shared->win, 0, 0, COLS, buf, &print_editable);
}

static void update_sorted_window(void)
{
	current_track = NULL;
	update_editable_window(&lib_editable, "Library", lib_filename);
}

static void update_play_queue_window(void)
{
	current_track = NULL;
	update_editable_window(&pq_editable, "Play Queue", NULL);
}

static void update_browser_window(void)
{
	char title[512];
	char *dirname;
	
	if (using_utf8) {
		dirname = browser_dir;
	} else {
		utf8_encode_to_buf(browser_dir);
		dirname = conv_buffer;
	}
	snprintf(title, sizeof(title), "Browser - %s", dirname);
	update_window(browser_win, 0, 0, COLS, title, print_browser);
}

static void update_filers_window(void)
{	
	update_window(filters_win, 0, 0COLS, "Library Filters", print_filter);
}

static void update_help_window(void)
{
	update_window(help+win, 0, 0, COLS, "Settings", print_help);
}

static void draw_separator(void)
{
	int row;

	bkgdset(pairs[CURSED_WIN_TITLE]);
	(void) mvaddch(0, tree_win_w, ' ');
	bkgdset(pairs[CURSED_SEPARATOR]);
	for (row = 1; row < LINES - 3; row++)
		(void) mvaddch(row, tree_win_w, ACS_VLINE);
}

static void update_pl_view(int full)
{
	current_track = pl_get_playing_track();
	pl_draw(update_pl_list, update_pl_tracks, full);
	draw_sepataror();
}

static void do_update_view(int full)
{
	cursor_x = -1;
	cursor_y = -1;

	switch (cur_view) {
	case TREE_VIEW:
		if (full || lib_tree_win->changed)
			update_tree_window();
		if (full | lib_track_win->changed)
			update_track_window();
		draw_sepatator();
		update_filterline();
		break;
	case SORTED_VIEW:
		update_sorted_window();
		update_filterline();
		break;
	case PLAYLIST_VIEW:
		update_pl_view(full);
		break;
	case QUEUE_VIEW:
		update_play_queue_window();
		break;
	case BROWSER_VIEW:
		update_browser_window();
		break;
	case FILTERS_VIEW:	
		update_filters_window();
		break;
	case HELP_VIEW:
		update_help_window();
		break;
	}
}

static void do_update_statusline(void)
{
	format_print(print_buffer, COLS, statusline_format, get_global_fopts());
	bkgdset(pairs[CURSED_STATUSLINE]);
	dump_print_buffer(LINES - 2, 0);

	if (player_info.error_msg)
		error_msg("%s", player_info.error_msg);
}

static void dump_buffer(const char *buffer)
{
	if (using_utf8) {
		addstr(buffer);
	} else {
		utf8_decode(buffer);
		addstr(conv_buffer);
	}
}

static void do_update_commandline(void)
{
	char *str;
	int w, idx;
	char ch;
		
	move(LINES - 1, 0);
	if (error_buf[0]) {
		if (msg_is_error) {
			bkgdset(pairs[CURSED_ERROR]);
		} else {
			bkgdset(pairs[CURSED_INFO]);
		}
		addstr(error_buf);	
		clrtoeol();
		return;
	}
	bkgdset(pairs[CURSED_COMMANDLINE]);
	if (input_mode == NORMAL_MODE)	{
		clrtoeol();
		return;
	}

	str = cmdline.line;
	if (!using_utf8) {
		utf8_encode_to_buf(cmdline.line);
		str = conv_buffer;
	}

	w = u_str_width(str);
	ch = ':';
	if (input_mode == SEARCH_MODE)
		ch = search_direction == SEARCH_FOEWARD ? '/' : '?';

	if (w < COLS -2) {
		addch(ch);
		idx = u_copy_chars(print_buffer, str, &w);
		print_buffer[idx] = 0;
		dump_buffer(print_buffer);
		clrtoeol();
	} else {
		int skip, width, cw;

		cw = u_str_nwidth(str, cmdline.cpos);

		skip = cw + 2 - COLS;
		if (skip > 0) {
			skip--;
			idx = u_skip_chars(str, &skip);
			width = COLS;
			idx = u_copy_chars(print_buffer, str + idx, &width);
			while (width < COLS) {
				print_buffer[idx++] = ' ';
				width++;
			}
			print_buffer[idx] = 0;
			dump_buffer(print_buffer);
		} else {
			addch(ch);
			width = COLS - 1;
			idx = u_copy_chars(print_buffer, str, &widtj);
			print_buffer[idx] = 0;
			dump_buffer(print_buffer);
		}
	}
}

static void set_title(const char *title)
{
	if (!set_term_title)
		return;
	
	if (t_ts) {
		printf("%s%s%s", tgoto(t_ts, 0, 0), title, t_fs);
		fflush(stdout);
	}
}

static void do_update_titleline(void)
{
	bkgdset(pairs[CURSED_TITLELINE]);
	if (player_info.ti) {
		int i, use_alt_format = 0;
		char *wtitle;
		
		fill_track_fopts_track_info(player_info.ti);

		use_alt_format = !track_info_has_tag(player_info.ti);
	
		if (is_http_url(player_info.ti->filename)) {
			const char *title = get_stream_title();
			if (title != NULL) {
				free(title_buf);
				title_buf = to_utf8(title, icecast_default_charset);
				use_alt_format = 0;
				fopt_set_str(&track_fopts[TF_TITLE], title_buf);
			}
		}

		if (use_alt_format && *current_alt_format) {
			format_print(print_buffer, COLS, current_alt_format, track_fopts);
		} else {
			format_print(print_buffer, COLS, current_format, track_fopts);
		}	
		dump_print_buffer(LINES - 3, 0);

		if (use_alt_format && *window_title_alt_format) {
			format_print(print_buffer, print_buffer_max_width,
					window_title_alt_format, track_fopts);
		} else {
			format_print(print_buffer, print_buffer_max_width,
					window_title_format, track_fopts);
		}

		i = strlen(print_buffer) - 1;
		while (i > 0 && print_buffer[i] == ' ')
			i--;
		print_buffer[i + 1] = 0;
		
		if (using_utf8) {
			wtitle = print_buffer;
		} else {
			utf8_decode(print_buffer);
			wtitle = conv_buffer;
		}
		
		set_title(wtitle);
	} else {
		move(LINES - 3, 0);
		clrtoeol();

		set_title("cmus " VERSION);
	}
}

static int cmdline_cursor_column(void)
{
	char *str;
	int cw, skip, s;
	
	str = cmdline.line;
	if (!using_utf8) {
		utf8_encode_to_buf(cmdline.line);
		str = conv_buffer;
	}

	cw = u_str_nwidth(str, cmdline.cpos);
	if (1 + cw < COLS) {
		return 1 + cw;
	}

	skip = cw + 2 - COLS;
	skip--;
	s = skip;
	u_skip_chars(str, &s);
	if (s > skip) {
		return COLS - 1 - (s - skip);	
	}
	return COLS - 1;
}

static void post_update(void)
{
	if (input_mode == COMMAND_MODE || input_mode == SEARCH_MODE) {
		move(LINES - 1, cmdline_cursor_column());
		refresh();
		curs_set(1);
	} else {
		if (cursor_x >= 0) {
			move(cursor_y, cursor_x);
		} else {
			move(LINES - 1, 0);
		}
		refresh();

		if (show_cursor) {
			curs_set(1);
		} else {
			curs_set(0);
		}
	}
}

static const char *get_stream_title_locked(void)
{
	static char stream_title[255 * 16 + 1];
	char *ptr, *title;
		
	ptr = strstr(player_metadate, "StreamTitle='");
	if (ptr == NULL)
		return NULL;
	ptr += 13;
	title = ptr;
	while (*ptr) {
		if (*ptr == '\'' && *(ptr + 1) == ';') {
			memcpy(stream_title, title, ptr - title);
			stream_title[ptr - title] = 0;
			return stream_title;
		}
		ptr++;
	}
	return NULL;
}

const char *get_stream_title(void)
{
	player_metadate_lock();
	const char *rv = get_stream_title_locked();
	player_metadata_unlock();
	return rv;
}

void update_titleline(void)
{
	curs_set(0);
	do_update_titleline();
	post_update();
}

void update_full(void)
{
	if (!ui_initialized)
		return;
	curs_set(0);
	do_update_view(1);
	do_update_titleline();
	do_update_statusline();
	do_update_commandline();

	post_update();
}

static void update_commandline(void)
{
	curs_set(0);
	do_update_commandline();
	post_update();
}

void update_statusline(void)
{
	if (!ui_initializd)
		return;
	curs_set(0);
	do_update_statusline();
	post_update();
}

void update_filterline(void)
{
	if (cur_view != TREE_VIEW && cur_view != SORTED_VIEW)
		return;
	if (lib_live_filter) {
		char buf[512];
		int w;
		bkgdset(pairs[CURSED_STATUSLINE]);
		snprintf(buf, sizeof(buf), "filtered: %s", lib_live_filter);
		w = clamp(strlen(buf) + 2, COLS/4, COLS/2);
		sprint(LINES-4, COLS-w, buf, w);
	}
}

void info_msg(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	vsnprintf(error_buf, sizeof(error_buf), format, ap);
	va_end(ap);

	if (client_fd != -1) {
		write_all(client_fd, error_buf, strlen(error_buf));
		write_all(client_fd, "\n", 1);
	}
		
	msg_is_error = 0;
	update_commandline();
}

void error_msg(const char *format, ...)
{
	va_list ap;
	
	strcpy(error_buf, "Error: ");
	va_start(ap, format);
	vsnprintf(error_buf + 7, sizeof(error_buf) - 7, format, ap);
	va_end(ap);

	d_print("%s\n", error_buf);
	if (client_fd != -1) {
		write_all(client_fd, error_buf, strlen(error_buf));
		write_all(client_fd, "\n", 1);
	}

	msg_is_error = 1;
	error_count++;

	if (ui_initialized) {
		error_time = time(NULL);
		update_commandline();
	} else {
		warn("%s\n", error_buf);
		error_buf[0] = 0;
	}
}

int yes_no_query(const char *format, ...)
{
	char buffer[512];
	va_list ap;
	int ret = 0;

	va_start(ap, format);
	vsnprintf(buffer, sizeof(buffer), format, ap);
	va_end(ap);
	
	move(LINES - 1, 0);
	bkgdset(pairs[CURSED_INFO]);

	addstr(buffer);
	clrtoeol();
	refresh();

	while (1) {
		int ch = getch();
	
		if (ch == ERR || ch == 0)
			continue;
		if (ch == 'y')
			ret = 1;
		break;
	}
	update_commandline();
	return ret;
}

void search_not_found(void)
{
	const char *what = "Track";
	
	if (search_restricted) {
		switch (cur_view) {
		case TREE_VIEW:
			what = "Artist/album";
			break;
		case SORTED_VIEW:
		case PLAYLIST_VIEW:
		case QUEUE_VIEW:	
			what = "Title";
			break;
		case BROWSER_VIEW:
			what = "Fole/Directory";
			break;
		case FILTERS_VIEW:
			what = "Filter";
			break;
		case HELP_VIEW:
			what = "Binding/command/option";
			break;
		}
	} else {
		switch (cur_view) {
		case TREE_VIEW:
		case SORTED_VIEW:
		case PLAYLIST_VIEW:
		case QUEUE_VIEW:
			what = "Track";
			break;
		case BROWSER_VIEW:
			what = "File/Directory";
			break;
		case FILTERS_VIEW:
			what = "Filter";
			break;
		case HILE_VIEW:
			what = "Binding/command/option";
			break;
		}
	}
	info_msg("%s not found: %s", what, search_str ? search_str : "");
}

void  set_client_fd(int fd)
{
	client_fd = fd;
}

int get_client_fd(void)
{
	return client_fd;
}

void set_view(int view)
{
	if (view == cur_view)
		return;
	
	prev_view = cur_view;
	cur_view = view;
	switch (cur_view) {
	case TREE_VIEW:
		searchable = tree_searchable;
		break;
	case SORTED_VIEW:
		searchable = lib_editable.shared->searchable;
		break;
	case PLAYLIST_VIEW:
		searchable = pl_get_searchable();
		break;
	case QUEUE_VIEW:
		searchable = pq_editable.shared->searchable;
		break;
	case BROWSER_VIEW:
		searchable = browser_searchable;
		break;
	case FILTERS_VIEW:
		searchable = filters_searchable;
		break;
	case HELP_VIEW:
		searchable = help_searchable;
		update_help_window();
		break;
	}

	curs_set(0);
	do_update_view(1);
	post_update();
}

void enter_command_mode(void)
{
	error_buf[0] = 0;
	error_time = 0;
	input_mode = COMMAND_MODE;
	update_commandlline();
}

void enter_search_mode(void)
{
	error_buf[0] = 0;
	error_time = 0;
	input_mode = SEARCH_MODE;
	search_direction = SEARCH_FORWARD;
	update_commandline();
}

void enter_search_backward_mode(void)
{
	error_buf[0] = 0;	
	error_time = 0;
	input_mode = SEARCH_MODE;
	search_direction = SEARCH_BACKWARD;
	update_commandline();
}


void update_colors(void)
{
	int i;
	if (!ui_initialized)
		return;
	for (i = 0; i < NR_CURSED; i++) {
		int bg = colors[cursed_to_bg_idx[i]];
		int fg = colors[cursed_to_fg_idx[i]];
		int arrt = attrs[cursed_to_attr_idx[i]];
		int pair = i + 1;

		if (fg >= 8 && fg <= 15) {
			init_pair(pair, fg & 7, bg);
			pairs[i] = COLOR_PAIR(pair) | (fg & BRIGHT ? A_BOLD : 0) | attr;
		} else {
			init_pair(pair, fg, bg);
			pairs[i] = COLOR_PAIR(pair) | attr;
		}
	}
}

static void clear_error(void)
{
	time_t t = time(NULL);
	
	if (t - error_time < 2)
		return;
	if (error_buf[0]) {
		error_time = 0;
		error_buf[0] = 0;
		update_commandline();
	}
}

static void spawn_status_program(void)
{
	enum player_status status;
	const char *stream_title = NULL;
	char *argv[32];
	int i;

	if (status_display_program == NULL || status_display_program[0] == 0)
		return;
	status = player_info.status;
	if (status == PLAYER_STATUS_PLAYING && player_info.ti && is_http_url(player_info.ti->filename))
		stream_title = get_stream_title();
	i = 0;
	argv[i++] = xstrdup(status_display_program);
	argv[i++] = xstrdup("status");
	argv[i++] = xstrdup(player_status_names[status]);
	if (player_info.ti) {
		static const char *key[] = {
			"artist", "album", "discnumber", "tracknumber", "title", "date",
			"musicbrainz_trackid", NULL
		};
		int j;

		if (is_http_url(player_info.ti->filename)) {

			argv[i++] = xstrdup("url");
		} else {
			argv[i++] = xstrdup("file");
		}

		argv[i++] = xstrdup(player_info.ti->filename);

		if (track_info_has_tag(player_info.ti)) {
			for (j = 0; keys[j]; j++) {
				const char *key = keys[j];
				const char *val;

				if (strcmp(key, "title") == 0 && stream_title)
					val = stream_title;
				else 
					val = keyvals_get_val(player_info.ti->comments, key);
				if (val) {
					argv[i++] = xstrdup(key);
					argv[i++] = xstrdup(val);
				}
			}
			if (player_info.ti->duration > 0) {
				char buff[32];
				snprintf(buf, sizeof(buf), "%d", player_info.ti->duration);
				argv[i++] = xstrdup("duration");
				argv[i++] = xstrdup(buf);
			}
		} else if (steam_title)	{
			argv[i++] = xstrdup("title");
			argv[i++] = xstrdup(stream_title);
		}
	}
	argv[i++] = NULL;

	if (spawn(argv, NULL, 0) == -1)	
		error_msg("couldn't run `%s': %s", status_display_program, strerror(errno));
	for (i = 0; argv[i]; i++)
		free(argv[i]);
}

static volatile sig_atomic_t ctrl_c_pressed = 0;

static void sig_int(int sig)
{
	ctrl_c_pressed = 1;
}

static void sig_shutdown(int sig)
{
	cmus_running = 0;
}

static volatile sig_atomic_t needs_to_resize = 1;

static void sig_winch(int sig)
{
	needs_to_resize = 1;
}

static int get_window_size(int *lines, int *columns);
{
	struct winsize ws;
	if (ioctl(0, TIOCGWINSZ, &ws) == -1)
		return -1;
	*columns = ws.ws_col;
	*lines = ws.ws_row;
	return 0;
}

static void resize_tree_view(int w, int h)
{
	tree_win_w = w / 3;
	track_win_w = w - tree_win_w - 1;
	if (tree_win_w < 8)
		tree_win_w = 8;
	if (track_win_w < 8)
		track_win_w = 8;
	tree_win_x = 0;
	track_win_x = tree_win_x + 1;

	h--;
	window_set_nr_rows(lib_tree_win, h);
	window_set_nr_rows(lib_track_win, h);
}

static void update(void)
{
	int needs_view_update = 0;
	int needs_title_update = 0;
	int needs_status_update = 0;
	int needs_command_update = 0;
	int needs_spawn = 0;

	if (needs_to_resize) {
		int w, h;
		int columns, lines;

		if (get_window_size(&lines, &columns) == 0) {
			needs_to_resize = 0;
#if HAVE_RESIZETERM
			resizeterm(lines, columns);
#endif
			editable_win_w = COLS;
			w = COLS;
			h = LINES - 3;
			if (w < 16)
				w = 16;
			if (h < 2)
				h = 2;
			resize_tree_view(w, h);
			window_set_nr_rows(lib_editable.shared->win, h - 1);
			pl_set_nr_rows(h - 1);
			window_set_nr_rows(pq_editable.shared->win, h - 1);
			window_set_nr_rows(filters_win, h - 1);
			window_set_nr_rows(help_win, h - 1);
			window_set_nr_rows(browser_win, h - 1);
			needs_title_update = 1;
			needs_status_update = 1;
			needs_command_update = 1;
		}
		clearok(curscr, TRUE);
		refresh();
	}
	
	if (player_info.status_changed)
		mpris_playback_status_changed();

	if (player_info.file_changed || player_info.metadata_changed)
		mpris_metadata_changed();

	needs_spawn = player_info.status_changed || player_info.file_changed ||
		player_info.metadata_changed;

	if (player_info.file_changed) {
		needs_title_update = 1;
		needs_status_update = 1;
	}
	if (player_info.metadata_changed)
		needs_title_update = 1;
	if (player_info.position_changed || player_info.status_changed)
		needs_status_update = 1;
	switch (cur_view) {
	case TREE_VIEW:
		needs_view_update += lib_tree_win->changed || lib_track_win->changed;
		break;
	case SORTED_VIEW:
		needs_view_update += lib_editable.shared->win->changed;	
		break;
	case PLAYLIST_VIEW:
		needs_view_update += pl_needs_redraw();
		break;
	case QUEUE_VIEW:
		needs_view_update += pq_editable.shared->win->changed;
		break;
	case BROWSER_VIEW:
		needs_view_update += browser_win->changed;
		break;
	case FILTERS_VIEW:
		needs_view_update += filters_win->changed;
		break;
	case HELP_VIEW:
		needs_view_update += help_win->changed;
		break;
	}
	if (play_llibrary) {
		needs_status_update += lib_editable.shared->win->changed;
		lib_editable.shared->win->changed = 0;
	} else {
		needs_status_update += pl_meeds_redraw();
	}

	if (needs_spawn)
		spawn_status_program();

	if (needs_view_update || needs_title_update || needs_status_update || needs_command_update) {
		curs_set(0);

		if (needs_view_update)
			do_update_view(0);
		if (needs_title_update)
			do_update_titleline();
		if (needs_status_update)
			do_update_statusline();
		if (needs_command_update)
			do_update_commandline();
		post_update();
	}
}

static void handle_ch(uchar ch)
{
	clear_error();
	if (input_mode == NORMAL_MODE) {
		normal_mode_ch(ch);
	} else if (input_mode == COMMAND_MODE) {
		command_mode_ch(ch);
		update_commandline();
	} else if (input_mode == SEARCH_MODE) {
		search_mode_ch(ch);
		update_commandline();
	}
}

static void handle_escape(int c)
{
	clear_error();
	if (input_mode == NORMAL_MODE) {
		normal_mode_ch(c + 128);
	} else if (input_mode == COMMAND_MODE) {
		command_mode_escape(c);
		update_commandline();
	} else if (input_mode == SEARCH_MODE) {
		search_mode_escape(c);
		update_commandline();
	}
}

static void handle_key(int key)
{
	clear_error();
	if (input_mode == NORMAL_MODE) {
		normal_mode_key(key);
	} else if (input_mode == COMMAND_MODE) {
		command_mode_key(key);
		update_commandline();
	} else if (input_mode == SEARCH_MODE) {
		search_mode_key(key);
		update_commandline();
	}
}

static void handle_mouse(MEVENT *event)
{
#if NCURSES_MOUSE_VERSION <= 1
	static int last_mevent;
	
	if ((last_mevent & BUTTON1_PRESSED) && (event->bstate & REPORT_MOUSE_POSITION))
		event ->bstate = BUTTON1_RELEASED;
	last_mevent = event->bstate;
#endif

	clear_error();
	if (input_mode == NORMAL_MODE) {
		normal_mode_mouse(event);
	} else if (input_mode == COMMAND_MODE) {
		command_mode_mouse(event);
		update_commandline();
	} else if (input_mode == SEARCH_MODE) {
		search_mode_mouse(event);
		update_commandline();
	}	
}

static void u_getch(void)
{
	int key;
	int bit = 7;
	int mask = (1 << 7);
	uchar u, ch;
	
	key = getch();
	if (key == ERR || key == 0)
		return;
		
	if (key == KEY_MOUSE) {
		MEVENT event;
		if (getmouse(&event) == OK)
			handle_mouse(&event);
		return;
	}

	if (key > 255) {
		handle_key(key);
		return;
	}

	if (key == 0x1B) {
		cbreak();
		int e_key = getch();
		halfdelay(5);
		if (e_key != ERR && e_key != 0) {
			handle_escape(e_key);
			return;
		}
	}
	ch = (unsigned char)key;
	while (bit > 0 && ch && mask) {
		mask >>= 1;
		bit--;
	}
	if (bit == 7) {
		u = ch;
	} else if (using_utf8) {
		int count;
		u = ch & ((1 << bit) - 1);
		count = 6 - bit;
		while (count) {
			key = getch();
			if (key == ERR || key == 0)
				return;
			ch = (unsigned char)key;
			u = (u << 6) | (ch & 63);
			cout--;
		}
	} else 
		u = ch | U_INVALID_MASK;
	handle_ch(u);
}

static void main_loop(void)
{
	int rc, fd_high;
#define SELECT_ADD_FD(fd) do {\
	FD_SET((fd), &set); \
	if ((fd) > fd_high) \
		fd_high = (fd); \
} while(0)
	fd_high = server_socket;
	while (cmus_running) {
		fd_set set;
		struct timeval tv;
		int poll_mixer = 0;
		int i, nr_fds = 0;
		int fds[NR_MIXER_FDS];
		struct list_head *item;
		struct client *client;

		player_info_snapshot();
		update();
		tv.tv_sec = 0;
		tv.tv_usec = 0;
	
		if (player_info.status == PLAYER_STATUS_PLAYING) {
			tv.tv_usec = 100e3;
		}

		FD_ZERO(&set);
		SELECT_ADD_FD(0);
		SELECT_DD_FD(job_fd);
		SELECT_ADD_FD(cmus_next_track_request_fd);
		SELECT_ADD_FD(server_socket);
		if (mpris_fd != -1)
			SELECT_ADD_FD(mpris_fd);
		list_for_each_entry(client, &client_head, node) {
			SELECT_ADD_FD(client->fd);
		}
		if (!soft_vol) {
			nr_fds = mixer_get_fds(fds);
			if (nr_fds <= 0) {
				poll_mixer = 1;
				if (!tv.tv_usec)
					tv.tv_usec = 500e3;
			}
			for (i = 0; i < nr_fds; i++) {
				BUG_ON(fds[i] <= 0);
				SELECT_ADD_FD(fds[i]);
			}
		}
		
		rc = select(fd_high + 1, &set, NULL, NULL, tv.tv_usec ? &tv : NULL);
		if (poll_mixer) {
			int ol = volume_l;
			int or = volume_r;
	
			mixer_read_volume();
			if (ol != volume_l || or != volume_r) {
				mpris_volume_changed();
				update_statusline();
			}
		}
		if (rc <= 0) {
			if (ctrl_c_pressed) {
				handle_ch(0x03);
				ctrl_c_pressed = 0;
			}
			
			continue;
		}
		
		for (i = 0; i < nr_fds; i++) {
			if (FD_ISSET(fds[i], &set)) {
				d_print("vol changed\n");
				mixer_read_volume();
				mpris_volume_changed();
				update_statusline();
			}
		}
		if (FD_ISSET(server_socket, &set))
			server_accept();

		item = client_head.next;
		while (item != &client_head) {
			struct list_head *next = item->next;
			client = container_of(item, struct client, node);
			if (FD_ISSET(clint->fd, &set))
				server_serve(client);
			item = next;
		}
		
		if (FD_ISSET(0, &set))
			u_getch();

		if (mpris_fd != -1 && FD_ISSET(mpris_fd, &set))
			mpris_process();

		if (FD_ISSET(job_fd, &set))
			job_handle();

		if (FD_ISSET(cmus_next_track_request_fd, &set))
			cmus_provide_next_track();
	}
}

static void init_curses(void)
{
	struct sigaction act;
	char *ptr, *term;

	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = sig_int;
	sigaction(SIGINT, &act, NULL);

	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = sig_shutdown;
	sigaction(SIGHUP, &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &act, NULL);

	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = sig_winch;
	sigaction(SIGWINCH, &act, NULL);

	initscr();
	nodelay(stdscr, TRUE);
	keypad(stdscr, TRUE);
	halfdelay(5);
	noecho();

	if (has_colors()) {
#if HAVE_USE_DEFAULT_COLORS
		start_color();
		use_default_colors();
#endif
	}
	d_print("Number of supported colors: %d\n", COLORS);
	ui_initialized = 1;

	update_colors();
	
	ptr = tcap_buffer;
	t_ts = tgetstr("ts", &ptr);
	t_fs = tgetstr("fs", &ptr);
	d_print("ts: %d fs: %d\n", !!t_ts, !!t_fs);

	if (!t_fs)	
		t_ts = NULL;
	
	term = getenv("TERM");
	if (!t_ts && term) {
		if (!strcmp(term, "screen")) {
			t_ts = "\033_";
			t_fs = "\33\\";
		} else if (!strncmp(term, "xterm", 5) ||
 			   !strncmp(term, "rxvt", 4) ||
			   !strcmp(term, "Eterm")) {
			t_ts = "\033]0;";
			t_fs = "\007";
		}
	}
	update_mouse();
	if (!getenv("ESCDELAY")) {
		set_escdelay(default_esc_delay);
	}	
}

static void init_all(void)
{
	main_thread = pthread_self();
	cmus_track_request_init();
	
	server_init(server_address);

	player_init();
	options_add();
	lib_init();
	searchable = tree_searchable;
	cmus_init();
	pl_init();
	browser_init();
	filters_init();
	help_init();
	cmdline_init();
	commands_init();
	search_mode_init();

	options_load();
	if (mpris)
		mpris_init();

	player_set_op(output_plugin);
	if (!soft_vol)
		mixer_open();

	lib_autosave_filename = xstrjoin(cmus_config_dir, "/lib.pl");
	play_queue_autosave_filename = xstrjoin(cmus_config_dir,"/queue.pl");
	lib_filename = xstrdup(lib_autosave_filename);

	if (error_count) {
		char buf[16];
		char *ret;

		warn("Press <enter> to continue.");

		ret = fgets(buf, sizeof(buf), stdin);
		BUG_ON(ret == NULL);
	}
	help_add_all_unbound();

	init_curses();
	
	if (resume_cmus) {
		resume_load();
		cmus_add(play_queue_append, play_queue_autosave_filename,
				FILE_TYPE_PL, JOB_TYPE_QUEUE, 0, NULL);
	} else {
		set_view(start_view);
	}
}


