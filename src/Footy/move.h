#ifndef MOVE_H_
#define MOVE_H_

#include <stdint.h>
#include <stdbool.h>

// EPFL-MICRO 315 library
#include <motors.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief   Init module threads
*
*/
void move_init_threads(void);

/**
* @brief   Rotate the robot
*
* @param angle				Relative angle of the desired rotation in deg
* @param speed				Rotation speed of right wheel in step/s
*
*/
void move_rotate(float angle, int16_t speed);

/**
* @brief   Move the robot straight until obstacle
*
* @param speed				Translation speed of wheels in step/s
*
*/
void move_until_obstacle(int16_t speed);

/**
* @brief   Move the robot straight for a given distance
*
* @param distance			Distance in mm
* @param speed				Translation speed of wheels in step/s
*
*/
void move_straight(float distance, int16_t speed);
/**
* @brief   Move the robot in half circle around a point at radius in front of him, then face center
*
* @param radius				Circle radius in mm
* @param speed				Translation speed of wheels in step/s
*
*/
void move_round_about(float radius, int16_t speed);

#ifdef __cplusplus
}
#endif

#endif // MOVE_H_
