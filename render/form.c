/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 */

/** \file
 * Form handling functions (implementation).
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "curl/curl.h"
#include "netsurf/render/box.h"
#include "netsurf/render/form.h"
#include "netsurf/utils/utils.h"


static char *form_textarea_value(struct form_control *textarea);


/**
 * Create a struct form_control.
 *
 * \param  type  control type
 * \return  a new structure, or 0 on memory exhaustion
 */

struct form_control *form_new_control(form_control_type type)
{
	struct form_control *control;

	control = malloc(sizeof *control);
	if (!control)
		return 0;
	control->type = type;
	control->name = 0;
	control->value = 0;
	control->initial_value = 0;
	control->disabled = false;
	control->form = 0;
	control->box = 0;
	control->selected = false;
	control->prev = 0;
	control->next = 0;
	return control;
}


/**
 * Add a control to the list of controls in a form.
 */

void form_add_control(struct form *form, struct form_control *control)
{
	control->form = form;
	if (form->controls) {
		assert(form->last_control);
		form->last_control->next = control;
		control->prev = form->last_control;
		control->next = 0;
		form->last_control = control;
	} else {
		form->controls = form->last_control = control;
	}
}


/**
 * Free a struct form_control.
 *
 * \param  control  structure to free
 */

void form_free_control(struct form_control *control)
{
	free(control->name);
	free(control->value);
	free(control->initial_value);
	if (control->type == GADGET_SELECT) {
		struct form_option *option, *next;
		for (option = control->data.select.items; option;
				option = next) {
			next = option->next;
			free(option->text);
			free(option->value);
			free(option);
		}
	}
	free(control);
}


/**
 * Identify 'successful' controls.
 *
 * See HTML 4.01 section 17.13.2.
 */

struct form_successful_control *form_successful_controls(struct form *form,
		struct form_control *submit_button)
{
	struct form_control *control;
	struct form_option *option;
	struct form_successful_control sentinel, *last_success;
	last_success = &sentinel;
	sentinel.next = 0;

	for (control = form->controls; control; control = control->next) {
		struct form_successful_control *success_new;

		/* ignore disabled controls */
		if (control->disabled)
			continue;

		/* ignore controls with no name */
		if (!control->name)
			continue;

		/* only the activated submit button is successful */
		if (control->type == GADGET_SUBMIT && control != submit_button)
			continue;

		/* ignore checkboxes and radio buttons which aren't selected */
		if (control->type == GADGET_CHECKBOX && !control->selected)
			continue;
		if (control->type == GADGET_RADIO && !control->selected)
			continue;

		/* select */
		if (control->type == GADGET_SELECT) {
			for (option = control->data.select.items; option;
					option = option->next) {
				if (option->selected) {
					success_new = calloc(1, sizeof(*success_new));
					success_new->file = false;
					success_new->name = strdup(control->name);
					success_new->value = strdup(option->value);
					success_new->next = 0;
					last_success->next = success_new;
					last_success = success_new;
				}
			}
			continue;
		}

		/* textarea */
		if (control->type == GADGET_TEXTAREA) {
			success_new = calloc(1, sizeof(*success_new));
			success_new->file = false;
			success_new->name = strdup(control->name);
			success_new->value = form_textarea_value(control);
			success_new->next = 0;
			last_success->next = success_new;
			last_success = success_new;
			continue;
		}

		/* image */
		if (control->type == GADGET_IMAGE) {
			unsigned int len = strlen(control->name) + 3;
			/* x */
			success_new = calloc(1, sizeof(*success_new));
			success_new->file = false;
			success_new->name = calloc(1, len);
			sprintf(success_new->name, "%s.x", control->name);
			success_new->value = calloc(1, 20);
			sprintf(success_new->value, "%i", control->data.image.mx);
			success_new->next = 0;
			last_success->next = success_new;
			last_success = success_new;
			/* y */
			success_new = calloc(1, sizeof(*success_new));
			success_new->file = false;
			success_new->name = calloc(1, len);
			sprintf(success_new->name, "%s.y", control->name);
			success_new->value = calloc(1, 20);
			sprintf(success_new->value, "%i", control->data.image.my);
			success_new->next = 0;
			last_success->next = success_new;
			last_success = success_new;
		}

		/* ignore reset */
		if (control->type == GADGET_RESET)
			continue;

		/* file */
		if (control->type == GADGET_FILE && control->value) {
		        success_new = calloc(1, sizeof(*success_new));
		        success_new->file = true;
		        success_new->name = strdup(control->name);
		        success_new->value = strdup(control->value);
		        success_new->next = 0;
		        last_success->next = success_new;
		        last_success = success_new;
		        continue;
		}

		/* all others added if they have a value */
		if (control->value) {
			success_new = calloc(1, sizeof(*success_new));
			success_new->file = false;
			success_new->name = strdup(control->name);
			success_new->value = strdup(control->value);
			success_new->next = 0;
			last_success->next = success_new;
			last_success = success_new;
		}
	}

	return sentinel.next;
}


/**
 * Find the value for a textarea control.
 */

char *form_textarea_value(struct form_control *textarea)
{
	unsigned int len = 1;
	char *value, *s;
	struct box *inline_container, *text_box;

	/* find required length */
	for (inline_container = textarea->box->children;
			inline_container;
			inline_container = inline_container->next) {
		for (text_box = inline_container->children;
				text_box;
				text_box = text_box->next) {
			len += text_box->length + 1;
		}
		len += 2;
	}

	/* construct value */
	s = value = calloc(1, len);
	for (inline_container = textarea->box->children;
			inline_container;
			inline_container = inline_container->next) {
		for (text_box = inline_container->children;
				text_box;
				text_box = text_box->next) {
			strncpy(s, text_box->text, text_box->length);
			s += text_box->length;
			*s++ = ' ';
		}
		*s++ = '\r';
		*s++ = '\n';
	}
	*s = 0;

	return value;
}


/**
 * Encode controls using application/x-www-form-urlencoded.
 */

char *form_url_encode(struct form_successful_control *control)
{
	char *s = calloc(1, 0);
	unsigned int len = 0, len1;

	for (; control; control = control->next) {
		char *name = curl_escape(control->name, 0);
		char *value = curl_escape(control->value, 0);
		len1 = len + strlen(name) + strlen(value) + 2;
		s = realloc(s, len1 + 1);
		sprintf(s + len, "%s=%s&", name, value);
		len = len1;
                curl_free(name);
                curl_free(value);
	}
	if (len)
		s[len - 1] = 0;
	return s;
}


/**
 * Free a linked list of form_successful_control.
 */

void form_free_successful(struct form_successful_control *control)
{
	struct form_successful_control *next;
	for (; control; control = next) {
		next = control->next;
		free(control->name);
		free(control->value);
		free(control);
	}
}
