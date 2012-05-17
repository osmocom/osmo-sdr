
/**************************************************************
* 
* Revision History of opcode.h
* 
* 
* 09/11/07 NN Updated to support version 1.3
* This version supported new POLING STATUS LOOP opcodes  
* for Flash programming of the Lattice FPGA devices
* #define LOOP    = 0x58
* #define ENDLOOP = 0x59
***************************************************************/


/*************************************************************
*                                                            *
* LATTICE CABLE DEFINTIONS.                                  *
*                                                            *
* Define these only if the lattice cable is being used.      *
*                                                            *
*************************************************************/

#define    pinTDI       1
#define    pinTCK       2
#define    pinTMS       4
#define    pinENABLE    8
#define    pinTRST      16
#define    pinCE        32
#define    pinTDO       64

/*************************************************************
*                                                            *
* ERROR DEFINITIONS                                          *
*                                                            *
*************************************************************/

#define ERR_VERIFY_FAIL				-1
#define ERR_FIND_ALGO_FILE			-2
#define ERR_FIND_DATA_FILE			-3
#define ERR_WRONG_VERSION			-4
#define ERR_ALGO_FILE_ERROR			-5
#define ERR_DATA_FILE_ERROR			-6
#define ERR_OUT_OF_MEMORY			-7

/*************************************************************
*                                                            *
* DATA TYPE REGISTER BIT DEFINITIONS                         *
*                                                            *
*************************************************************/

#define SIR_DATA		0x0001	/*** Current command is SIR ***/
#define SDR_DATA		0x0002	/*** Current command is SDR ***/
#define TDI_DATA		0x0004	/*** Command contains TDI ***/
#define TDO_DATA		0x0008	/*** Command contains TDO ***/
#define MASK_DATA		0x0010	/*** Command contains MASK ***/
#define DTDI_DATA		0x0020	/*** Verification flow ***/
#define DTDO_DATA		0x0040	/*** Verification flow ***/
#define COMPRESS		0x0080	/*** Compressed data file ***/
#define COMPRESS_FRAME	0x0100	/*** Compressed data frame ***/

/*************************************************************
*                                                            *
* USED JTAG STATE                                            *
*                                                            *
*************************************************************/

#define RESET      0x00
#define IDLE       0x01
#define IRPAUSE    0x02
#define DRPAUSE    0x03
#define SHIFTIR    0x04
#define SHIFTDR    0x05
#define DRCAPTURE  0x06

/*************************************************************
*                                                            *
* VME OPCODE DEFINITIONS                                     *
*                                                            *
* These are the opcodes found in the VME file.  Although     *
* most of them are similar to SVF commands, a few opcodes    *
* are available only in VME format.                          *
*                                                            *
*************************************************************/

#define STATE			0x10
#define SIR				0x11
#define SDR				0x12
#define TCK				0x1B
#define WAIT			0x1A
#define ENDDR			0x02
#define ENDIR			0x03
#define HIR				0x06
#define TIR				0x07
#define HDR				0x08
#define TDR		        0x09
#define TDI				0x13
#define CONTINUE		0x70
#define TDO				0x14
#define MASK			0x15
#define LOOP			0x58
#define ENDLOOP			0x59
#define LCOUNT			0x66    
#define LDELAY			0x67		
#define LSDR			0x68		
#define ENDSTATE		0x69
#define ENDVME			0x7F

/*************************************************************
*                                                            *
* Begin future opcodes at 0xA0 to avoid conflict with Full   *
* VME opcodes.                                               *
*                                                            *
*************************************************************/

#define BEGIN_REPEAT	0xA0
#define END_REPEAT		0xA1
#define END_FRAME		0xA2
#define DATA			0xA3
#define PROGRAM			0xA4
#define VERIFY			0xA5
#define DTDI			0xA6
#define DTDO			0xA7

/*************************************************************
*                                                            *
* Opcode for discrete pins toggling							 *
*                                                            *
*************************************************************/
#define signalENABLE  0x1C    /*assert the ispEN pin*/
#define signalTMS     0x1D    /*assert the MODE or TMS pin*/
#define signalTCK     0x1E    /*assert the SCLK or TCK pin*/
#define signalTDI     0x1F    /*assert the SDI or TDI pin*/
#define signalTRST    0x20    /*assert the RESET or TRST pin*/   
#define signalTDO     0x21    /*assert the RESET or TDO pin*/   
#define signalCableEN    0x22    /*assert the RESET or CableEN pin*/  