#include "central.h"

// ChibiOS & others
#include "ch.h"					// main include files

// this project files
#include "move.h"
#include "sensors.h"
#include "constantes.h"

// local defines

// semaphores
static BSEMAPHORE_DECL(central_semaphore_image_request, TRUE);


void central_control_loop(void)
{
	//PI: pour une raison peu connue, le robot ne fera pas assez d'�tapes au premier tour
	// Ce sympt�me dispara�t quand on essaye de connecter l'e-puck
	// chThdSleepMilliseconds(5000); // wait for user to place e-puck on ground

	float ball_angle = 0.f;
	bool ball_found = false;
	while(1)
	{
		chThdSleepMilliseconds(5000);

		ball_found = false;
		sensors_set_ball_to_be_search();
		for(uint8_t i = 0 ; i < EPUCK_SEARCH_NB_ROTATION ; ++i)
		{
			// speed 50 in 222 ms
			// speed 40 in 277 ms
			move_rotate(EPUCK_SEARCH_ROTATION_ANGLE, 50);

			// wait for the end of the turn plus some inertia stability (e-puck is shaky)
			// meanwhile, image process can occur
			//chThdSleepMilliseconds(250);
			chThdSleepMilliseconds(1100);

			chBSemSignal(&central_semaphore_image_request);
			chBSemWait(sensors_get_semaphore_authorization_move());

			ball_found = sensors_is_ball_found(&ball_angle);
			if(ball_found)
				break;
		}
		if(ball_found)
		{
			sensors_set_ball_to_be_search();

			move_rotate(ball_angle, 50);
			chThdSleepMilliseconds(1000);

			move_until_obstacle(500);
		}
	}
	/*while(1)
	{
		chBSemSignal(&central_semaphore_image_request);
		chBSemWait(central_get_semaphore_authorization_move());
		chThdSleepMilliseconds(6000);
	}*/
}

void * central_get_semaphore_authorization_acquire(void)
{
	return &central_semaphore_image_request;
}