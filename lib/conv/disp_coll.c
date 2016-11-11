/**
 * @brief Dispatching record collector
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

#include <aushape/conv/disp_coll.h>
#include <aushape/conv/execve_coll.h>
#include <aushape/conv/single_coll.h>
#include <aushape/conv/coll.h>
#include <aushape/misc.h>
#include <string.h>

/** Record type name -> collector instance link */
struct aushape_conv_disp_coll_inst_link {
    /** Record type name */
    const char                 *name;
    /** Collector instance */
    struct aushape_conv_coll   *inst;
};

struct aushape_conv_disp_coll {
    /** Abstract base collector */
    struct aushape_conv_coll    coll;
    /**
     * Record type name -> collector instance map,
     * terminated by a link with NULL name, and a catch-all collector
     */
    struct aushape_conv_disp_coll_inst_link    *map;
};

static const struct aushape_conv_single_coll_args
                            aushape_conv_disp_coll_args_default_single = {
    .unique = false
};

static const struct aushape_conv_disp_coll_type_link
                            aushape_conv_disp_coll_args_default[] = {
    {
        .name  = NULL,
        .type  = &aushape_conv_single_coll_type,
        .args  = &aushape_conv_disp_coll_args_default_single
    }
};

static bool
aushape_conv_disp_coll_is_valid(const struct aushape_conv_coll *coll)
{
    struct aushape_conv_disp_coll *disp_coll =
                    (struct aushape_conv_disp_coll *)coll;
    struct aushape_conv_disp_coll_inst_link    *link;

    if (disp_coll->map == NULL) {
        return false;
    }

    link = disp_coll->map;
    do {
        if (!aushape_conv_coll_is_valid(link->inst)) {
            return false;
        }
    } while ((link++)->name != NULL);

    return true;
}

static enum aushape_rc
aushape_conv_disp_coll_init(struct aushape_conv_coll *coll,
                            const void *args)
{
    struct aushape_conv_disp_coll *disp_coll =
                    (struct aushape_conv_disp_coll *)coll;
    enum aushape_rc rc;
    const struct aushape_conv_disp_coll_type_link  *type_map = 
                (const struct aushape_conv_disp_coll_type_link *)args;
    const struct aushape_conv_disp_coll_type_link  *type_link;
    size_t map_size;
    struct aushape_conv_disp_coll_inst_link    *inst_map;
    struct aushape_conv_disp_coll_inst_link    *inst_link;

    if (type_map == NULL) {
        type_map = aushape_conv_disp_coll_args_default;
    }

    /* Count entries in type map */
    for (map_size = 1, type_link = type_map;
         type_link->name != NULL;
         map_size++, type_link++);

    /* Create instance link array */
    inst_map = malloc(sizeof(*inst_map) * map_size);
    if (inst_map == NULL) {
        rc = AUSHAPE_RC_NOMEM;
        goto cleanup;
    }

    /* Initialize instance links */
    type_link = type_map;
    inst_link = inst_map;
    while (true) {
        inst_link->name = type_link->name;
        rc = aushape_conv_coll_create(&inst_link->inst, type_link->type,
                                      &coll->format, coll->gbuf,
                                      type_link->args);
        if (rc != AUSHAPE_RC_OK) {
            assert(rc != AUSHAPE_RC_INVALID_ARGS);
            goto cleanup;
        }
        if (type_link->name == NULL) {
            break;
        }
        inst_link++;
        type_link++;
    }

    /* Store created instance link array */
    disp_coll->map = inst_map;
    inst_map = NULL;
    rc = AUSHAPE_RC_OK;

cleanup:
    if (inst_map != NULL) {
        inst_link = inst_map;
        while (true) {
            if (inst_link->inst == NULL) {
                break;
            }
            aushape_conv_coll_destroy(inst_link->inst);
            inst_link->inst = NULL;
            if (inst_link->name == NULL) {
                break;
            }
            inst_link++;
        }
        free(inst_map);
    }
    return rc;
}

static void
aushape_conv_disp_coll_cleanup(struct aushape_conv_coll *coll)
{
    struct aushape_conv_disp_coll *disp_coll =
                    (struct aushape_conv_disp_coll *)coll;
    struct aushape_conv_disp_coll_inst_link *link = disp_coll->map;

    do {
        aushape_conv_coll_destroy(link->inst);
        link->inst = NULL;
    } while ((link++)->name != NULL);
    free(disp_coll->map);
}

static bool
aushape_conv_disp_coll_is_empty(const struct aushape_conv_coll *coll)
{
    struct aushape_conv_disp_coll *disp_coll =
                    (struct aushape_conv_disp_coll *)coll;
    struct aushape_conv_disp_coll_inst_link *link = disp_coll->map;
    do {
        if (!aushape_conv_coll_is_empty(link->inst)) {
            return false;
        }
    } while ((link++)->name != NULL);
    return true;
}

static void
aushape_conv_disp_coll_empty(struct aushape_conv_coll *coll)
{
    struct aushape_conv_disp_coll *disp_coll =
                    (struct aushape_conv_disp_coll *)coll;
    struct aushape_conv_disp_coll_inst_link *link = disp_coll->map;
    do {
        aushape_conv_coll_empty(link->inst);
    } while ((link++)->name != NULL);
}

/**
 * Lookup a collector corresponding to a record type name within a dispatcher
 * collector map.
 *
 * @param coll      The dispatcher collector to lookup the collector in.
 * @param name      The record type name to use for lookup.
 *
 * @return A collector corresponding to the record type name.
 */
static struct aushape_conv_coll *
aushape_conv_disp_coll_lookup(struct aushape_conv_coll *coll,
                              const char *name)
{
    struct aushape_conv_disp_coll *disp_coll =
                    (struct aushape_conv_disp_coll *)coll;
    struct aushape_conv_disp_coll_inst_link *link;

    assert(aushape_conv_coll_is_valid(coll));
    assert(coll->type == &aushape_conv_disp_coll_type);
    assert(name != NULL);

    for (link = disp_coll->map;
         link->name != NULL && strcmp(link->name, name) != 0;
         link++);

    return link->inst;
}

static enum aushape_rc
aushape_conv_disp_coll_add(struct aushape_conv_coll *coll,
                           size_t level,
                           bool *pfirst,
                           auparse_state_t *au)
{
    const char *name;
    assert(aushape_conv_coll_is_valid(coll));
    assert(coll->type == &aushape_conv_disp_coll_type);
    assert(pfirst != NULL);
    assert(au != NULL);

    name = auparse_get_type_name(au);
    if (name == NULL) {
        return AUSHAPE_RC_CONV_AUPARSE_FAILED;
    }
    return aushape_conv_coll_add(aushape_conv_disp_coll_lookup(coll, name),
                                 level, pfirst, au);
}

static enum aushape_rc
aushape_conv_disp_coll_end(struct aushape_conv_coll *coll,
                           size_t level,
                           bool *pfirst)
{
    struct aushape_conv_disp_coll *disp_coll =
                    (struct aushape_conv_disp_coll *)coll;
    struct aushape_conv_disp_coll_inst_link *link;
    enum aushape_rc rc;

    assert(aushape_conv_coll_is_valid(coll));
    assert(coll->type == &aushape_conv_disp_coll_type);
    assert(pfirst != NULL);

    link = disp_coll->map;
    do {
        rc = aushape_conv_coll_end(link->inst, level, pfirst);
        if (rc != AUSHAPE_RC_OK) {
            assert(rc != AUSHAPE_RC_INVALID_ARGS);
            return rc;
        }
    } while ((link++)->name != NULL);

    return AUSHAPE_RC_OK;
}

const struct aushape_conv_coll_type aushape_conv_disp_coll_type = {
    .size       = sizeof(struct aushape_conv_disp_coll),
    .init       = aushape_conv_disp_coll_init,
    .is_valid   = aushape_conv_disp_coll_is_valid,
    .cleanup    = aushape_conv_disp_coll_cleanup,
    .is_empty   = aushape_conv_disp_coll_is_empty,
    .empty      = aushape_conv_disp_coll_empty,
    .add        = aushape_conv_disp_coll_add,
    .end        = aushape_conv_disp_coll_end,
};