// terminal.c

/*============================ INCLUDES ======================================*/
#include ".\app_cfg.h"

#if USE_SERVICE_GUI_TGUI == ENABLED
#include "..\interface.h"

/*============================ MACROS ========================================*/
#define TGUI_TERMINAL_CLEAR_CODE	    (0x0C)

#define WIDTH                           (80)
#define HEIGHT                          (24)

// termianal write byte
#ifndef TGUI_TERMINAL_WRITE_BYTE
#   error No defined TGUI_TERMINAL_WRITE_BYTE
#endif

// termianal read byte
#ifndef TGUI_TERMINAL_READ_BYTE
#   error No defined TGUI_TERMINAL_READ_BYTE
#endif

/*============================ MACROFIED FUNCTIONS ===========================*/

/*============================ TYPES =========================================*/

//! terminal command output control block
typedef struct {
    enum {
        TERMINAL_PRN_STR_START = 0,     //!< stream output start status
        TERMINAL_PRN_STR_CHECK,         //!< stream output cheak status
        TERMINAL_PRN_STR_SEND           //!< stream output send status
    } tState;                           //!< stream output status
    uint8_t *pchStr;                    //!< stream buffer address
    uint_fast16_t hwSize;               //!< stream buffer size
} terminal_prn_str_t;

/*============================ GLOBAL VARIABLES ==============================*/
/*============================ LOCAL VARIABLES ===============================*/
static grid_brush_t s_tCurrentGridBrush;

/*============================ PROTOTYPES ====================================*/
//! initialize terminal stream output 
static bool terminal_init_prn_str(  terminal_prn_str_t *ptPRN,
                                    uint8_t *pchStr,
                                    uint_fast16_t hwSize);

//! terminal stream output 
static fsm_rt_t terminal_prn_str(terminal_prn_str_t *ptPRN);

/*============================ IMPLEMENTATION ================================*/

#define TERMINAL_SET_GRID_RESET()           \
    do {                                    \
        s_tState = TERMINAL_SET_GRID_START; \
    } while(0)

/*! \brief set current cursor position
 *! \param tGrid cursor position
 *! \retval fsm_rt_on_going set grid on going
 *! \retval fsm_rt_cpl set grid finish
 */
fsm_rt_t terminal_set_grid(grid_t tGrid)
{
    static enum {
        TERMINAL_SET_GRID_START = 0,
        TERMINAL_SET_GRID_SEND
    } s_tState = TERMINAL_SET_GRID_START;
    uint8_t chRow, chColumn;
    static uint8_t s_chSetCode[8];
    static terminal_prn_str_t s_tPrn;

    fsm_rt_t tfsm;
    
    switch ( s_tState ) {
        case TERMINAL_SET_GRID_START:
            chRow = HEIGHT - tGrid.chTop;
            chColumn = WIDTH - tGrid.chLeft;
            s_chSetCode[0] = 0x1B;
            s_chSetCode[1] = '[';
            s_chSetCode[2] = ( chRow / 10 ) + 0x30;
            s_chSetCode[3] = ( chRow % 10 ) + 0x30;
            s_chSetCode[4] = ';';
            s_chSetCode[5] = ( chColumn / 10 ) + 0x30;
            s_chSetCode[6] = ( chColumn % 10 ) + 0x30;
            s_chSetCode[7] = 'H';
            terminal_init_prn_str(&s_tPrn, s_chSetCode, 8);
            s_tState = TERMINAL_SET_GRID_SEND;
            // break;

        case TERMINAL_SET_GRID_SEND:
            tfsm = terminal_prn_str(&s_tPrn);
            if ( IS_FSM_ERR(tfsm) ) {
                return tfsm;
            } else if ( fsm_rt_cpl == tfsm ) {
                TERMINAL_SET_GRID_RESET();
                return tfsm;
            }
            break;
    }
    
    return fsm_rt_on_going;
}


#define TERMINAL_GET_GRID_RESET()           \
    do {                                    \
        s_tState = TERMINAL_GET_GRID_START; \
    } while(0)

/*! \brief get current cursor position
 *! \param tGrid cursor position
 *! \retval fsm_rt_on_going set grid finish
 *! \retval fsm_rt_cpl set grid on going
 */
fsm_rt_t terminal_get_grid(grid_t *ptGrid)
{
    static enum {
        TERMINAL_GET_GRID_START = 0,
        TERMINAL_GET_GRID_SEND,
        TERMINAL_GET_GRID_RECEIVE,
        TERMINAL_GET_GRID_CHECK
    } s_tState = TERMINAL_GET_GRID_START;
    
    static uint8_t s_chCmdCode[4];
    static uint8_t s_chReceiveCode[8];
    static uint8_t s_chReceiveCnt;
    static uint8_t s_chRow, s_chColumn;
    static terminal_prn_str_t s_tPrn;
    fsm_rt_t tfsm;

	if ( NULL == ptGrid ) {
		return fsm_rt_err;
	}
    
    switch ( s_tState ) {
        case TERMINAL_GET_GRID_START:
            s_chCmdCode[0] = 0x1B;
            s_chCmdCode[1] = '[';
            s_chCmdCode[2] = '6';
            s_chCmdCode[3] = 'n';
            terminal_init_prn_str(&s_tPrn, s_chCmdCode, 4);
            s_tState = TERMINAL_GET_GRID_SEND;
            // break;

        case TERMINAL_GET_GRID_SEND:
            tfsm = terminal_prn_str(&s_tPrn);
            if ( IS_FSM_ERR(tfsm) ) {
                return tfsm;
            } else if ( fsm_rt_cpl == tfsm ) {
                s_chReceiveCnt = 0;
                s_tState = TERMINAL_GET_GRID_RECEIVE;
            }
            break;

        case TERMINAL_GET_GRID_RECEIVE: {
            uint8_t chTemp;
            if ( TGUI_TERMINAL_READ_BYTE(&chTemp) ) {
                s_chReceiveCode[s_chReceiveCnt] = chTemp;
                s_chReceiveCnt++;
                if ( s_chReceiveCnt > 8 ) {
                    return fsm_rt_err;
                }
                if ( 'R' == chTemp ) {
                    s_tState = TERMINAL_GET_GRID_CHECK;
                }
            }
            break;
        case TERMINAL_GET_GRID_CHECK:
            if ( ';' == s_chReceiveCode[3] ) {
                s_chRow = s_chReceiveCode[2] - 0x30;
                if ( 'R' == s_chReceiveCode[5] ) {
                    s_chColumn = s_chReceiveCode[4] - 0x30;
                } else if ( 'R' == s_chReceiveCode[6] ) {
                    s_chColumn = ( s_chReceiveCode[4] - 0x30 ) * 10;
                    s_chColumn += ( s_chReceiveCode[5] - 0x30 );
                } else {
                    return fsm_rt_err;
                }
            } else if ( ';' == s_chReceiveCode[4] ) {
                s_chRow = ( s_chReceiveCode[2] - 0x30 ) * 10;
                s_chRow += ( s_chReceiveCode[3] - 0x30 );
                if ( 'R' == s_chReceiveCode[6] ) {
                    s_chColumn = s_chReceiveCode[5] - 0x30;
                } else if ( 'R' == s_chReceiveCode[7] ) {
                    s_chColumn = ( s_chReceiveCode[5] - 0x30 ) * 10;
                    s_chColumn += ( s_chReceiveCode[6] - 0x30 );
                } else {
                    return fsm_rt_err;
                }
            } else {
                return fsm_rt_err;
            }
            ptGrid->chTop = HEIGHT - s_chRow;
            ptGrid->chLeft = WIDTH - s_chColumn;
            TERMINAL_GET_GRID_RESET();
            return fsm_rt_cpl;
            // break;
        }
    }
    
    return fsm_rt_on_going;
}

#define TERMINAL_SAVE_CURRENT_RESET()           \
    do {                                        \
        s_tState = TERMINAL_SAVE_CURRENT_START; \
    } while(0)    
/*! \brief save current cursor position
 *! \param none
 *! \retval fsm_rt_on_going save grid on going
 *! \retval fsm_rt_cpl save grid on finish
 */
fsm_rt_t terminal_save_current(void)
{
    static enum {
        TERMINAL_SAVE_CURRENT_START = 0,
        TERMINAL_SAVE_CURRENT_SEND
    } s_tState = TERMINAL_SAVE_CURRENT_START;
    static uint8_t s_chSaveCode[3];
    static terminal_prn_str_t s_tPrn;
    fsm_rt_t tfsm;
    
    switch ( s_tState ) {
        case TERMINAL_SAVE_CURRENT_START:
            s_chSaveCode[0] = 0x1B;
            s_chSaveCode[1] = '[';
            s_chSaveCode[2] = 's';
            terminal_init_prn_str(&s_tPrn, s_chSaveCode, 3);
            s_tState = TERMINAL_SAVE_CURRENT_SEND;
            // break;

        case TERMINAL_SAVE_CURRENT_SEND:
            tfsm = terminal_prn_str(&s_tPrn);
            if ( IS_FSM_ERR(tfsm) ) {
                return tfsm;
            } else if ( fsm_rt_cpl == tfsm ) {
                TERMINAL_SAVE_CURRENT_RESET();
                return tfsm;
            }
            break;
    }
    
    return fsm_rt_on_going;
}


#define TERMINAL_RESUME_RESET()             \
    do {                                    \
        s_tState = TERMINAL_RESUME_START;   \
    } while(0)    

/*! \brief resume current cursor position
 *! \param none
 *! \retval fsm_rt_on_going set grid on going
 *! \retval fsm_rt_cpl resume grid complete
 */
fsm_rt_t terminal_resume(void)
{
    static enum {
        TERMINAL_RESUME_START = 0,
        TERMINAL_RESUME_SEND
    } s_tState = TERMINAL_RESUME_START;
    static uint8_t s_chResumeCode[3];
    static terminal_prn_str_t s_tPrn;
    fsm_rt_t tfsm;
    
    switch ( s_tState ) {
        case TERMINAL_RESUME_START:
            s_chResumeCode[0] = 0x1B;
            s_chResumeCode[1] = '[';
            s_chResumeCode[2] = 'u';
            terminal_init_prn_str(&s_tPrn, s_chResumeCode, 3);
            s_tState = TERMINAL_RESUME_SEND;
            break;

        case TERMINAL_RESUME_SEND:
            tfsm = terminal_prn_str(&s_tPrn);
            if ( IS_FSM_ERR(tfsm) ) {
                return tfsm;
            } else if ( fsm_rt_cpl == tfsm ) {
                TERMINAL_RESUME_RESET();
                return tfsm;
            }
            break;
    }
    
    return fsm_rt_on_going;
}

#define TERMINAL_SET_BRUSH_RESET()	            \
    do {                                        \
        s_tState = TERMINAL_SET_BRUSH_START;    \
    }	while(0)    
/*! \brief set display attribute
 *! \param tBrush display attribute
 *! \retval fsm_rt_on_going set brush on going
 *! \retval fsm_rt_cpl set brush finish
 */
fsm_rt_t terminal_set_brush(grid_brush_t tBrush)
{
	static enum {
		TERMINAL_SET_BRUSH_START = 0,
		TERMINAL_SET_BRUSH_SEND
	} s_tState = TERMINAL_SET_BRUSH_START;
	static uint8_t s_chCmdCode[8];
	static terminal_prn_str_t s_tPrn;
    fsm_rt_t tfsm;
	
	switch ( s_tState ) {
		case TERMINAL_SET_BRUSH_START:
			s_tCurrentGridBrush = tBrush;
			if ( ( tBrush.tForeground.tValue > 7 ) || ( tBrush.tBackground.tValue > 7 ) ) {
				return fsm_rt_err;
			}
			s_chCmdCode[0] = 0x1B;
			s_chCmdCode[1] = '[';
			s_chCmdCode[2] = '3';
			s_chCmdCode[3] = tBrush.tForeground.tValue + 0x30;
			s_chCmdCode[4] = ';';
			s_chCmdCode[5] = '4';
			s_chCmdCode[6] = tBrush.tBackground.tValue + 0x30;
			s_chCmdCode[7] = 'm';
			terminal_init_prn_str(&s_tPrn, s_chCmdCode, 8);
			break;
		case TERMINAL_SET_BRUSH_SEND:
			tfsm = terminal_prn_str(&s_tPrn);
			if ( IS_FSM_ERR(tfsm) ) {
				return tfsm;
			} else if ( fsm_rt_cpl == tfsm ) {
				TERMINAL_SET_BRUSH_RESET();
				return tfsm;
			}
			break;
	}

	return fsm_rt_on_going;
}

/*! \brief get display attribute
 *! \param none
 *! \return display attribute
 */
grid_brush_t terminal_get_brush(void)
{
	return s_tCurrentGridBrush;
}

/*! \brief terminal clear
 *! \param none
 *! \retval fsm_rt_on_going terminal clear on going
 *! \retval fsm_rt_cpl terminal clear finish
 */
fsm_rt_t terminal_clear(void)
{
	if ( TGUI_TERMINAL_WRITE_BYTE(TGUI_TERMINAL_CLEAR_CODE) ) {
		return fsm_rt_cpl;
	}
	return fsm_rt_on_going;
}

#define TERMINAL_PRINT_RESET()              \
    do {                                    \
        s_tState = TERMINAL_PRINT_START;    \
    } while(0)
/*! \brief stream output
 *! \param ptPRN stream output control block
 *! \param 
 *! \retval fsm_rt_on_going stream output on going
 *! \retval fsm_rt_cpl stream output clear finish
 */
fsm_rt_t terminal_print(uint8_t *pchString, uint_fast16_t hwSize)
{
    static enum {
        TERMINAL_PRINT_START = 0,
        TERMINAL_PRINT_SEND
    } s_tState = TERMINAL_PRINT_START;
    static terminal_prn_str_t s_tPrn;
    fsm_rt_t tfsm;
    
    if ( NULL == pchString ) {
        return fsm_rt_err;
    }
    
    switch ( s_tState ) {
        case TERMINAL_PRINT_START:
            terminal_init_prn_str(&s_tPrn, pchString, hwSize);
            s_tState = TERMINAL_PRINT_SEND;
            // break;
        case TERMINAL_PRINT_SEND:
            tfsm = terminal_prn_str(&s_tPrn);
            if ( IS_FSM_ERR(tfsm) ) {
                return tfsm;
            } else if ( fsm_rt_cpl == tfsm ) {
                TERMINAL_PRINT_RESET();
                return tfsm;
            }
            break;
    }
    
    return fsm_rt_on_going;
}

#define TERMINAL_PRN_STR_RESET()                \
    do {                                        \
        ptPRN->tState = TERMINAL_PRN_STR_START; \
    } while(0)
/*! \brief terminal command output
 *! \param ptPRN terminal command output control block
 *! \retval fsm_rt_on_going terminal clear on going
 *! \retval fsm_rt_cpl terminal clear finish
 */
static fsm_rt_t terminal_prn_str(terminal_prn_str_t *ptPRN)
{
    if ( NULL == ptPRN ) {
        return fsm_rt_err;
    }
    
    switch( ptPRN->tState ) {
        case TERMINAL_PRN_STR_START:
            if( ( NULL == ptPRN->pchStr ) || ( 0 == ptPRN->hwSize ) ) {
				return fsm_rt_err;              // parameter error
            } else {
                ptPRN->tState = TERMINAL_PRN_STR_SEND;
            }
            break;

        case TERMINAL_PRN_STR_SEND:
            if( TGUI_TERMINAL_WRITE_BYTE(*(ptPRN->pchStr)) ) {
                ptPRN->pchStr++;
				ptPRN->hwSize--;
                ptPRN->tState = TERMINAL_PRN_STR_CHECK;
            }
            break;

		case TERMINAL_PRN_STR_CHECK:
			if ( 0 == ptPRN->hwSize ) {
				TERMINAL_PRN_STR_RESET();
				return fsm_rt_cpl;
			}
			ptPRN->tState = TERMINAL_PRN_STR_SEND;
			break;
    }
    return fsm_rt_on_going;
}

/*! \brief initialize terminal command output 
 *! \param ptPRN terminal command output control block
 *! \param 
 *! \param 
 *! \retval false initialize stream terminal command failed
 *! \retval true initialize stream terminal command succeeded
 */
static bool terminal_init_prn_str(  terminal_prn_str_t *ptPRN,
                                    uint8_t *pchStr,
                                    uint_fast16_t hwSize)
{
    if ( (NULL == ptPRN) || (NULL == pchStr) ) {
        return false;
    }
    ptPRN->tState = TERMINAL_PRN_STR_START;
    ptPRN->pchStr = pchStr;
    ptPRN->hwSize = hwSize;
    return true;
}


#endif  /* USE_SERVICE_GUI_TGUI == ENABLED */

/* EOF */
