#ifndef SENSORS_H_
#define SENSORS_H_

#include <stdint.h>
#include <stdbool.h>

#include "constantes.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Analysis_result {
	uint16_t 				leftAngle;//relative angle of the left edge of the ball
	uint16_t 				rightAngle;//relative angle of the right edge of the ball
	bool					found;//true if the ball was found
	bool					leftCropped;//true if the falling edge is at the end of the detection window
	bool					rightCropped;//true if the falling edge is at the end of the detection window
};

void sensors_start(void);

/**
* @brief   Take an image of a line in front of the robot
*
*/
void capture_front_view(void);

/**
* @brief   Get the result of an analysis, wait until available
*
* @return					Analysis result
*
*/
struct Analysis_result get_analysis_result(void);

#ifdef __cplusplus
}
#endif


#endif //SENSORS_H_
