/***************************************************************************

CuboCD32 definitions

***************************************************************************/

#ifndef __CUBOCD32_H__
#define __CUBOCD32_H__

extern void amiga_akiko_init(void);
extern NVRAM_HANDLER( cd32 );
extern READ32_HANDLER(amiga_akiko32_r);
extern WRITE32_HANDLER(amiga_akiko32_w);

#endif /* __CUBOCD32_H__ */
