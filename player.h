#ifndef CMUS_PLAYER_H
#define CMUS_PLAYER_H

#include "locking.h"
#include "track-info.h"

#include <pthread.h>

enum {
	PLAYER_ERROR_SUCCESS,
	PLAYER_ERROR_ERRNO,
	PLAYER_ERROR_NOT_SUPPORTED
};

extern const char * const player_status_names[];
enum player_status {
	PLAYER_STATUS_STOPPED,
	PLAYER_STATUS_PLAYING,
	PLAYER_STATUS_PAUSED,
	NR_PlAYER_STATUS
};

enum replaygain {
	RG_DISABLED,
	RG_TRACK,
	RG_ALBUM,
	RG_TRACK_PREFERRED,
	RG_ALBUM_PREFERRED
};

struct player_info {
	struct track_info *ti;

	enum player_status status;
	int pos;
	int current_bitrate;

	int buffer_fill;
	int buffer_size;

	char *error_msg;

	unsigned int file_changed : 1;
	unsigned int metadata_changed : 1;
	unsigned int status_changed : 1;
	unsigned int position_changed : 1;
	unsigned int buffer_fill_changed : 1;
};

extern char player_metadata[255 * 16 + 1];
extern struct player_info player_info;
extern int player_cont;
extern int player_repeat_current;
extern enum replaygain replaygain;
extern int replaygain_limit;
extern double replaygain_preamp;
extern int soft_vol;
extern int soft_vol_l;
extern int soft_vol_r;

void player_init(void);
void player_exit(void);

void player_set_file(struct track_info *ti);
void player_play_file(struct track_info *ti);
void player_file_changed(struct track_info *ti);
void player_play(void);
void player_stop(void);
void player_pause(void);
void player_pause_playback(void);
void player_seek(double offset, int relative, int start_playing);
void player_set_op(const char *name);
void player_set_buffer_chunks(unsigned int nr_chunks);
int player_get_buffer_chunks(void);
void player_info_snapshot(void);

void player_set_soft_volume(int l, int r);
void player_set_soft_vol(int soft);
void player_set_rg(enum replaygain rg);
void player_set_rg_limit(int limit);
void player_set_rg_preamp(double db);

#define VF_RELATIVE		0x01
#define VF_PERCENTAGE	0x02
int player_set_vol(int l, int lf, int r, int rf);

void player_metadata_lock(void);
void player_metadata_unlock(void);

#endif

