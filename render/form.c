/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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
#include "netsurf/utils/log.h"
#include "netsurf/utils/utf8.h"
#include "netsurf/utils/utils.h"


static char *form_textarea_value(struct form_control *textarea);

/**
 * Create a struct form.
 *
 * \param  action  URL to submit form to, used directly (not copied)
 * \param  method  method and enctype
 * \param  charset characterset of form (not copied)
 * \return  a new structure, or 0 on memory exhaustion
 */

struct form *form_new(char *action, form_method method, char *charset)
{
	struct form *form;

	form = malloc(sizeof *form);
	if (!form)
		return 0;
	form->action = action;
	form->method = method;
	form->charset = charset;
	form->controls = 0;
	form->last_control = 0;
	form->prev = 0;
	return form;
}


/**
 * Create a struct form_control.
 *
 * \param  type  control type
 * \return  a new structure, or 0 on memory exhaustion
 */

struct form_control *form_new_control(form_control_type type)
{
	struct form_control *control;

	if ((control = malloc(sizeof *control)) == NULL)
		return NULL;
	control->type = type;
	control->name = NULL;
	control->value = NULL;
	control->initial_value = NULL;
	control->disabled = false;
	control->form = NULL;
	control->box = NULL;
	control->caret_inline_container = NULL;
	control->caret_text_box = NULL;
	control->caret_box_offset = control->caret_form_offset = 0;
	control->length = control->maxlength = 0;
	control->selected = false;
	control->prev = NULL;
	control->next = NULL;
	return control;
}


/**
 * Add a control to the list of controls in a form.
 */

void form_add_control(struct form *form, struct form_control *control)
{
	control->form = form;
	if (form->controls != NULL) {
		assert(form->last_control);
		form->last_control->next = control;
		control->prev = form->last_control;
		control->next = NULL;
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
 * Add an option to a form select control.
 *
 * \param  control   form control of type GADGET_SELECT
 * \param  value     value of option, used directly (not copied)
 * \param  text      text for option, used directly (not copied)
 * \param  selected  this option is selected
 * \return  true on success, false on memory exhaustion
 */

bool form_add_option(struct form_control *control, char *value, char *text,
		bool selected)
{
	struct form_option *option;

	assert(control);
	assert(control->type == GADGET_SELECT);

	option = malloc(sizeof *option);
	if (!option)
		return false;
	option->selected = option->initial_selected = false;
	option->value = value;
	option->text = text;
	option->next = 0;

	/* add to linked list */
	if (control->data.select.items == 0)
		control->data.select.items = option;
	else
		control->data.select.last_item->next = option;
	control->data.select.last_item = option;

	/* set selected */
	if (selected && (control->data.select.num_selected == 0 ||
			control->data.select.multiple)) {
		option->selected = option->initial_selected = true;
		control->data.select.num_selected++;
		control->data.select.current = option;
	}

	control->data.select.num_items++;

	return true;
}


/**
 * Identify 'successful' controls.
 *
 * \param  form           form to search for successful controls
 * \param  submit_button  control used to submit the form, if any
 * \parma  successful_controls  updated to point to linked list of
 *                        form_successful_control, 0 if no controls
 * \return  true on success, false on memory exhaustion
 *
 * See HTML 4.01 section 17.13.2.
 */

bool form_successful_controls(struct form *form,
		struct form_control *submit_button,
		struct form_successful_control **successful_controls)
{
	struct form_control *control;
	struct form_option *option;
	struct form_successful_control sentinel, *last_success, *success_new;
	char *value;

	last_success = &sentinel;
	sentinel.next = 0;

	for (control = form->controls; control; control = control->next) {
		/* ignore disabled controls */
		if (control->disabled)
			continue;

		/* ignore controls with no name */
		if (!control->name)
			continue;

		switch (control->type) {
			case GADGET_HIDDEN:
				/* ignore hidden controls with no value */
				/* this fixes ebay silliness */
				if (!control->value)
					continue;

				/* fall through */
			case GADGET_TEXTBOX:
			case GADGET_PASSWORD:
				value = strdup(control->value);
				if (!value) {
					LOG(("failed to duplicate value"
						"'%s' for control %s",
							control->value,
							control->name));
					goto no_memory;
				}
				break;

			case GADGET_RADIO:
			case GADGET_CHECKBOX:
				/* ignore checkboxes and radio buttons which
				 * aren't selected */
				if (!control->selected)
					continue;
				if (control->value)
					value = strdup(control->value);
				else
					value = strdup("on");
				if (!value) {
					LOG(("failed to duplicate"
						"value '%s' for"
						"control %s",
						control->value,
						control->name));
					goto no_memory;
				}
				break;

			case GADGET_SELECT:
				/* select */
				for (option = control->data.select.items;
						option != NULL;
						option = option->next) {
					if (!option->selected)
						continue;
					success_new = malloc(sizeof(*success_new));
					if (!success_new) {
						LOG(("malloc failed"));
						goto no_memory;
					}
					success_new->file = false;
					success_new->name = strdup(control->name);
					success_new->value = strdup(option->value);
					success_new->next = NULL;
					last_success->next = success_new;
					last_success = success_new;
					if (!success_new->name ||
						!success_new->value) {
						LOG(("strdup failed"));
						goto no_memory;
					}
				}

				continue;
				break;

			case GADGET_TEXTAREA:
				/* textarea */
				value = form_textarea_value(control);
				if (!value) {
					LOG(("failed handling textarea"));
					goto no_memory;
				}
				if (value[0] == 0) {
					free(value);
					continue;
				}
				break;

			case GADGET_IMAGE: {
				/* image */
				const size_t len = strlen(control->name) + 3;

				/* x */
				success_new = malloc(sizeof(*success_new));
				if (!success_new) {
					LOG(("malloc failed"));
					goto no_memory;
				}
				success_new->file = false;
				success_new->name = malloc(len);
				success_new->value = malloc(20);
				if (!success_new->name ||
						!success_new->value) {
					free(success_new->name);
					free(success_new->value);
					free(success_new);
					LOG(("malloc failed"));
					goto no_memory;
				}
				sprintf(success_new->name, "%s.x",
						control->name);
				sprintf(success_new->value, "%i",
						control->data.image.mx);
				success_new->next = 0;
				last_success->next = success_new;
				last_success = success_new;

				/* y */
				success_new = malloc(sizeof(*success_new));
				if (!success_new) {
					LOG(("malloc failed"));
					goto no_memory;
				}
				success_new->file = false;
				success_new->name = malloc(len);
				success_new->value = malloc(20);
				if (!success_new->name ||
						!success_new->value) {
					free(success_new->name);
					free(success_new->value);
					free(success_new);
					LOG(("malloc failed"));
					goto no_memory;
				}
				sprintf(success_new->name, "%s.y",
						control->name);
				sprintf(success_new->value, "%i",
						control->data.image.my);
				success_new->next = 0;
				last_success->next = success_new;
				last_success = success_new;

				continue;
				break;
			}

			case GADGET_SUBMIT:
				/* only the activated submit button is
				 * successful */
				if (control != submit_button)
					continue;
				value = strdup(control->value);
				if (!value) {
					LOG(("failed to duplicate value"
						"'%s' for control %s",
							control->value,
							control->name));
					goto no_memory;
				}
				break;

			case GADGET_RESET:
				/* ignore reset */
				continue;
				break;

			case GADGET_FILE:
				/* file */
				if (!control->value)
					continue;
				success_new = malloc(sizeof(*success_new));
				if (!success_new) {
					LOG(("malloc failed"));
					goto no_memory;
				}
				success_new->file = true;
				success_new->name = strdup(control->name);
				success_new->value = strdup(control->value);
				success_new->next = 0;
				last_success->next = success_new;
				last_success = success_new;
				if (!success_new->name ||
						!success_new->value) {
					LOG(("strdup failed"));
					goto no_memory;
				}

				continue;
				break;

			default:
				assert(0);
				break;
		}

		success_new = malloc(sizeof(*success_new));
		if (!success_new) {
			LOG(("malloc failed"));
			goto no_memory;
		}
		success_new->file = false;
		success_new->name = strdup(control->name);
		success_new->value = value;
		success_new->next = NULL;
		last_success->next = success_new;
		last_success = success_new;
		if (!success_new->name) {
			LOG(("failed to duplicate name '%s'",
					control->name));
			goto no_memory;
		}
	}

	*successful_controls = sentinel.next;
	return true;

no_memory:
	warn_user("NoMemory", 0);
	form_free_successful(sentinel.next);
	return false;
}


/**
 * Find the value for a textarea control.
 *
 * \param  textarea  control of type GADGET_TEXTAREA
 * \return  the value as a UTF-8 string on heap, or 0 on memory exhaustion
 */

char *form_textarea_value(struct form_control *textarea)
{
	unsigned int len = 0;
	char *value, *s;
	struct box *text_box;

	/* find required length */
	for (text_box = textarea->box->children->children; text_box;
			text_box = text_box->next) {
		if (text_box->type == BOX_INLINE)
			len += text_box->length + 1;
		else /* BOX_BR */
			len += 2;
	}

	/* construct value */
	s = value = malloc(len + 1);
	if (!s)
		return 0;
	for (text_box = textarea->box->children->children; text_box;
			text_box = text_box->next) {
		if (text_box->type == BOX_INLINE) {
			strncpy(s, text_box->text, text_box->length);
			s += text_box->length;
			if (text_box->next)
				/* only add space if this isn't the last box */
				*s++ = ' ';
		} else { /* BOX_BR */
			*s++ = '\r';
			*s++ = '\n';
		}
	}
	*s = 0;

	return value;
}


/**
 * Encode controls using application/x-www-form-urlencoded.
 *
 * \param  form  form to which successful controls relate
 * \param  control  linked list of form_successful_control
 * \return  URL-encoded form, or 0 on memory exhaustion
 */

char *form_url_encode(struct form *form,
		struct form_successful_control *control)
{
	char *name, *value, *n_temp, *v_temp;
	char *s = malloc(1), *s2;
	unsigned int len = 0, len1;

	if (!s)
		return 0;
	s[0] = 0;

	for (; control; control = control->next) {
		n_temp = utf8_to_enc(control->name, form->charset, 0);
		if (!n_temp) {
			free(s);
			return 0;
		}
		v_temp = utf8_to_enc(control->value, form->charset, 0);
		if (!v_temp) {
			free(n_temp);
			free(s);
			return 0;
		}
		name = curl_escape(n_temp, 0);
		value = curl_escape(v_temp, 0);
		len1 = len + strlen(name) + strlen(value) + 2;
		s2 = realloc(s, len1 + 1);
		if (!s2) {
			curl_free(value);
			curl_free(name);
			free(v_temp);
			free(n_temp);
			free(s);
			return 0;
		}
		s = s2;
		sprintf(s + len, "%s=%s&", name, value);
		len = len1;
		curl_free(name);
		curl_free(value);
		free(v_temp);
		free(n_temp);
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
