/*************************************************************************

    8080bw.h

*************************************************************************/

#include "sound/discrete.h"


#define CABINET_PORT_TAG	"CAB"


/*----------- defined in drivers/8080bw.c -----------*/

UINT8 polaris_get_cloud_pos(void);


/*----------- defined in audio/8080bw.c -----------*/

WRITE8_HANDLER( invadpt2_sh_port_1_w );
WRITE8_HANDLER( invadpt2_sh_port_2_w );

WRITE8_HANDLER( spcewars_sh_port_w );

WRITE8_HANDLER( lrescue_sh_port_1_w );
WRITE8_HANDLER( lrescue_sh_port_2_w );
extern struct Samplesinterface lrescue_samples_interface;

WRITE8_HANDLER( cosmo_sh_port_2_w );

WRITE8_HANDLER( ballbomb_sh_port_1_w );
WRITE8_HANDLER( ballbomb_sh_port_2_w );

WRITE8_HANDLER( indianbt_sh_port_1_w );
WRITE8_HANDLER( indianbt_sh_port_2_w );
WRITE8_HANDLER( indianbt_sh_port_3_w );
extern discrete_sound_block indianbt_discrete_interface[];

WRITE8_HANDLER( polaris_sh_port_1_w );
WRITE8_HANDLER( polaris_sh_port_2_w );
WRITE8_HANDLER( polaris_sh_port_3_w );
extern discrete_sound_block polaris_discrete_interface[];

MACHINE_RESET( schaser );
MACHINE_START( schaser );
WRITE8_HANDLER( schaser_sh_port_1_w );
WRITE8_HANDLER( schaser_sh_port_2_w );
extern struct SN76477interface schaser_sn76477_interface;
extern discrete_sound_block schaser_discrete_interface[];

WRITE8_HANDLER( rollingc_sh_port_w );

WRITE8_HANDLER( invrvnge_sh_port_w );

WRITE8_HANDLER( lupin3_sh_port_1_w );
WRITE8_HANDLER( lupin3_sh_port_2_w );

WRITE8_HANDLER( schasrcv_sh_port_1_w );
WRITE8_HANDLER( schasrcv_sh_port_2_w );

WRITE8_HANDLER( yosakdon_sh_port_1_w );
WRITE8_HANDLER( yosakdon_sh_port_2_w );

WRITE8_HANDLER( shuttlei_sh_port_1_w );
WRITE8_HANDLER( shuttlei_sh_port_2_w );


/*----------- defined in video/8080bw.c -----------*/

extern UINT8 *c8080bw_colorram;

void c8080bw_flip_screen_w(int data);
void c8080bw_screen_red_w(int data);
void schaser_background_control_w(int data);

VIDEO_UPDATE( invadpt2 );
VIDEO_UPDATE( ballbomb );
VIDEO_UPDATE( schaser );
VIDEO_UPDATE( schasrcv );
VIDEO_UPDATE( rollingc );
VIDEO_UPDATE( polaris );
VIDEO_UPDATE( lupin3 );
VIDEO_UPDATE( cosmo );
VIDEO_UPDATE( indianbt );
VIDEO_UPDATE( shuttlei );
VIDEO_UPDATE( sflush );
