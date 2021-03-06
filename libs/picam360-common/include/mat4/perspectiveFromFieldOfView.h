#ifndef __mat4_perspectiveFromFieldOfView__
#define __mat4_perspectiveFromFieldOfView__

#include "type.h"
#include <math.h>

/**
 * Generates a perspective projection matrix with the given field of view.
 * This is primarily useful for generating projection matrices to be used
 * with the still experiemental WebVR API.
 *
 * @param {mat4} out mat4 frustum matrix will be written into
 * @param {number} fov Object containing the following values: upDegrees (0), downDegrees (1), leftDegrees (2), rightDegrees (3)
 * @param {number} near Near bound of the frustum
 * @param {number} far Far bound of the frustum
 * @returns {mat4} out
 */
static mat4 mat4_perspectiveFromFieldOfView(mat4 out, float fov[4], float near, float far) {
    float upTan = tanf(fov[0] * M_PI/180.0),
        downTan = tanf(fov[1] * M_PI/180.0),
        leftTan = tanf(fov[2] * M_PI/180.0),
        rightTan = tanf(fov[3] * M_PI/180.0),
        xScale = 2.0 / (leftTan + rightTan),
        yScale = 2.0 / (upTan + downTan);

    out[0] = xScale;
    out[1] = 0.0;
    out[2] = 0.0;
    out[3] = 0.0;
    out[4] = 0.0;
    out[5] = yScale;
    out[6] = 0.0;
    out[7] = 0.0;
    out[8] = -((leftTan - rightTan) * xScale * 0.5);
    out[9] = ((upTan - downTan) * yScale * 0.5);
    out[10] = far / (near - far);
    out[11] = -1.0;
    out[12] = 0.0;
    out[13] = 0.0;
    out[14] = (far * near) / (near - far);
    out[15] = 0.0;
    return out;
}

#endif
