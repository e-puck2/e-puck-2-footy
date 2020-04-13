#include "sensors.h"

//  Standard Library
#include <math.h>

// ChibiOS & others
#include "ch.h"							// main include files
#include "hal.h"						// Hardware Abstraction Layer subsystem header
#include <usbcfg.h>						// USB services
#include <chprintf.h>					// mini printf-like functionality

// EPFL-MICRO 315 library
#include <camera/dcmi_camera.h>			// e-puck-2 Digital CaMera Interface
#include <camera/po8030.h>				// e-puck-2 main frontal camera
#include <sensors/VL53L0X/VL53L0X.h>	// e-puck-2 Time Of Flight sensor

// this project files
#include "debug.h"
#include "move.h"
#include "central.h"

// local defines
#define IMAGE_BUFFER_SIZE	PO8030_MAX_WIDTH			// size of the image acquired, in RGB565 pixel =640
#define IMAGE_LINE_HEIGHT	PO8030_MAX_HEIGHT/2			// captured line height = 240

#define NO_RISE_FALL_FOUND_POS          IMAGE_BUFFER_SIZE + 1
#define GREEN_PIXEL_RISE_FALL_THRESHOLD (int16_t)(0.25 * 63)
#define THRESHOLD_BALL_COLOR_IN_GREEN   13
#define TAN_45_OVER_2_CONST             0.4142135679721832275390625f // in rad, fit for float
#define DERIVATION_PERIOD_DELTA			2

#define	MOVE_SECURITY_SPACE				50


// semaphores
static BSEMAPHORE_DECL(sensors_semaphore_image_ready_for_process, TRUE);
static BSEMAPHORE_DECL(sensors_semaphore_image_completed, TRUE);

// global variables to this module
static bool s_sensors_ball_found = false;///@Pierre: C'est tout b�te mais le s_ est une bonne convention je toruve pour diff�rencier les variables statiques
static bool s_sensors_ball_left_found = false;
static bool s_sensors_ball_right_found = false;
static float s_sensors_ball_angle;
static float s_sensors_ball_left_angle;
static float s_sensors_ball_right_angle;

// local function prototypes
float compute_angle_ball(uint16_t ball_middle_pos);
void detection_in_image(uint8_t * green_pixels);

// threaded functions
static THD_WORKING_AREA(wa_acquire_image, 256);
static THD_FUNCTION(acquire_image, arg)
{
	// this function is used in a thread
    chRegSetThreadName(__FUNCTION__);
    (void)arg;

	// acquire a line from the camera, from pixel 0 in line IMAGE_LINE_HEIGHT of size IMAGE_BUFFER_SIZE x 2 // @PI a revoir
    // caution: two lines asked for internal reasons

	if(po8030_advanced_config(FORMAT_RGB565, 0, IMAGE_LINE_HEIGHT, IMAGE_BUFFER_SIZE, 2, SUBSAMPLING_X1, SUBSAMPLING_X1) != MSG_OK)
		chprintf((BaseSequentialStream *)&SD3, "Error po8030_advanced_config\n");
	//if(po8030_set_awb(0) != MSG_OK)
		//chprintf((BaseSequentialStream *)&SD3, "Error po8030_set_awb\n");

	dcmi_enable_double_buffering();
	dcmi_set_capture_mode(CAPTURE_ONE_SHOT);
	dcmi_prepare();

    while(1)
    {
    	// await the end of an image capture to be signaled
    	chBSemWait(central_get_semaphore_authorization_acquire());
    	// start capturing the image
		dcmi_capture_start();
		// wait until completed
		wait_image_ready();
		// signal completeness with the semaphore
		chBSemSignal(&sensors_semaphore_image_ready_for_process);
    }
}

static THD_WORKING_AREA(wa_process_image, 1024);
static THD_FUNCTION(process_image, arg)
{
	// this function is used in a thread
    chRegSetThreadName(__FUNCTION__);
    (void)arg;

	uint8_t * img_raw_RGB565_pixels = NULL;
	uint8_t   green_pixels[IMAGE_BUFFER_SIZE] = {0};

	//static bool s_send_computer = true;
    while(1)
    {
    	// await the end of an image capture to be signaled
        chBSemWait(&sensors_semaphore_image_ready_for_process);

		// get the pointer to the array filled with the last image in RGB565
        img_raw_RGB565_pixels = dcmi_get_last_image_ptr();

        for(uint16_t i = 0 ; i < 2 * IMAGE_BUFFER_SIZE ; i += 2)
        	green_pixels[i/2] = (((img_raw_RGB565_pixels[i] & 7) << 3) | (img_raw_RGB565_pixels[i] >> 5)) & 63;

        detection_in_image(green_pixels);


		//debug_send_uint8_array_to_computer(red_pixels, IMAGE_BUFFER_SIZE);

		//debug_send_for_printlinke_couple_uint8(img_raw_RGB565_pixels, 2 * IMAGE_BUFFER_SIZE);

        chBSemSignal(&sensors_semaphore_image_completed);
    }
}

float compute_angle_ball(uint16_t ball_middle_pos)
{
    return atan((((float)ball_middle_pos / 320) - 1) * TAN_45_OVER_2_CONST) * 180.f / M_PI;///@Pierre: c'est bien pour quand on programme mais ultimement on fera mieux de pr�calculer ces valeurs
}

void detection_in_image(uint8_t * green_pixels)
{
    uint16_t last_fall_pos = NO_RISE_FALL_FOUND_POS;
    uint16_t sum;
    int16_t pixel_derivative = (int16_t)green_pixels[DERIVATION_PERIOD_DELTA*2];//compute initial slope with white wall hypothesis -> detect fall if ball cut

    for(uint16_t i = DERIVATION_PERIOD_DELTA ; i < IMAGE_BUFFER_SIZE - DERIVATION_PERIOD_DELTA ; ++i)
    {
        if(last_fall_pos != NO_RISE_FALL_FOUND_POS)   // if the beginning of a ball has been seen, we can look at the end of a ball
        {
            sum += green_pixels[i];
            if(pixel_derivative >= GREEN_PIXEL_RISE_FALL_THRESHOLD || i==IMAGE_BUFFER_SIZE - DERIVATION_PERIOD_DELTA)//detect rise or cut ball
            {
                if(sum < THRESHOLD_BALL_COLOR_IN_GREEN * (i - last_fall_pos))
                {
                    if(last_fall_pos != DERIVATION_PERIOD_DELTA)
                    {
                    	s_sensors_ball_left_found = true;
        				s_sensors_ball_left_angle = getAngle()+compute_angle_ball(last_fall_pos);//ball is not cut -> left edge found
                    }
                    if(i !=IMAGE_BUFFER_SIZE - DERIVATION_PERIOD_DELTA)
                    {
                    	s_sensors_ball_right_found = true;
        				s_sensors_ball_right_angle = getAngle()+compute_angle_ball(i);//ball is not cut -> right edge found
                    }

                    chprintf((BaseSequentialStream *)&SD3, "detected edges are: %d and %d\n", last_fall_pos, i);
                    chprintf((BaseSequentialStream *)&SD3, "angles are %f and %f\n", s_sensors_ball_left_angle, s_sensors_ball_right_angle);

                    if(s_sensors_ball_left_found && s_sensors_ball_right_found)//the two edges (not cut) were found
                    {
                    	s_sensors_ball_found = true;
                    	s_sensors_ball_angle = (s_sensors_ball_left_angle + s_sensors_ball_right_angle)/2.f;
                        chprintf((BaseSequentialStream *)&SD3, "ball is located in between: %f and %f degrees\n", s_sensors_ball_right_angle, s_sensors_ball_left_angle);
                        chprintf((BaseSequentialStream *)&SD3, "angle is %f\n", s_sensors_ball_angle);
                    }

                    last_fall_pos = NO_RISE_FALL_FOUND_POS;
                }
            }
        }
        if(pixel_derivative <= -GREEN_PIXEL_RISE_FALL_THRESHOLD)
        {
            last_fall_pos = i;
            sum = 0;
        }
        pixel_derivative = (int16_t)green_pixels[DERIVATION_PERIOD_DELTA + 2] - (int16_t)green_pixels[i - DERIVATION_PERIOD_DELTA];
    }
}

void sensors_start(void)
{
	// image
	chThdCreateStatic(wa_process_image, sizeof(wa_process_image), NORMALPRIO, process_image, NULL);
	chThdCreateStatic(wa_acquire_image, sizeof(wa_acquire_image), NORMALPRIO, acquire_image, NULL);
	// TOF distance
	VL53L0X_start();
}

void sensors_set_ball_to_be_search(void)
{
	s_sensors_ball_found = false;
}

bool sensors_is_ball_found(float * ball_angle, float * ball_seen_angle)
{
	if(s_sensors_ball_found)
	{
		*ball_angle = s_sensors_ball_angle;
		*ball_seen_angle = s_sensors_ball_left_angle - s_sensors_ball_right_angle;
	}
	return s_sensors_ball_found;
}

bool sensors_can_move(void)
{
	/// @PI: Ne marche pas � cause de l'orientation trop vers le haut du VL53L0X
	debug_send_uint32_to_computer(VL53L0X_get_dist_mm());
	return VL53L0X_get_dist_mm() > MOVE_SECURITY_SPACE;	// VL53L0X is now an opaque type outside this module.
}

void * sensors_get_semaphore_authorization_move(void)
{
	return &sensors_semaphore_image_completed;
}
