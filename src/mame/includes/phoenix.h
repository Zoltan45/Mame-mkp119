#include "sound/discrete.h"
#include "sound/custom.h"


/*----------- defined in audio/phoenix.c -----------*/

extern discrete_sound_block phoenix_discrete_interface[];

WRITE8_HANDLER( phoenix_sound_control_a_w );
WRITE8_HANDLER( phoenix_sound_control_b_w );

void *phoenix_sh_start(int clock, const struct CustomSound_interface *config);

/*----------- defined in audio/pleiads.c -----------*/

WRITE8_HANDLER( pleiads_sound_control_a_w );
WRITE8_HANDLER( pleiads_sound_control_b_w );
WRITE8_HANDLER( pleiads_sound_control_c_w );

void *pleiads_sh_start(int clock, const struct CustomSound_interface *config);
void *naughtyb_sh_start(int clock, const struct CustomSound_interface *config);
void *popflame_sh_start(int clock, const struct CustomSound_interface *config);

/*----------- defined in video/naughtyb.c -----------*/

extern UINT8 *naughtyb_videoram2;
extern UINT8 *naughtyb_scrollreg;
extern int naughtyb_cocktail;

WRITE8_HANDLER( naughtyb_videoram2_w );
WRITE8_HANDLER( naughtyb_videoreg_w );
WRITE8_HANDLER( popflame_videoreg_w );

VIDEO_START( naughtyb );
PALETTE_INIT( naughtyb );
VIDEO_UPDATE( naughtyb );


/*----------- defined in video/phoenix.c -----------*/

PALETTE_INIT( phoenix );
PALETTE_INIT( pleiads );
VIDEO_START( phoenix );
VIDEO_UPDATE( phoenix );

WRITE8_HANDLER( phoenix_videoram_w );
WRITE8_HANDLER( phoenix_videoreg_w );
WRITE8_HANDLER( pleiads_videoreg_w );
WRITE8_HANDLER( phoenix_scroll_w );

READ8_HANDLER( phoenix_input_port_0_r );
READ8_HANDLER( pleiads_input_port_0_r );
READ8_HANDLER( survival_input_port_0_r );
READ8_HANDLER( survival_protection_r );

