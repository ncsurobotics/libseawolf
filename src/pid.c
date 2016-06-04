/**
 * \file
 * \brief PID controller
 */

#include "seawolf.h"

/**
 * \defgroup PID Proportional-Integral-Derivative (PID) controller
 * \ingroup Utilities
 * \brief Provides an implementation of a Proportional-Integral-Derivative (PID) controller
 * \sa http://en.wikipedia.org/wiki/PID_Controller
 * \{
 */

/**
 * \brief Create a new PID controller object
 *
 * Instantiates a new PID controller object associated with the given set point
 * and coefficients
 *
 * \param sp The initial set point for the PID
 * \param p Initial proportional coefficient
 * \param i Initial integral coefficient
 * \param d Initial differential coefficient
 * \return The new PID object
 */

PID* PID_new(double sp, double p, double i, double d) {
    PID* pid = malloc(sizeof(PID));
    if(pid == NULL) {
        return NULL;
    }

    pid->timer = Timer_new();

    pid->p = p;
    pid->i = i;
    pid->d = d;

    pid->sp = sp;
    pid->active_region = -1;

    pid->e_last = 0;
    pid->e_dt = 0;

    pid->paused = true;
    
    //NEW: pid low pass filter for d term
    double* buf = malloc(1*sizeof(double));
    if(buf == NULL) {
        printf("unable to spawn PID d buffer!\n");
    }
    
    PID_LPF_t d_lpf = {
        .value=0,
        .buf=buf,
        .head=0,
        .n=1
    };
    
    // attach lp
    pid->d_lpf = d_lpf;

    return pid;
}

/**
 * \brief Temporary pause a PIDs operation
 *
 * Pause the PID timers. The PID will resumning normal operation without
 * reseting the integral component at the next update.
 *
 * \param pid The controller object
 */
void PID_pause(PID* pid) {
    pid->paused = true;
}

/**
 * \brief Update and return the manipulated variable based on the new process variable
 *
 * Return the new value of the maniuplated variable (mv) based on the new value
 * of the given process variable (pv)
 *
 * \param pid The controller object
 * \param pv The new process variable (pv)
 * \return The new manipulated variable after considering the new process variable
 */
double PID_update(PID* pid, double pv) {
    double delta_t = Timer_getDelta(pid->timer);
    double e = pid->sp - pv;
    double mv;
    double raw_diff_e = 0;

    /* Calculate output value */
    mv  = pid->p * e;

    /* Skip these if we are paused */
    if(pid->paused == false) {
        /* Update running error */
        pid->e_dt += delta_t * e;

	/* prevent I from over-saturating */
	if (pid->e_dt * pid->i > 1) {
		pid->e_dt = 1/pid->i;
	} else if (pid->e_dt * pid->i < -1) {
		pid->e_dt = -1/pid->i;
	}

	/* if error is outside the linear region, reset the ITerm.
	This will also prevent I from over-saturating and from having a
	residual effect when system reaches set point */
	if (pid->active_region > 0) {
	    if (abs(e) > pid->active_region) {
	        pid->e_dt = 0;
            }
        }

        mv += pid->i * pid->e_dt;
    }
    
    /* compute D term */
    raw_diff_e = ((e - pid->e_last) / delta_t);
    mv        += pid->d * PID_stepLPF(pid, raw_diff_e);

    /* Unpause (why?)*/
    pid->paused = false;

    /* Store error */
    pid->e_last = e;

    return mv;
}

double PID_stepLPF(PID* pid, double val) {
    double sum = 0;
    
    // write value to buffer
    pid->d_lpf.buf[ pid->d_lpf.head ] = val;
    
    // run summation
    for(int i=0; i < pid->d_lpf.n; i++) {
        sum += pid->d_lpf.buf[i];
    }
    
    // average and increment pointer
    pid->d_lpf.value = (sum / pid->d_lpf.n);
    pid->d_lpf.head = (pid->d_lpf.head + 1) % pid->d_lpf.n;
    
    return pid->d_lpf.value;
}

/**
 * \brief Reset the integral component
 *
 * Reset the cumulative error associated with the integral component of the
 * controller to 0.
 *
 * \param pid The controller object
 */
void PID_resetIntegral(PID* pid) {
    pid->e_dt = 0;
}

/**
 * \brief Change the set point for the controller
 *
 * Change the set point (sp) for the controller. It is a good idea to reset the
 * controller after calling this. The PID is considered "paused" after this so
 * the next update call will reinitialize the controller and reset the
 * derivative component.
 *
 * \param pid The controller object
 * \param sp The new set point for the controller
 */
void PID_setSetPoint(PID* pid, double sp) {
    pid->sp = sp;
    pid->paused = true;
}

/**
 * \brief Change coefficients
 *
 * Change the coefficients associated with the controller
 *
 * \param pid The controller object
 * \param p The proportional coefficient
 * \param i The integral coefficient
 * \param d The differential coefficient
 */
void PID_setCoefficients(PID* pid, double p, double i, double d) {
    pid->p = p;
    pid->i = i;
    pid->d = d;
}

/**
 * \brief Define the (plus/minus) size of the active region
 *
 * Outside this region, thrusters will likley be maxed out
 * and in order to prevent ITerm saturation, the ITerm will
 * be ignored outside this region.
 *
 * \param pid The controller object
 */
void PID_setActiveRegion(PID* pid, double active_region) {
    pid->active_region = active_region;
}

/**
 * \brief 
 *
 * 
 *
 * \param -
 */
void PID_setDerivativeBufferSize(PID* pid, uint8_t n) {
    // tray and allocate memory for the 
    double* temp = malloc(n*sizeof(double));
    if (temp == NULL) {
        printf("Couldnt allocate memory for the derivative buffer!\n");
        return;
    }
    // deallocate memory that is no longer needed
    free(pid->d_lpf.buf);
    
    // allocate memory for the new buffer size
    pid->d_lpf.buf = temp;
    pid->d_lpf.n = n;
    
    // wipe the new memory clean
    for(int i=0; i < n; i++) {
        pid->d_lpf.buf[i] = 0.0;
    }
}

/**
 * \brief Destroy the controller object
 *
 * Destroy and free the memory associated with the given controller
 *
 * \param pid The controller object
 */
void PID_destroy(PID* pid) {
    Timer_destroy(pid->timer);
    free(pid);
}

/** \} */
