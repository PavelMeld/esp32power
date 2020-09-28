#ifndef __LAMP_H
#define __LAMP_H

enum lamp_mode_e	{
	LAMP_MODE_R1 = 0,	/* Lamp 1 */
	LAMP_MODE_R2,		/* Lamp 2 */
	LAMP_MODE_R12,		/* Both */
	LAMP_MODE_OFF,		/* OFF    */
	LAMP_MODES_COUNT,
};

enum lamp_mode_e lamp_get_mode();
void lamp_set_mode(enum lamp_mode_e	mode);
void lamp_next_visible_mode();
void lamp_save_mode();
void lamp_init();

#endif
