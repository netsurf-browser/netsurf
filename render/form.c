/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2005 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * Form handling functions (implementation).
 */

#define _GNU_SOURCE  /* for strndup */
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "netsurf/render/box.h"
#include "netsurf/render/form.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utf8.h"
#include "netsurf/utils/utils.h"


static char *form_textarea_value(struct form_control *textarea);
static char *form_acceptable_charset(struct form *form);

/**
 * Create a struct form.
 *
 * \param  action  URL to submit form to, used directly (not copied)
 * \param  method  method and enctype
 * \param  charset acceptable charactersets for form submission (not copied)
 * \param  doc_charset  characterset of containing document (not copied)
 * \return  a new structure, or 0 on memory exhaustion
 */

struct form *form_new(char *action, char *target, form_method method,
		char *charset, char *doc_charset)
{
	struct form *form;

	form = malloc(sizeof *form);
	if (!form)
		return 0;
	form->action = action;
	form->target = target;
	form->method = method;
	form->accept_charsets = charset;
	form->document_charset = doc_charset;
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
 *
 * \param form  The form to add the control to
 * \param control  The control to add
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
 * \param  successful_controls  updated to point to linked list of
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
	bool had_submit = false;

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
			case GADGET_TEXTBOX:
			case GADGET_PASSWORD:
				if (control->value)
					value = strdup(control->value);
				else
					value = strdup("");
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
				if (control != submit_button)
					/* only the activated submit button
					 * is successful */
					continue;

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
				if (!submit_button && !had_submit)
					/* no submit button specified, so
					 * use first declared in form */
					had_submit = true;
				else if (control != submit_button)
					/* only the activated submit button
					 * is successful */
					continue;
				if (control->value)
					value = strdup(control->value);
				else
					value = strdup("");
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
				/* Handling of blank file entries is
				 * implementation defined - we're perfectly
				 * within our rights to treat it as an
				 * unsuccessful control. Unfortunately, every
				 * other browser submits the field with
				 * a blank filename and no content. So,
				 * that's what we have to do, too.
				 */
				success_new = malloc(sizeof(*success_new));
				if (!success_new) {
					LOG(("malloc failed"));
					goto no_memory;
				}
				success_new->file = true;
				success_new->name = strdup(control->name);
				success_new->value = strdup(control->value ?
						control->value : "");
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
		if (text_box->type == BOX_TEXT)
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
		if (text_box->type == BOX_TEXT) {
			strncpy(s, text_box->text, text_box->length);
			s += text_box->length;
			if (text_box->next && text_box->next->type != BOX_BR)
				/* only add space if this isn't
				 * the last box on a line (or in the area) */
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
	char *charset;
	unsigned int len = 0, len1;
	utf8_convert_ret err;
	url_func_result url_err;

	if (!s)
		return 0;
	s[0] = 0;

	charset = form_acceptable_charset(form);
	if (!charset)
		return 0;

	for (; control; control = control->next) {
		err = utf8_to_enc(control->name, charset, 0, &n_temp);
		if (err == UTF8_CONVERT_BADENC) {
			/* charset not understood, try document charset */
			err = utf8_to_enc(control->name,
					form->document_charset, 0, &n_temp);
			if (err == UTF8_CONVERT_BADENC)
				/* that also failed, use 8859-1 */
				err = utf8_to_enc(control->name,
						"ISO-8859-1", 0, &n_temp);
		}
		if (err == UTF8_CONVERT_NOMEM) {
			free(charset);
			free(s);
			return 0;
		}

		assert(err == UTF8_CONVERT_OK);

		err = utf8_to_enc(control->value, charset, 0, &v_temp);
		if (err == UTF8_CONVERT_BADENC) {
			err = utf8_to_enc(control->value,
					form->document_charset, 0, &v_temp);
			if (err == UTF8_CONVERT_BADENC)
				err = utf8_to_enc(control->value,
						"ISO-8859-1", 0, &v_temp);
		}
		if (err == UTF8_CONVERT_NOMEM) {
			free(n_temp);
			free(charset);
			free(s);
			return 0;
		}

		assert(err == UTF8_CONVERT_OK);

		url_err = url_escape(n_temp, true, &name);
		if (url_err == URL_FUNC_NOMEM) {
			free(v_temp);
			free(n_temp);
			free(charset);
			free(s);
			return 0;
		}

		assert(url_err == URL_FUNC_OK);

		url_err = url_escape(v_temp, true, &value);
		if (url_err == URL_FUNC_NOMEM) {
			free(name);
			free(v_temp);
			free(n_temp);
			free(charset);
			free(s);
			return 0;
		}

		assert(url_err == URL_FUNC_OK);

		len1 = len + strlen(name) + strlen(value) + 2;
		s2 = realloc(s, len1 + 1);
		if (!s2) {
			free(value);
			free(name);
			free(v_temp);
			free(n_temp);
			free(charset);
			free(s);
			return 0;
		}
		s = s2;
		sprintf(s + len, "%s=%s&", name, value);
		len = len1;
		free(name);
		free(value);
		free(v_temp);
		free(n_temp);
	}

	free(charset);

	if (len)
		s[len - 1] = 0;
	return s;
}


/**
 * Free a linked list of form_successful_control.
 *
 * \param control Pointer to head of list to free
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

/**
 * Find an acceptable character set encoding with which to submit the form
 *
 * \param form  The form
 * \return Pointer to charset name (on heap, caller should free) or NULL
 */
char *form_acceptable_charset(struct form *form)
{
	char *temp, *c;

	if (!form)
		return NULL;

	if (!form->accept_charsets) {
		/* no accept-charsets attribute for this form */
		if (form->document_charset)
			/* document charset present, so use it */
			return strdup(form->document_charset);
		else
			/* no document charset, so default to 8859-1 */
			return strdup("ISO-8859-1");
	}

	/* make temporary copy of accept-charsets attribute */
	temp = strdup(form->accept_charsets);
	if (!temp)
		return NULL;

	/* make it upper case */
	for (c = temp; *c; c++)
		*c = toupper(*c);

	/* is UTF-8 specified? */
	c = strstr(temp, "UTF-8");
	if (c) {
		free(temp);
		return strdup("UTF-8");
	}

	/* dispense with temporary copy */
	free(temp);

	/* according to RFC2070, the accept-charsets attribute of the
	 * form element contains a space and/or comma separated list */
	c = form->accept_charsets;

	/* What would be an improvement would be to choose an encoding
	 * acceptable to the server which covers as much of the input
	 * values as possible. Additionally, we need to handle the case
	 * where none of the acceptable encodings cover all the textual
	 * input values.
	 * For now, we just extract the first element of the charset list
	 */
	while (*c && !isspace(*c)) {
		if (*c == ',')
			break;
		c++;
	}

	return strndup(form->accept_charsets, c - form->accept_charsets);
}
