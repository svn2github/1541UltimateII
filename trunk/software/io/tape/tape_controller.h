/*************************************************************/
/* Tape Emulator / Recorder Control                          */
/*************************************************************/
#include <stdio.h>
#include "event.h"
#include "poll.h"
#include "menu.h"
#include "iomap.h"

#define PLAYBACK_STATUS  *((volatile BYTE *)(C2N_PLAY_BASE + 0x000))
#define PLAYBACK_CONTROL *((volatile BYTE *)(C2N_PLAY_BASE + 0x000))
#define PLAYBACK_DATA     ((volatile BYTE *)(C2N_PLAY_BASE + 0x800))

#define C2N_ENABLE      0x01
#define C2N_CLEAR_ERROR 0x02
#define C2N_FLUSH_FIFO  0x04
#define C2N_MODE_SELECT 0x08
#define C2N_SENSE		0x10
#define C2N_OUT_READ    0x00
#define C2N_OUT_WRITE   0x40
#define C2N_OUT_WRITE_N 0x80
#define C2N_OUT_TOGGLE  0xC0

#define C2N_STAT_ENABLED    0x01
#define C2N_STAT_ERROR      0x02
#define C2N_STAT_FIFO_FULL  0x04
#define C2N_STAT_FIFO_AF    0x08
#define C2N_STAT_STREAM_EN  0x40
#define C2N_STAT_FIFO_EMPTY 0x80

class TapeController : public ObjectWithMenu
{
	FILE *file;
	uint32_t length;
	int   block;
    int   mode;
	int   paused;
	int   recording;
	uint8_t  controlByte;
	uint8_t  *blockBuffer;
	void read_block();
public:
	TapeController();
	virtual ~TapeController();
	
	int  fetch_task_items(IndexedList<Action*> &item_list);
	static void exec(void *obj, void *param);
	
	void close();
	void stop();
	void start(int);
	void poll(Event &);
	void set_file(FILE *f, uint32_t, int);
};

extern TapeController *tape_controller;
