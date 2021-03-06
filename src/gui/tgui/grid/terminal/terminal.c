// terminal.c

/*============================ INCLUDES ======================================*/
#include ".\app_cfg.h"

#if USE_SERVICE_GUI_TGUI == ENABLED
#include "..\interface.h"

/*============================ MACROS ========================================*/
#define TGUI_TERMINAL_CLEAR_CODE	    (0x0C)
#define ASCII_ESC                       (0x1B)

#define WIDTH                           (80)
#define HEIGHT                          (23)

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
//! \name terminal status
//! @{
typedef enum {
    TER_READY_IDLE      = 0,            //!< terminal is idle    
    TER_READY_BUSY      = 1,            //!< terminal is busy
} em_ter_status_t;
//! @}

/*============================ PROTOTYPES ====================================*/
/*! \brief set current cursor position
 *! \param tGrid cursor position
 *! \retval fsm_rt_on_going set grid on going
 *! \retval fsm_rt_cpl set grid finish
 */
static fsm_rt_t terminal_set_grid(grid_t tGrid);

/*! \brief get current cursor position
 *! \param tGrid cursor position
 *! \retval fsm_rt_on_going set grid finish
 *! \retval fsm_rt_cpl set grid on going
 */
static fsm_rt_t terminal_get_grid(grid_t *ptGrid);

/*! \brief save current cursor position
 *! \param none
 *! \retval fsm_rt_on_going save grid on going
 *! \retval fsm_rt_cpl save grid on finish
 */
static fsm_rt_t terminal_save_current(void);

/*! \brief resume current cursor position
 *! \param none
 *! \retval fsm_rt_on_going set grid on going
 *! \retval fsm_rt_cpl resume grid complete
 */
static fsm_rt_t terminal_resume(void);

/*! \brief set display attribute
 *! \param tBrush display attribute
 *! \retval fsm_rt_on_going set brush on going
 *! \retval fsm_rt_cpl set brush finish
 */
static fsm_rt_t terminal_set_brush(grid_brush_t tBrush);

/*! \brief get display attribute
 *! \param none
 *! \return display attribute
 */
static grid_brush_t terminal_get_brush(void);

/*! \brief terminal clear
 *! \param none
 *! \retval fsm_rt_on_going terminal clear on going
 *! \retval fsm_rt_cpl terminal clear finish
 */
static fsm_rt_t terminal_clear(void);

/*! \brief terminal print
 *! \param none
 *! \retval fsm_rt_on_going terminal print on going
 *! \retval fsm_rt_cpl terminal print finish
 */
static fsm_rt_t terminal_print(uint8_t *pchString, uint_fast16_t hwSize);

/*============================ GLOBAL VARIABLES ==============================*/
//! \brief terminal object
const i_gdc_t terminal = {
    .Info = {
        .chWidth = WIDTH,
        .chHeight = HEIGHT,
    },
    .Position = {
        .Set = terminal_set_grid,
        .Get = terminal_get_grid,
        .SaveCurrent = terminal_save_current,
        .Resume = terminal_resume,
    },
    .Color = {
        .Set = terminal_set_brush,
        .Get = terminal_get_brush,
    },
    .Clear = terminal_clear,
    .Print = terminal_print,
};

/*============================ LOCAL VARIABLES ===============================*/
//! grid brush
static grid_brush_t s_tCurrentGridBrush;

//! terminal exchange buffer
static uint8_t s_chSend[8] = {
    ASCII_ESC, '[',
};

//! terminal lock status
static em_ter_status_t s_tCurrentStatus = TER_READY_IDLE;

/*============================ IMPLEMENTATION ================================*/

#define TER_STREAM_RESET_FSM()                  \
    do {                                        \
        s_tState = TER_STREAM_START;            \
    } while(false)

/*! \brief terminal stream send with external interface TGUI_TERMINAL_WRITE_BYTE()
 *!
 *! \param pchStream output stream buffer
 *! \param wSize stream length
 *!
 *! \retval fsm_rt_on_going FSM should keep running
 *! \retval fsm_rt_cpl FSM complete.
 */
static fsm_rt_t fsm_ter_stream_exchange(uint8_t *pchStream, uint8_t chSize)
{
    NO_INIT static uint8_t *s_pchStream;
    NO_INIT static uint8_t s_chSize;

    static enum {
        TER_STREAM_START                = 0,
        TER_STREAM_SEND
    } s_tState = TER_STREAM_START;

    switch (s_tState) {
        case TER_STREAM_START:              //!< FSM start
            //! check parameter
            if ((NULL == pchStream) || (0 == chSize)) {
                return fsm_rt_cpl;          //!< doing nothing at all
            } else {
                //! read & write
                s_pchStream = pchStream;
                s_chSize = chSize;            //!< initialize size
                s_tState = TER_STREAM_SEND;
            }
            break;

        case TER_STREAM_SEND: {             //!< FSM start
            if (TGUI_TERMINAL_WRITE_BYTE(*s_pchStream)) {
                //! success
                s_pchStream++;
                if (0 == --s_chSize) {
                    TER_STREAM_RESET_FSM();
                    return fsm_rt_cpl;
                }
            }
            break;
        }
    }

    return fsm_rt_on_going;                 //!< state machine keep running
}

#define TERMINAL_SET_GRID_RESET()           \
    do {                                    \
        s_tState = TERMINAL_SET_GRID_START; \
    } while(0)

#if 0

/*! \brief set current cursor position
 *! \param tGrid cursor position
 *! \retval fsm_rt_on_going set grid on going
 *! \retval fsm_rt_cpl set grid finish
 */
static fsm_rt_t terminal_set_grid(grid_t tGrid)
{
    static enum {
        TERMINAL_SET_GRID_START = 0,
        TERMINAL_SET_GRID_SEND
    } s_tState = TERMINAL_SET_GRID_START;
    
    switch ( s_tState ) {
        case TERMINAL_SET_GRID_START:

            SAFE_ATOM_CODE(
                //! whether system is initialized
                if (TER_READY_BUSY == s_tCurrentStatus) {
                    EXIT_SAFE_ATOM_CODE();
                    return fsm_rt_on_going;
                }
                //! set current state
                s_tCurrentStatus = TER_READY_BUSY;
            )

            do {
                uint8_t chRow = HEIGHT - tGrid.chTop;
                uint8_t chColumn = tGrid.chLeft;

                s_chSend[2] = ( chRow / 10 ) + '0';
                s_chSend[3] = ( chRow % 10 ) + '0';
                s_chSend[4] = ';';
                s_chSend[5] = ( chColumn / 10 ) + '0';
                s_chSend[6] = ( chColumn % 10 ) + '1';
                s_chSend[7] = 'H';
            } while (false);

            s_tState = TERMINAL_SET_GRID_SEND;
            // break;

        case TERMINAL_SET_GRID_SEND:
            if (fsm_rt_cpl == fsm_ter_stream_exchange(s_chSend, 8)) {
                SAFE_ATOM_CODE(
                    //! set idle state
                    s_tCurrentStatus = TER_READY_IDLE;
                )
                TERMINAL_SET_GRID_RESET();
                return fsm_rt_cpl;
            }
            break;
    }
    
    return fsm_rt_on_going;
}

#else

/*! \brief set current cursor position
 *! \param tGrid cursor position
 *! \retval fsm_rt_on_going set grid on going
 *! \retval fsm_rt_cpl set grid finish
 */
static fsm_rt_t terminal_set_grid(grid_t tGrid)
{
    static enum {
        TERMINAL_SET_GRID_START = 0,
        TERMINAL_SET_GRID_SEND
    } s_tState = TERMINAL_SET_GRID_START;
    static uint8_t s_chIndex = 2;

    switch ( s_tState ) {
        case TERMINAL_SET_GRID_START:

            SAFE_ATOM_CODE(
                //! whether system is initialized
                if (TER_READY_BUSY == s_tCurrentStatus) {
                    EXIT_SAFE_ATOM_CODE();
                    return fsm_rt_on_going;
                }
                //! set current state
                s_tCurrentStatus = TER_READY_BUSY;
            )

            do {
                uint8_t chRow = HEIGHT - tGrid.chTop;
                uint8_t chColumn = tGrid.chLeft;
                s_chIndex = 2;

                if (0 != chRow / 10) {
                    s_chSend[s_chIndex++] = chRow / 10 + '0';
                }
                s_chSend[s_chIndex++] = chRow % 10 + '0';
                s_chSend[s_chIndex++] = ';';
                if (0 != chColumn / 10) {
                    s_chSend[s_chIndex++] = chColumn / 10 + '0';
                }
                s_chSend[s_chIndex++] = ( chColumn % 10 ) + '1';
                s_chSend[s_chIndex++] = 'H';
            } while (false);

            s_tState = TERMINAL_SET_GRID_SEND;
            // break;

        case TERMINAL_SET_GRID_SEND:
            if (fsm_rt_cpl == fsm_ter_stream_exchange(s_chSend, s_chIndex)) {
                SAFE_ATOM_CODE(
                    //! set idle state
                    s_tCurrentStatus = TER_READY_IDLE;
                )
                TERMINAL_SET_GRID_RESET();
                return fsm_rt_cpl;
            }
            break;
    }

    return fsm_rt_on_going;
}

#endif

#define TERMINAL_GET_GRID_RESET()           \
    do {                                    \
        s_tState = TERMINAL_GET_GRID_START; \
    } while(0)

/*! \brief get current cursor position
 *! \param tGrid cursor position
 *! \retval fsm_rt_on_going set grid finish
 *! \retval fsm_rt_cpl set grid on going
 */
static fsm_rt_t terminal_get_grid(grid_t *ptGrid)
{
    static enum {
        TERMINAL_GET_GRID_START = 0,
        TERMINAL_GET_GRID_SEND,
        TERMINAL_GET_GRID_RECEIVE,
        TERMINAL_GET_GRID_CHECK
    } s_tState = TERMINAL_GET_GRID_START;
    
    NO_INIT static uint8_t s_chReceiveCode[8];
    NO_INIT static uint8_t s_chReceiveCnt;

	if ( NULL == ptGrid ) {
		return fsm_rt_err;
	}
    
    switch ( s_tState ) {
        case TERMINAL_GET_GRID_START:
            SAFE_ATOM_CODE(
                //! whether system is initialized
                if (TER_READY_BUSY == s_tCurrentStatus) {
                    EXIT_SAFE_ATOM_CODE();
                    return fsm_rt_on_going;
                }
                //! set current state
                s_tCurrentStatus = TER_READY_BUSY;
            )

            s_chSend[2] = '6';
            s_chSend[3] = 'n';
            s_tState = TERMINAL_GET_GRID_SEND;
            // break;

        case TERMINAL_GET_GRID_SEND:
            if (fsm_rt_cpl == fsm_ter_stream_exchange(s_chSend, 4)) {
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
                        SAFE_ATOM_CODE(
                            //! set idle state
                            s_tCurrentStatus = TER_READY_IDLE;
                        )
                        return fsm_rt_err;
                    }
                    if ( 'R' == chTemp ) {
                        s_tState = TERMINAL_GET_GRID_CHECK;
                    }
                }
            }
            break;

        case TERMINAL_GET_GRID_CHECK: {
                int_fast8_t chRow;
                int_fast8_t chColumn;

                if ( ';' == s_chReceiveCode[3] ) {
                    chRow = HEIGHT - (s_chReceiveCode[2] - '0');
                    if ( 'R' == s_chReceiveCode[5] ) {
                        chColumn = s_chReceiveCode[4] - '1';
                    } else if ( 'R' == s_chReceiveCode[6] ) {
                        chColumn = ( s_chReceiveCode[4] - '0' ) * 10;
                        chColumn += ( s_chReceiveCode[5] - '1' );
                    } else {
                        SAFE_ATOM_CODE(
                            //! set idle state
                            s_tCurrentStatus = TER_READY_IDLE;
                        )
                        return fsm_rt_err;
                    }
                } else if ( ';' == s_chReceiveCode[4] ) {
                    chRow = ( s_chReceiveCode[2] - '0' ) * 10;
                    chRow += ( s_chReceiveCode[3] - '0' );
                    chRow = HEIGHT - chRow;
                    if ( 'R' == s_chReceiveCode[6] ) {
                        chColumn = s_chReceiveCode[5] - '1';
                    } else if ( 'R' == s_chReceiveCode[7] ) {
                        chColumn = ( s_chReceiveCode[5] - '0' ) * 10;
                        chColumn += ( s_chReceiveCode[6] - '1' );
                    } else {
                        SAFE_ATOM_CODE(
                            //! set idle state
                            s_tCurrentStatus = TER_READY_IDLE;
                        )
                        return fsm_rt_err;
                    }
                } else {
                    return fsm_rt_err;
                }
                ptGrid->chTop = chRow;
                ptGrid->chLeft = chColumn;

                SAFE_ATOM_CODE(
                    //! set idle state
                    s_tCurrentStatus = TER_READY_IDLE;
                )
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
static fsm_rt_t terminal_save_current(void)
{
    static enum {
        TERMINAL_SAVE_CURRENT_START = 0,
        TERMINAL_SAVE_CURRENT_SEND
    } s_tState = TERMINAL_SAVE_CURRENT_START;
    
    switch ( s_tState ) {
        case TERMINAL_SAVE_CURRENT_START:
            SAFE_ATOM_CODE(
                //! whether system is initialized
                if (TER_READY_BUSY == s_tCurrentStatus) {
                    EXIT_SAFE_ATOM_CODE();
                    return fsm_rt_on_going;
                }
                //! set current state
                s_tCurrentStatus = TER_READY_BUSY;
            )
            s_chSend[2] = 's';
            s_tState = TERMINAL_SAVE_CURRENT_SEND;
            // break;

        case TERMINAL_SAVE_CURRENT_SEND:
            if (fsm_rt_cpl == fsm_ter_stream_exchange(s_chSend, 3)) {
                SAFE_ATOM_CODE(
                    //! set idle state
                    s_tCurrentStatus = TER_READY_IDLE;
                )
                TERMINAL_SAVE_CURRENT_RESET();
                return fsm_rt_cpl;
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
static fsm_rt_t terminal_resume(void)
{
    static enum {
        TERMINAL_RESUME_START = 0,
        TERMINAL_RESUME_SEND
    } s_tState = TERMINAL_RESUME_START;

    switch ( s_tState ) {
        case TERMINAL_RESUME_START:
            SAFE_ATOM_CODE(
                //! whether system is initialized
                if (TER_READY_BUSY == s_tCurrentStatus) {
                    EXIT_SAFE_ATOM_CODE();
                    return fsm_rt_on_going;
                }
                //! set current state
                s_tCurrentStatus = TER_READY_BUSY;
            )
            s_chSend[2] = 'u';
            s_tState = TERMINAL_RESUME_SEND;
            break;

        case TERMINAL_RESUME_SEND:
            if (fsm_rt_cpl == fsm_ter_stream_exchange(s_chSend, 3)) {
                SAFE_ATOM_CODE(
                    //! set idle state
                    s_tCurrentStatus = TER_READY_IDLE;
                )
                TERMINAL_RESUME_RESET();
                return fsm_rt_cpl;
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
static fsm_rt_t terminal_set_brush(grid_brush_t tBrush)
{
	static enum {
		TERMINAL_SET_BRUSH_START = 0,
		TERMINAL_SET_BRUSH_SEND
	} s_tState = TERMINAL_SET_BRUSH_START;

	switch ( s_tState ) {
		case TERMINAL_SET_BRUSH_START:
			if ( ( tBrush.tForeground.tValue > 7 ) || ( tBrush.tBackground.tValue > 7 ) ) {
				return fsm_rt_err;
			}
            SAFE_ATOM_CODE(
                //! whether system is initialized
                if (TER_READY_BUSY == s_tCurrentStatus) {
                    EXIT_SAFE_ATOM_CODE();
                    return fsm_rt_on_going;
                }
                //! set current state
                s_tCurrentStatus = TER_READY_BUSY;

                s_tCurrentGridBrush = tBrush;
            )
			s_chSend[2] = '3';
			s_chSend[3] = tBrush.tForeground.tValue + '0';
			s_chSend[4] = ';';
			s_chSend[5] = '4';
			s_chSend[6] = tBrush.tBackground.tValue + '0';
			s_chSend[7] = 'm';
            s_tState = TERMINAL_SET_BRUSH_SEND;
			//break;

		case TERMINAL_SET_BRUSH_SEND:
			if (fsm_rt_cpl == fsm_ter_stream_exchange(s_chSend, 8)) {
                SAFE_ATOM_CODE(
                    //! set idle state
                    s_tCurrentStatus = TER_READY_IDLE;
                )
				TERMINAL_SET_BRUSH_RESET();
				return fsm_rt_cpl;
			}
			break;
	}

	return fsm_rt_on_going;
}

/*! \brief get display attribute
 *! \param none
 *! \return display attribute
 */
static grid_brush_t terminal_get_brush(void)
{
    grid_brush_t tBrush;

    SAFE_ATOM_CODE(
        tBrush = s_tCurrentGridBrush;
    )

	return tBrush;
}

/*! \brief terminal clear
 *! \param none
 *! \retval fsm_rt_on_going terminal clear on going
 *! \retval fsm_rt_cpl terminal clear finish
 */
static fsm_rt_t terminal_clear(void)
{
	static enum {
		TERMINAL_START = 0,
		TERMINAL_CLEAR
	} s_tState = TERMINAL_START;

    switch (s_tState) {
		case TERMINAL_START:
            SAFE_ATOM_CODE(
                //! whether system is initialized
                if (TER_READY_BUSY == s_tCurrentStatus) {
                    EXIT_SAFE_ATOM_CODE();
                    return fsm_rt_on_going;
                }
                //! set current state
                s_tCurrentStatus = TER_READY_BUSY;
            )
            s_tState = TERMINAL_CLEAR;
            //break;

        case TERMINAL_CLEAR:
            if ( TGUI_TERMINAL_WRITE_BYTE(TGUI_TERMINAL_CLEAR_CODE) ) {
                SAFE_ATOM_CODE(
                    //! set idle state
                    s_tCurrentStatus = TER_READY_IDLE;
                )
                return fsm_rt_cpl;
            }
            break;
    }

	return fsm_rt_on_going;
}

/*! \brief terminal print
 *! \param none
 *! \retval fsm_rt_on_going terminal print on going
 *! \retval fsm_rt_cpl terminal print finish
 */
static fsm_rt_t terminal_print(uint8_t *pchString, uint_fast16_t hwSize)
{
	static enum {
		TERMINAL_START = 0,
		TERMINAL_PRINT,
	} s_tState = TERMINAL_START;

    switch (s_tState) {
		case TERMINAL_START:
            SAFE_ATOM_CODE(
                //! whether system is initialized
                if (TER_READY_BUSY == s_tCurrentStatus) {
                    EXIT_SAFE_ATOM_CODE();
                    return fsm_rt_on_going;
                }
                //! set current state
                s_tCurrentStatus = TER_READY_BUSY;
            )
            s_tState = TERMINAL_PRINT;
            //break;

        case TERMINAL_PRINT:
            if (fsm_rt_cpl == fsm_ter_stream_exchange(pchString, hwSize)) {
                SAFE_ATOM_CODE(
                    //! set idle state
                    s_tCurrentStatus = TER_READY_IDLE;
                )
                return fsm_rt_cpl;
            }
            break;
    }

	return fsm_rt_on_going;

}

#endif  /* USE_SERVICE_GUI_TGUI == ENABLED */

/* EOF */
