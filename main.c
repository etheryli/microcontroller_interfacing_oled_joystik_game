////////////////////////////////////////////////////////////////////////////////
// ECE 2534:        Lab 3
//
// File name:       main
// Description:     A game
// Resources:       A lot
// How to use:      Look at document
// Written by:      Hung Nguyen
// Last modified:   30 March 2017


#include <stdio.h>
#include <plib.h>
#include "PmodOLED.h"
#include "OledChar.h"
#include "OledGrph.h"
#include "delay.h"
#include "myDebug.h"
#include "myBoardConfigFall2016.h"

// Bits for setting the port to allow the extraneous wires to work
#define BIT2 (1 << 2)
#define BIT6 (1 << 6)

// Boundary values for comparisons of joystick readings
#define LO 100
#define HI 900

// Button 1 & 2 bit
#define BUTTON1 (1 << 6)
#define BUTTON2 (1 << 7)

// Global definitions of the user-defined shapes characters
BYTE blank[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
char blank_char = 0x00;
BYTE arrow[8] = {0x00, 0x24, 0x66, 0xE7, 0xE7, 0x66, 0x24, 0x00};
char arrow_char = 0x01;
BYTE check[8] = {0x00, 0x01, 0x03, 0x06, 0x8C, 0xD8, 0x70, 0x20};
char check_char = 0x02;
BYTE avatar[8] = {0x24, 0xE7, 0x00, 0x66, 0x00, 0x18, 0x42, 0x3C};
char avatar_char = 0x03;
BYTE prize[8] = {0x00, 0xDB, 0x18, 0xFF, 0xFF, 0x18, 0xDB, 0x00};
char prize_char = 0x04;
BYTE trap[8] = {0x24, 0x3C, 0xE7, 0x5A, 0x5A, 0xE7, 0x3C, 0x24};
char trap_char = 0x05;
BYTE win[8] = {0x92, 0x92, 0xBA, 0xFE, 0xAA, 0xFE, 0x00, 0xFE};
char win_char = 0x06;
BYTE loss[8] = {0x7C, 0x92, 0xBA, 0xFE, 0xEE, 0xFE, 0x54, 0x54};
char loss_char = 0x07;

// Volatile global variable to count number of times in Timer2 ISR
volatile unsigned int timer2_ms_value = 0;

// Volatile global variables for ADC readings
volatile unsigned int ADC_UD_reading;
volatile unsigned int ADC_LR_reading;


//*******************
//**** INTERRUPTS ***
//*******************


// Interrupt handler for the ADC
// IPL7 highest interrupt priority
// UD reads from Mux B, LR reads from Mux A
void __ISR(_ADC_VECTOR, IPL7SRS) _ADCHandler(void)
{
	ADC_UD_reading = ReadADC10(1);
	ADC_LR_reading = ReadADC10(0);
	INTClearFlag(INT_AD1);
}

// The interrupt handler for Timer2
// IPL4 medium interrupt priority
void __ISR(_TIMER_2_VECTOR, IPL4AUTO) _Timer2Handler(void)
{
	timer2_ms_value++;				// Increments milliseconds counter
	INTClearFlag(INT_T2);			// Clear interrupt source flag
}


//********************************
//* ADC CONFIGURATION SETTINGS!! *
//********************************

// ADC Mux Config with MUXA, MUXB, AN2, and AN4 as positive inputs,
// VREFL as negative input
#define AD_MUX_CONFIG ADC_CH0_POS_SAMPLEA_AN2 | ADC_CH0_POS_SAMPLEB_AN4 | \
						ADC_CH0_NEG_SAMPLEA_NVREF
						
// ADC Config1 settings
// Data stored as 16 bit unsigned int
// Internal clock used to start conversion
// ADC auto sampling (sampling begins immediately following conversion)
#define AD_CONFIG1 ADC_FORMAT_INTG | ADC_CLK_AUTO | ADC_AUTO_SAMPLING_ON

// ADC Config2 settings
// Using internal (VDD and VSS) as reference voltages
// Do not scan inputs
// Two samples per interrupt
// Buffer mode is one 16-word buffer
// Alternate sample mode on (both MUXA and MUXB)
#define AD_CONFIG2 ADC_VREF_AVDD_AVSS | ADC_SCAN_OFF | ADC_SAMPLES_PER_INT_2 | \
					ADC_BUF_16 | ADC_ALT_INPUT_ON
					
// ADC Config3 settings
// Autosample time in TAD = 8
// Prescaler for TAD: the 20 here corresponds to a
// ADCS value of 0x27 or 39 decimal => (39 + 1) * 2 * TPB = 8.0us = TAD
// NB: Time for an AD conversion is thus, 8 TAD for acquisition plus
// 		12 TAD for conversion = (8+12) * TAD = 20 * 8.0us = 160us.
#define AD_CONFIG3 ADC_SAMPLE_TIME_8 | ADC_CONV_CLK_20Tcy

// ADC Port Configuration (PCFG)
// Not scanning, so nothing need to be set here
// NB: AN2 and AN4 were selected via the MUX setting in AD_MUX_CONFIG which
// 		set the AD1CHS register (true, but not that obvious..)
#define AD_CONFIGPORT ENABLE_AN4_ANA | ENABLE_AN2_ANA

// ADC Input scan select (CSSL) -- skip scanning as not in scan mode
#define AD_CONFIGSCAN SKIP_SCAN_ALL

// Initialize ADC using defined definitions
void initADC()
{
	// Configure and enable the ADC hardware
	SetChanADC10(AD_MUX_CONFIG);
	OpenADC10(AD_CONFIG1, AD_CONFIG2, AD_CONFIG3, AD_CONFIGPORT, AD_CONFIGSCAN);
	EnableADC10();
	
	// Setup and clear and enable ADC interrupts
	INTSetVectorPriority(INT_ADC_VECTOR, INT_PRIORITY_LEVEL_7);
	INTClearFlag(INT_AD1);
	INTEnable(INT_AD1, INT_ENABLED);
}

// Initialize timer2 and set up the interrupts
void initTimer2() 
{
    // Configure Timer 2 to request a real-time interrupt once per millisecond.
    // The period of Timer 2 is (16 * 625)/(10 MHz) = 1ms.
    OpenTimer2(T2_ON | T2_IDLE_CON | T2_SOURCE_INT | T2_PS_1_16 | T2_GATE_OFF, 624);
    
    // Setup Timer 2 interrupts
    INTSetVectorPriority(INT_TIMER_2_VECTOR, INT_PRIORITY_LEVEL_4);
    INTClearFlag(INT_T2);
    INTEnable(INT_T2, INT_ENABLED);
}

void initINT()
{
    // This is a multi-vector setup
    INTConfigureSystem(INT_SYSTEM_CONFIG_MULT_VECTOR);
    
    // Let the interrupts happen
    INTEnableInterrupts();
}

void initGPIO()
{
    // Initializes GPIO
    TRISGSET = BUTTON1 | BUTTON2;   // Buttons as inputs
    
    // Initialize the unused pins
    TRISBSET = BIT2; // Brown wire pin to input
    TRISBCLR = BIT6; // Yellow wire pin to output
    LATBSET = BIT6; // Yellow wire pin to high
}

// All of the hardware initializations 
int initALL()
{	
    // Initial GPIO ports
    initGPIO();
    
    // Initialize Timer1 and OLED for display
    DelayInit();
    OledInit();

    // Initial Timer2 and ADC
    initTimer2();
    initADC();
    
    // Initial interrupt controller
    initINT();
    
    // Set up our user-defined shape characters for the OLED
    int successful_definition = 0;
    successful_definition = OledDefUserChar(avatar_char, avatar);
    if (!successful_definition)
    {
        return (EXIT_FAILURE);
    }
    successful_definition = OledDefUserChar(blank_char, blank);
    if (!successful_definition)
    {
        return (EXIT_FAILURE);
    }
    successful_definition = OledDefUserChar(arrow_char, arrow);
    if (!successful_definition)
    {
        return (EXIT_FAILURE);
    }
    successful_definition = OledDefUserChar(check_char, check);
    if (!successful_definition)
    {
        return (EXIT_FAILURE);
    }
    successful_definition = OledDefUserChar(blank_char, blank);
    if (!successful_definition)
    {
        return (EXIT_FAILURE);
    }
    successful_definition = OledDefUserChar(blank_char, blank);
    if (!successful_definition)
    {
        return (EXIT_FAILURE);
    }
    successful_definition = OledDefUserChar(blank_char, blank);
    if (!successful_definition)
    {
        return (EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}


//***************************
//**** OLED PIXELS DRAWER ***
//***************************


#ifdef USE_OLED_DRAW_GLYPH
// forward declaration of OledDrawGlyph() to satisfy the compiler
void OledDrawGlyph(char ch);
#endif

void my_oled_draw_glyph(int x_start, int y_start, BYTE *glyph_array)
{
    int i, j;
    
    // Draw horizontally and then downward
    for (i = 0; i < 8; i++)
    {
        for (j = 0; j < 8; j++)
        {
            int x = x_start + 7 - j;
            int y = y_start + i;
            
            OledMoveTo(x, y);
            
            int mask = 1 << j;
            
            if (glyph_array[i] & mask)
            {
                OledSetDrawColor(1);
            }
            else
            {
                OledSetDrawColor(0);
            }
            
            OledDrawPixel();
        }
    }
}

//*******************
//**** MAIN GAME ****
//*******************


// Returns randomized integer within max and min, inclusive of max and min range
int randomize(int max, int min)
{
}

int main()
{
    // Modes for the game with setup phase and then user phase for each screen
    enum {MAIN_SETUP, MAIN_USER, DIFFICULTY_SETUP, DIFFICULTY_USER,
            HOW_TO_SETUP, HOW_TO_USER, SCORE_SETUP, SCORE_USER, PLAY_SETUP,
            PLAY_USER, PLAY_PAUSE, PLAY_RESULT} mode = MAIN_SETUP;
    
    // Modes for joystick push and release
    int mid_flag = 1;
            
    // Stores selection difficulty (Easy = 1, Medium = 2, Hard = 3)
    int difficulty_selection = 1;
    
    // Scoreboard scores
    char score[6];
    int first = 0;
    int second = 0;
    int third = 0;
    
    // For randomly placed prizes and traps, play score, and win/loss flag
    int traps = 0;
    int traps_x[6];
    int traps_y[6];
    int prizes = 0;
    int prizes_x[6];
    int prizes_y[6];
    int tcol;
    int play_score = 0;
    int result = 1;
    
    // For detecting timer2 update
	unsigned int timer2_ms_value_save;
    unsigned int last_oled_update = 0;
    unsigned int ms_since_last_oled_update;
    unsigned int ms_per_update = 100;
	// Push Buttons
	int button1cur = 0;			// Act together to detect an actual push
	int button1prev = 0;        // and set to button#pressed flags
	int button1pressed = 0;
	int button2cur = 0;
	int button2prev = 0;
	int button2pressed = 0;
    
    // Help manipulate pixel locations
    int pix_x = 0;
    int pix_y = 0;
    
    // Initializes everything
    initALL();
    
    // Clear buffer
    OledClearBuffer();
    
    OledUpdate();
    
    // Seed the randomizer with timer2
	srand(timer2_ms_value);
    
    // Infinite while loop for the game
	while (1)
	{   
        // Checks timer
        ms_since_last_oled_update = timer2_ms_value - last_oled_update;
        
        // Updates only every 100ms
        if (ms_since_last_oled_update >= ms_per_update) 
        {
            timer2_ms_value_save = timer2_ms_value;
            last_oled_update = timer2_ms_value;
            
            // Get current push flags
            button1cur = ((PORTG & BUTTON1) == BUTTON1);      
            button2cur = ((PORTG & BUTTON2) == BUTTON2);  

            // Sets pressed flags if button is pushed and released
            // based on previous and current flags
            button1pressed = ((!button1cur) && (button1prev));  
            button2pressed = ((!button2cur) && (button2prev));  	
		
        	// Update previous push flags           
        	button1prev = button1cur;
        	button2prev = button2cur;
            
            // Switch case for different screen and setups
            switch (mode) 
            {
                case MAIN_SETUP:
                {
                    // Setup OLED
                    OledClear();
                    OledSetCursor(3, 0);
                    OledPutString("DIFFICULTY");
                    OledSetCursor(2, 1);
                    OledPutString("HOW  TO PLAY");
                    OledSetCursor(3, 2);
                    OledPutString("SCOREBOARD");
                    OledSetCursor(6, 3);
                    OledPutString("PLAY");
                    OledUpdate();
                    
                    // Resets position of arrow and selection
                    pix_x = 0;
                    pix_y = 0;
                    
                    // Transitions to user interaction view
                    mode = MAIN_USER;
                    break;
                }
                case MAIN_USER:
                {
                    // Draw blank over previous selection arrow
                    my_oled_draw_glyph(0, pix_y, blank);
                    
                    // Moves arrow up/down with push and release plus bound check
                    if (ADC_UD_reading < LO && pix_y + 8 <= 24 && mid_flag)
                    {
                        mid_flag = 0;
                        pix_y += 8;
                    }
                    if (ADC_UD_reading > HI && pix_y - 8 >= 0 && mid_flag)
                    {
                        mid_flag = 0;
                        pix_y -= 8;
                    }
                    
                    // Reset flag once joystick is in middle
                    if (ADC_UD_reading <= (HI - 100) && ADC_UD_reading >= (LO + 100)
                            && !mid_flag)
                    {
                        mid_flag = 1;   
                    }
                    
                    // Draw new selection arrow 
                    my_oled_draw_glyph(0, pix_y, arrow);
                    OledUpdate();
                    
                    // Detect user selection to transition
                    if (ADC_LR_reading > HI | ADC_LR_reading < LO)
                    {
                        // Change mode based on current arrow y-pixel
                        if (pix_y == 0) {
                            mode = DIFFICULTY_SETUP;
                        } else if (pix_y == 8) {
                            mode = HOW_TO_SETUP;
                        } else if (pix_y == 16) {
                            mode = SCORE_SETUP;
                        } else if (pix_y == 24) {
                            mode = PLAY_SETUP;
                        }
                    }
                    break;
                }    
                case DIFFICULTY_SETUP:
                {
                    // Setup OLED
                    OledClear();
                    OledSetCursor(3, 0);
                    OledPutString("DIFFICULTY:");
                    OledSetCursor(6, 1);
                    OledPutString("EASY");
                    OledSetCursor(5, 2);
                    OledPutString("MEDIUM");
                    OledSetCursor(6, 3);
                    OledPutString("HARD");
                    OledUpdate();
                    
                    // Draw check mark for current difficulty selection
                    pix_y = difficulty_selection * 8;
                    
                    my_oled_draw_glyph(112, pix_y, check);
                    OledUpdate();
                    
                    // Transitions to user interaction view
                    mode = DIFFICULTY_USER;
                    break;
                }   
                case DIFFICULTY_USER:
                {
                    // Draw blank over previous selection arrow
                    my_oled_draw_glyph(1, pix_y, blank);
                    
                    // Moves arrow up/down with push and release with bound check
                    if (ADC_UD_reading < LO && pix_y + 8 <= 24 && mid_flag)
                    {
                        mid_flag = 0;
                        pix_y += 8;
                    }
                    if (ADC_UD_reading > HI && pix_y - 8 >= 8 && mid_flag)
                    {
                        mid_flag = 0;
                        pix_y -= 8;
                    }
                    if (ADC_UD_reading <= (HI - 100) && ADC_UD_reading >= (LO + 100)
                            && !mid_flag)
                    {
                        mid_flag = 1;   // Reset flag once joystick is in middle
                    }
                    
                    // Draw selection arrow 
                    my_oled_draw_glyph(1, pix_y, arrow);
                    OledUpdate();
                    
                    // User makes a difficulty selection
                    if (ADC_LR_reading > HI | ADC_LR_reading < LO)
                    {
                        // Clear previous selection check mark at pix_x = 112
                        int curr_difficulty_y = difficulty_selection * 8;
                        my_oled_draw_glyph(112, curr_difficulty_y, blank);
                        
                        // Set new difficulty
                        if (pix_y == 8) {
                            difficulty_selection = 1;
                        } else if (pix_y == 16) {
                            difficulty_selection = 2;
                        } else if (pix_y == 24) {
                            difficulty_selection = 3;
                        }
                        
                        // Draw new check mark
                        my_oled_draw_glyph(112, pix_y, check);
                        OledUpdate();
                    }
                    
                    // Transition back to Main screen if Button 2 is pressed
                    if (button2pressed)
                        mode = MAIN_SETUP;
                    break;
                }
                case HOW_TO_SETUP:
                {
                    // Setup OLED
                    OledClear();
                    OledSetCursor(2, 0);
                    OledPutString("HOW TO PLAY:");
                    OledSetCursor(2, 1);
                    OledPutString("YOU MOVE THIS");
                    OledSetCursor(2, 2);
                    OledPutString("PRIZE: -1 PT");
                    OledSetCursor(2, 3);
                    OledPutString("TRAP:  +1 PT");
                    
                    my_oled_draw_glyph(0, 8, avatar);
                    my_oled_draw_glyph(0, 16, prize);
                    my_oled_draw_glyph(0, 24, trap);
                    OledUpdate();
                    
                    // Transitions to user interaction view
                    mode = HOW_TO_USER;
                    break;
                }
                case HOW_TO_USER:
                {
                    // Transition back to Main screen if Button 2 is pressed
                    if (button2pressed)
                        mode = MAIN_SETUP;
                    break;
                }
                case SCORE_SETUP:
                {
                    // Setup OLED
                    OledClear();
                    OledSetCursor(3, 0);
                    OledPutString("SCOREBOARD:");
                    
                    OledSetCursor(6, 1);
                    sprintf(score, "%s %2d", "1:", first);
                    OledPutString(score);
                    
                    OledSetCursor(6, 2);
                    sprintf(score, "%s %2d", "2:", second);
                    OledPutString(score);
                    
                    OledSetCursor(6, 3);
                    sprintf(score, "%s %2d", "3:", third);
                    OledPutString(score);
                    
                    OledUpdate();
                    // Transitions to user interaction view
                    mode = HOW_TO_USER;
                    break;
                }
                case SCORE_USER:
                {
                    // Transition back to Main screen if Button 2 is pressed
                    if (button2pressed)
                        mode = MAIN_SETUP;
                    break;
                }
                case PLAY_SETUP:
                {
                    // Set ups OLED
                    OledClear();
                    OledSetCursor(15, 0);
                    sprintf(score, "%d", play_score);
                    OledPutString(score);
                    
                    play_score = 0;
                    // TODO
                    // Randomizes traps and prizes placements accordingly
                    // Easy: 1 trap and 1 prize
                    // Medium: 2 traps and 2 prizes
                    // Hard: 3 traps and 3 prizes

                    switch(difficulty_selection)
                    {
                        case 1:
                            traps = 1;
                            prizes = 1;
                            break;
                        case 2:
                            traps = 2;
                            prizes = 2;
                            break;
                        case 3:
                            traps = 4;
                            prizes = 4;
                            break;
                    }
                    
                    // seed the random generator with timer value
                    int i = 0;
					// Pick the x and y randomly
                    for (i = 0; i < traps; i++)
                    {
						// 41 for easy, so 0 to 40, then 8 to 48 for 2 columns
                        // 4 columns for medium and 8 columns for hard
						tcol = rand() % 2; //randomly chooses column
                        traps_x[i] = (rand() % ((104 / (traps+prizes)) + 1)) + 8;
                        traps_y[i] = rand() % 25;
                        prizes_x[i] = (rand() % ((104 / (traps+prizes)) - 1)) + 8;
                        prizes_y[i] = rand() % 25;
						// correcting for random column chosen
						if (tcol)
							traps_x[i] += 104 / (traps+prizes);
						else
							prizes_x[i] += 104 / (traps+prizes);
						
						// Correcting for the greater columns
						traps_x[i] = traps_x[i] + i*(104/traps);
						prizes_x[i] = prizes_x[i] + i*(104/traps);
						
                        // Draw
						my_oled_draw_glyph(traps_x[i], traps_y[i], trap);
						my_oled_draw_glyph(prizes_x[i], prizes_y[i], prize);
                    }

                    OledUpdate();
                    
                    // Initial position of avatar
                    pix_x = 0;
                    pix_y = 0;
                    
                    // Transition to user interaction play
                    mode = PLAY_USER;
                    break;
                }
                case PLAY_USER:
                {
                    // Wipe previous avatar
                    my_oled_draw_glyph(pix_x, pix_y, blank);
                    
                    // Movement and bound check for board
                    if (ADC_UD_reading < LO + 100 && pix_y + 1 <= 24)
                        pix_y++;
                    if (ADC_UD_reading > HI - 100 && pix_y - 1 >= 0)
                    {
                        // Special bound check for scoreboard collision
                        if (pix_x >= 113 && (pix_y - 1) == 7)
                            pix_y = pix_y;
                        else
                            pix_y--;
                    }
                    if (ADC_LR_reading < LO + 100 && pix_x - 1 >= 0)
                        pix_x--;
                    if (ADC_LR_reading > HI - 100 && pix_x + 1 <= 120)
                    {
                        // Special bound check for scoreboard collision
                        if (pix_x + 1 > 112 && pix_y < 8)
                            pix_x = pix_x;
                        else
                            pix_x++;
                    }

                    // Center checking for loop as well as redraw of the glyphs
                    int k = 0;
                    for (k = 0; k < traps; k++)
                    {
                        // if the current center is ontop
                        if (pix_x == traps_x[k] && pix_y == traps_y[k])
                        {
                            //
                            traps_x[k] = 129;
                            traps_y[k] = 33;
                            play_score--;
                        }

                        if (!(traps_x[k] == 129 && traps_y[k] == 33))
                        {
                            my_oled_draw_glyph(traps_x[k], traps_y[k], trap);
                        }
                        
                        // 
                        if (pix_x == prizes_x[k] && pix_y == prizes_y[k])
                        {
                            // Make the trap
                            prizes_x[k] = 129;
                            prizes_y[k] = 33;
                            play_score++;
                            prizes--;
                        }
                        if (!(prizes_x[k] == 129 && prizes_y[k] == 33))
                        {
                            my_oled_draw_glyph(prizes_x[k], prizes_y[k], prize);
                        }
                    }
                    
                    // Redraw
                    my_oled_draw_glyph(pix_x, pix_y, avatar);
                    
                    // Update score + Redraw un-centered-touch traps/prizes
                    OledSetCursor(15, 0);
                    sprintf(score, "%d", play_score);
                    OledPutString(score);
                    OledUpdate();
                    
                    // Transition to end game
                    if (play_score < 0 )
                    {   
                        play_score = 0;
                        result = 0; // LOSS
                        mode = PLAY_RESULT;
                    }
                    if (prizes == 0)
                    {
                        result = 1; // WIN
                        mode = PLAY_RESULT;
                    }
                    // Transitions to pause, 
                    if (button1pressed)     // Pause
                        mode = PLAY_PAUSE;
                    if (button2pressed)     // Go to Main screen
                        mode = MAIN_SETUP;
                    
                    break;
                }
                case PLAY_PAUSE:
                {
                    // Transitions to interfaces
                    if (button1pressed)     // Un-pause
                        mode = PLAY_USER;
                    if (button2pressed)     // Go to Main screen
                        mode = MAIN_SETUP;
                    break;
                }
                case PLAY_RESULT:
                {
                    // OLED Setup for result
                    OledClear();
                    OledSetCursor(6, 2);
                   
                    // Checks result status to display the right text
                    if (result) {
                        OledPutString("Win");
                        my_oled_draw_glyph(56, 6, win);
                    } else {
                        OledPutString("Loss");
                        my_oled_draw_glyph(60, 6, loss);
                    }
                    OledUpdate();
                    
                    // Wait of 3 seconds that can be skipped with Button 2
                    ms_per_update = 3000;
                    
                    // Final transition to Main Menu
                    if (ms_since_last_oled_update >= ms_per_update)
                    {
                        int temp;
                        
                        // updates scoreboard
                        if (play_score > third){
                            third = play_score;
                            // Rearranges scoreboard
                            if (third > second)
                            {
                                temp = second;
                                second = third;
                                third = temp;
                                if (second > first)
                                {
                                    temp = first;
                                    first = second;
                                    second = temp;
                                }
                            }
                        }
                        
                        // Transition to main screen and reset ms_per_update
                        mode = MAIN_SETUP;
                        ms_per_update = 100;
                    }
                    break;
                }
            }
        }  
	}
	return EXIT_SUCCESS;
}
	
