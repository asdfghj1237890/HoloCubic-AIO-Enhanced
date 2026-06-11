/**
 * @file lv_port_indev_templ.c
 *
 */

 /*Copy this file as "lv_port_indev.c" and set this value to "1" to enable content*/

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"

static void encoder_init(void);
static void encoder_read(lv_indev_t* indev, lv_indev_data_t* data);
static void encoder_handler(void);


lv_indev_t* indev_encoder;

int32_t encoder_diff;
lv_indev_state_t encoder_state;


void lv_port_indev_init(void)
{
	/* Here you will find example implementation of input devices supported by LittelvGL:
	 *  - Touchpad
	 *  - Mouse (with cursor support)
	 *  - Keypad (supports GUI usage only with key)
	 *  - Encoder (supports GUI usage only with: left, right, push)
	 *  - Button (external buttons to press points on the screen)
	 *
	 *  The `..._read()` function are only examples.
	 *  You should shape them according to your hardware
	 */


	/*------------------
	 * Encoder
	 * -----------------*/

	 /*Initialize your encoder if you have*/
	encoder_init();

	/*Register a encoder input device*/
	indev_encoder = lv_indev_create();
	lv_indev_set_type(indev_encoder, LV_INDEV_TYPE_ENCODER);
	lv_indev_set_read_cb(indev_encoder, encoder_read);

	/* Later you should create group(s) with `lv_group_t * group = lv_group_create()`,
	 * add objects to the group with `lv_group_add_obj(group, obj)`
	 * and assign this input device to group to navigate in it:
	 * `lv_indev_set_group(indev_encoder, group);` */


}

/**********************
 *   STATIC FUNCTIONS
 **********************/


 /*------------------
  * Encoder
  * -----------------*/

  /* Initialize your keypad */
static void encoder_init(void)
{
	/*Your code comes here*/
}

/* Will be called by the library to read the encoder */
static void encoder_read(lv_indev_t* indev, lv_indev_data_t* data)
{
	LV_UNUSED(indev);

	data->enc_diff = encoder_diff;
	data->state = encoder_state;

	encoder_diff = 0;
}

/*Call this function in an interrupt to process encoder events (turn, press)*/
static void encoder_handler(void)
{
	/*Your code comes here*/


	encoder_diff += 0;
	encoder_state = LV_INDEV_STATE_REL;
}
