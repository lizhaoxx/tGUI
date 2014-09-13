// terminal.c Êä³ö½Ó¿Ú

#ifndef __TERMINAL_H__
#define __TERMINAL_H__

/*============================ INCLUDES ======================================*/
#include ".\app_cfg.h"

#if USE_SERVICE_GUI_TGUI == ENABLED
#include "..\grid\interface.h"

/*============================ MACROS ========================================*/
/*============================ MACROFIED FUNCTIONS ===========================*/
/*============================ TYPES =========================================*/
/*============================ GLOBAL VARIABLES ==============================*/
/*============================ LOCAL VARIABLES ===============================*/
/*============================ PROTOTYPES ====================================*/

//! terminal interface
extern const i_gdc_t terminal;

#endif  /* USE_SERVICE_GUI_TGUI == ENABLED */

#endif  /* __TERMINAL_H__ */


/* EOF */
