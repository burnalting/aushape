/**
 * Abstract record collector interface
 *
 * Copyright (C) 2016 Red Hat
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <aushape/conv/coll.h>
#include <string.h>

enum aushape_rc
aushape_conv_coll_create(struct aushape_conv_coll **pcoll,
                         const struct aushape_conv_coll_type *type,
                         const struct aushape_format *format,
                         struct aushape_gbuf *gbuf)
{
    enum aushape_rc rc;
    struct aushape_conv_coll *coll;

    if (pcoll == NULL ||
        !aushape_conv_coll_type_is_valid(type) ||
        !aushape_format_is_valid(format) ||
        !aushape_gbuf_is_valid(gbuf)) {
        return AUSHAPE_RC_INVALID_ARGS;
    }

    coll = calloc(type->size, 1);
    if (coll == NULL) {
        rc = AUSHAPE_RC_NOMEM;
    } else {
        coll->type = type;
        coll->format = *format;
        coll->gbuf = gbuf;

        rc = (type->init != NULL) ? type->init(coll) : AUSHAPE_RC_OK;
        if (rc == AUSHAPE_RC_OK) {
            assert(aushape_conv_coll_is_valid(coll));
            *pcoll = coll;
        } else {
            free(coll);
        }
    }

    return rc;
}

bool
aushape_conv_coll_is_valid(const struct aushape_conv_coll *coll)
{
    return coll != NULL &&
           aushape_conv_coll_type_is_valid(coll->type) &&
           aushape_format_is_valid(&coll->format) &&
           aushape_gbuf_is_valid(coll->gbuf) &&
           (coll->type->is_valid == NULL ||
            coll->type->is_valid(coll));
}

void
aushape_conv_coll_destroy(struct aushape_conv_coll *coll)
{
    assert(coll == NULL || aushape_conv_coll_is_valid(coll));

    if (coll == NULL) {
        return;
    }
    if (coll->type->cleanup != NULL) {
        coll->type->cleanup(coll);
    }
    memset(coll, 0, coll->type->size);
    free(coll);
}

bool
aushape_conv_coll_is_empty(const struct aushape_conv_coll *coll)
{
    assert(aushape_conv_coll_is_valid(coll));
    return coll->type->is_empty(coll);
}

void
aushape_conv_coll_empty(struct aushape_conv_coll *coll)
{
    assert(aushape_conv_coll_is_valid(coll));
    coll->type->empty(coll);
    coll->ended = false;
}

bool
aushape_conv_coll_is_ended(const struct aushape_conv_coll *coll)
{
    assert(aushape_conv_coll_is_valid(coll));
    return coll->ended;
}

enum aushape_rc
aushape_conv_coll_add(struct aushape_conv_coll *coll,
                      size_t level,
                      bool *pfirst,
                      auparse_state_t *au)
{
    if (!aushape_conv_coll_is_valid(coll) || pfirst == NULL || au == NULL) {
        return AUSHAPE_RC_INVALID_ARGS;
    }
    if (aushape_conv_coll_is_ended(coll)) {
        return AUSHAPE_RC_INVALID_STATE;
    }
    return coll->type->add(coll, level, pfirst, au);
}

enum aushape_rc
aushape_conv_coll_end(struct aushape_conv_coll *coll,
                      size_t level,
                      bool *pfirst)
{
    enum aushape_rc rc;
    if (!aushape_conv_coll_is_valid(coll) || pfirst == NULL) {
        return AUSHAPE_RC_INVALID_ARGS;
    }
    if (aushape_conv_coll_is_empty(coll) ||
        aushape_conv_coll_is_ended(coll)) {
        return AUSHAPE_RC_OK;
    }
    rc = (coll->type->end != NULL)
                ? coll->type->end(coll, level, pfirst)
                : AUSHAPE_RC_OK;
    if (rc == AUSHAPE_RC_OK) {
        coll->ended = true;
    }
    return rc;
}
