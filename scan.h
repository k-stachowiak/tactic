#ifndef SCAN_H
#define SCAN_H

#include "data.h"

/** @brief Scans a line between two points, calling a callback function
  *        for each point along the scan line. If the callback returns
  *        a non-zero value, the scan is stopped and the value is returned.
  * @param x1 The x coordinate of the start point.
  * @param y1 The y coordinate of the start point.
  * @param x2 The x coordinate of the end point.
  * @param y2 The y coordinate of the end point.
  * @param d The data object for the scan to be performed in.
  * @param func The callback function.
  * @return 0 if scan was performed without interruption, the hit value
  *         if the scan was interrupted.
  */
int scan_generic(
		int x1, int y1, int x2, int y2,
		struct Data* d, int(*func)(struct Data*, int, int));

/** @brief Callback function for a map plotting scan.
  * @param d The data in which the scan is performed.
  * @param x The x coordinate of the scanned point.
  * @param y The y coordinate of the scanned point.
  * @return 0 in nothing was detected, 1 if an obstacle was hit.
  */
int scan_plot(struct Data *d, int x, int y);

/** @brief Callback for the visibility scan.
  * @param d The data in which the scan is performed.
  * @param x The x coordinate of the scanned point.
  * @param y The y coordinate of the scanned point.
  * @return 0 if nothing was hit,
  *         FAKE_ASTEROID_INDEX if asteroid was hit,
  *         FAKE_PLAYER_INDEX if player was hit,
  *         index of enemy + 1 if an enemy was hit.
  */
int scan_visibility(struct Data *d, int x, int y);

#endif
