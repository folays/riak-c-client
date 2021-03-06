/*********************************************************************
 *
 * riak_object.c: Riak C Object
 *
 * Copyright (c) 2007-2014 Basho Technologies, Inc.  All Rights Reserved.
 *
 * This file is provided to you under the Apache License,
 * Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License.  You may obtain
 * a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 *********************************************************************/
#include <time.h>
#include "riak.h"
#include "riak_messages-internal.h"
#include "riak_utils-internal.h"
#include "riak_binary-internal.h"
#include "riak_object-internal.h"
#include "riak_config-internal.h"
#include "riak_print-internal.h"

//
// P A I R S
//

riak_pair*
riak_pair_new(riak_config *cfg) {
    riak_pair *lnk = (riak_pair*)(cfg->malloc_fn)(sizeof(riak_pair));
    if (lnk) memset(lnk, '\0', sizeof(riak_pair));
    return lnk;
}

riak_error
riak_pair_new_array(riak_config  *cfg,
                    riak_pair  ***array,
                    riak_size_t   len) {
    riak_pair **result = (riak_pair**)riak_config_clean_allocate(cfg, sizeof(riak_pair)*len);
    if (result == NULL) {
        return ERIAK_OUT_OF_MEMORY;
    }
    *array = result;

    return ERIAK_OK;
}

static riak_error
riak_pairs_copy_to_pb(riak_config  *cfg,
                      RpbPair    ***pbpair_target,
                      riak_pair   **pair,
                      int           num_pairs) {
    RpbPair **pbpair = riak_config_clean_allocate(cfg, sizeof(RpbPair*) * num_pairs);
    if (pbpair == NULL) {
        return ERIAK_OUT_OF_MEMORY;
    }
    int i;
    for(i = 0; i < num_pairs; i++) {
        pbpair[i] = riak_config_clean_allocate(cfg, sizeof(RpbPair));
        rpb_pair__init(pbpair[i]);
        
        if (pbpair[i] == NULL) {
            return ERIAK_OUT_OF_MEMORY;
        }
        
        riak_binary_copy_to_pb(&(pbpair[i]->key), pair[i]->key);
        
        if (pair[i]->has_value) {
            pbpair[i]->has_value = RIAK_TRUE;
            riak_binary_copy_to_pb(&(pbpair[i]->value), pair[i]->value);
        }
    }
    // Finally assign the pointer to the list of pair pointers
    *pbpair_target = pbpair;

    return ERIAK_OK;
}

static void
riak_pairs_free_pb(riak_config  *cfg,
                   RpbPair    ***pbpair_target,
                   int           num_pairs) {
    RpbPair **pbpair = *pbpair_target;
    int i;
    for(i = 0; i < num_pairs; i++) {
        riak_free(cfg, &(pbpair[i]));
    }
    riak_free(cfg, pbpair_target);
}

riak_error
riak_pairs_copy_from_pb(riak_config *cfg,
                        riak_pair  **pair_target,
                        RpbPair     *pbpair) {

    riak_pair *pair = (riak_pair*)riak_config_clean_allocate(cfg, sizeof(riak_pair));
    if (pair == NULL) {
        return ERIAK_OUT_OF_MEMORY;
    }
    pair->key = riak_binary_copy_from_pb(cfg, &(pbpair->key));
    if (pair->key == NULL) {
        return ERIAK_OUT_OF_MEMORY;
    }
    if (pbpair->has_value) {
        pair->has_value = RIAK_TRUE;
        pair->value = riak_binary_copy_from_pb(cfg, &(pbpair->value));
        if (pair->value == NULL) {
            return ERIAK_OUT_OF_MEMORY;
        }
    }
    // Finally assign the pointer to the list of pair pointers
    *pair_target = pair;

    return ERIAK_OK;
}

riak_error
riak_pairs_copy_array_from_pb(riak_config  *cfg,
                              riak_pair  ***pair_target,
                              RpbPair     **pbpair,
                              int           num_pairs) {
    riak_pair **pair = (riak_pair**)riak_config_clean_allocate(cfg, sizeof(riak_pair*) * num_pairs);
    if (pair == NULL) {
        return ERIAK_OUT_OF_MEMORY;
    }
    int i;
    for(i = 0; i < num_pairs; i++) {
        riak_error err = riak_pairs_copy_from_pb(cfg, &(pair[i]), pbpair[i]);
        if (err != ERIAK_OK) {
            return err;
        }
    }
    // Finally assign the pointer to the list of pair pointers
    *pair_target = pair;

    return ERIAK_OK;
}

riak_int32_t
riak_pairs_print(riak_pair   **pair,
                 riak_uint32_t num_pairs,
                 char        **target,
                 riak_int32_t *len,
                 riak_int32_t *total) {
    riak_int32_t wrote = 0;
    if (*len > 0) {
        riak_int32_t i;
        for(i = 0; i < num_pairs; i++) {
            wrote += riak_print_binary("key", pair[i]->key, target, len, total);
            if (pair[i]->has_value) {
                wrote += riak_print_binary("value", pair[i]->value, target, len, total);
            }
        }
    }
    return wrote;
}

void
riak_pairs_free(riak_config  *cfg,
                riak_pair  ***pair_target,
                int           num_pairs) {
    riak_pair **pair = *pair_target;
    int i;
    for(i = 0; i < num_pairs; i++) {
        riak_free(cfg, &(pair[i]->key));
        riak_free(cfg, &(pair[i]->value));
        riak_free(cfg, &(pair[i]));
    }
    riak_free(cfg, pair_target);
}

//
// L I N K S
//

riak_link*
riak_link_new(riak_config *cfg) {
    riak_link *lnk = (riak_link*)(cfg->malloc_fn)(sizeof(riak_link));
    if (lnk) memset(lnk, '\0', sizeof(riak_link));
    return lnk;
}

static riak_error
riak_links_copy_to_pb(riak_config  *cfg,
                      RpbLink    ***pblink_target,
                      riak_link   **link,
                      int           num_links) {
    RpbLink **pblink = (RpbLink**)(cfg->malloc_fn)(sizeof(RpbLink*) * num_links);
    if (pblink == NULL) {
        return ERIAK_OUT_OF_MEMORY;
    }
    int i;
    for(i = 0; i < num_links; i++) {
        pblink[i] = (RpbLink*)(cfg->malloc_fn)(sizeof(RpbLink));
        if (pblink[i] == NULL) {
            return ERIAK_OUT_OF_MEMORY;
        }
        memset(pblink[i], '\0', sizeof(RpbLink));
        if (link[i]->has_bucket) {
            pblink[i]->has_bucket = RIAK_TRUE;
            riak_binary_copy_to_pb(&(pblink[i]->bucket), link[i]->bucket);
        }
        if (link[i]->has_key) {
            pblink[i]->has_key = RIAK_TRUE;
            riak_binary_copy_to_pb(&(pblink[i]->key), link[i]->key);
        }
        if (link[i]->has_tag) {
            pblink[i]->has_tag = RIAK_TRUE;
            riak_binary_copy_to_pb(&(pblink[i]->tag), link[i]->tag);
        }
    }
    // Finally assign the pointer to the list of link pointers
    *pblink_target = pblink;

    return ERIAK_OK;
}

static void
riak_links_free_pb(riak_config  *cfg,
                   RpbLink    ***pblink_target,
                   int           num_links) {
    RpbLink **pblink = *pblink_target;
    int i;
    for(i = 0; i < num_links; i++) {
        riak_free(cfg, &(pblink[i]));
    }
    riak_free(cfg, pblink_target);
}

static riak_error
riak_links_copy_from_pb(riak_config  *cfg,
                        riak_link  ***link_target,
                        RpbLink     **pblink,
                        int           num_links) {
    riak_link **link = (riak_link**)(cfg->malloc_fn)(sizeof(riak_link*) * num_links);
    if (pblink == NULL) {
        return ERIAK_OUT_OF_MEMORY;
    }
    int i;
    for(i = 0; i < num_links; i++) {
        link[i] = (riak_link*)(cfg->malloc_fn)(sizeof(riak_link));
        if (link[i] == NULL) {
            return ERIAK_OUT_OF_MEMORY;
        }
        memset(link[i], '\0', sizeof(riak_link));
        if (pblink[i]->has_bucket) {
            link[i]->has_bucket = RIAK_TRUE;
            link[i]->bucket = riak_binary_copy_from_pb(cfg, &(pblink[i]->bucket));
            if (link[i]->bucket == NULL) {
                return ERIAK_OUT_OF_MEMORY;
            }
        }
        if (pblink[i]->has_key) {
            pblink[i]->has_key = RIAK_TRUE;
            link[i]->key = riak_binary_copy_from_pb(cfg, &(pblink[i]->key));
            if (link[i]->key == NULL) {
                return ERIAK_OUT_OF_MEMORY;
            }
        }
        if (pblink[i]->has_tag) {
            pblink[i]->has_tag = RIAK_TRUE;
            link[i]->tag = riak_binary_copy_from_pb(cfg, &(pblink[i]->tag));
            if (link[i]->tag == NULL) {
                return ERIAK_OUT_OF_MEMORY;
            }
        }
    }
    // Finally assign the pointer to the list of link pointers
    *link_target = link;

    return ERIAK_OK;
}

riak_error
riak_link_new_array(riak_config  *cfg,
                    riak_link  ***array,
                    riak_size_t   len) {
    riak_link **result = (riak_link**)(cfg->malloc_fn)(sizeof(riak_link)*len);
    if (result == NULL) {
        return ERIAK_OUT_OF_MEMORY;
    }
    memset((void*)result, '\0', sizeof(riak_link)*len);
    *array = result;

    return ERIAK_OK;
}

void
riak_links_free(riak_config  *cfg,
                riak_link  ***link_target,
                int           num_links) {
    riak_link **link = *link_target;
    int i;
    for(i = 0; i < num_links; i++) {
        riak_free(cfg, &(link[i]->bucket));
        riak_free(cfg, &(link[i]->key));
        riak_free(cfg, &(link[i]->tag));
        riak_free(cfg, &(link[i]));
    }
    riak_free(cfg, *link_target);
}

int
riak_links_print(riak_link    *link,
                 char         *target,
                 riak_uint32_t len) {
    int total = 0;
    char buffer[2048];
    if (link->has_bucket) {
        riak_binary_print(link->bucket, buffer, sizeof(buffer));
        total += riak_snprintf_cat(&target, &len, "Link buffer: %s\n", buffer);
    }
    if (link->has_key) {
        riak_binary_print(link->key, buffer, sizeof(buffer));
        total += riak_snprintf_cat(&target, &len, "Link Key: %s\n", buffer);
    }
    if (link->has_tag) {
        riak_binary_print(link->tag, buffer, sizeof(buffer));
        total += riak_snprintf_cat(&target, &len, "Link Tag: %s\n", target);
    }
    return total; /* beware: total could be >= len */
}

//
// R I A K   O B J E C T
//
riak_object*
riak_object_new(riak_config *cfg) {
    riak_object *o = (riak_object*)(cfg->malloc_fn)(sizeof(riak_object));
    if (o) memset(o, '\0', sizeof(riak_object));
    return o;
}

riak_error
riak_object_new_array(riak_config   *cfg,
                      riak_object ***array,
                      riak_size_t    len) {
    riak_object **result = (riak_object**)(cfg->malloc_fn)(sizeof(riak_object)*len);
    if (result == NULL) {
        return ERIAK_OUT_OF_MEMORY;
    }
    memset((void*)result, '\0', sizeof(riak_object)*len);
    *array = result;

    return ERIAK_OK;
}

void
riak_object_free_array(riak_config   *cfg,
                       riak_object ***array_target,
                       riak_size_t    len) {
    int i;
    riak_object **array = *array_target;
    for(i = 0; i < len; i++) {
        riak_object_free(cfg, &(array[i]));
    }
    riak_free(cfg, array_target);
}

int
riak_object_to_pb_copy(riak_config *cfg,
                       RpbContent  *to,
                       riak_object *from) {

    rpb_content__init(to);

    riak_binary_copy_to_pb(&(to->value), from->value);
    if (from->has_charset) {
        to->has_charset = RIAK_TRUE;
        riak_binary_copy_to_pb(&(to->charset), from->charset);
    }
    if (from->has_content_encoding) {
        to->has_content_encoding = RIAK_TRUE;
        riak_binary_copy_to_pb(&(to->content_encoding), from->encoding);
    }
    if (from->has_content_type) {
        to->has_content_type = RIAK_TRUE;
        riak_binary_copy_to_pb(&(to->content_type), from->content_type);
    }
    if (from->has_deleted) {
        to->has_deleted = RIAK_TRUE;
        to->deleted = from->deleted;
    }
    if (from->has_last_mod) {
        to->has_last_mod = RIAK_TRUE;
        to->last_mod = from->last_mod;
    }
    if (from->has_last_mod_usecs) {
        to->has_last_mod_usecs = RIAK_TRUE;
        to->last_mod_usecs = from->last_mod_usecs;
    }
    if (from->has_vtag) {
        to->has_vtag = RIAK_TRUE;
        riak_binary_copy_to_pb(&(to->vtag), from->vtag);
    }

    // Indexes
    if (from->n_indexes > 0) {
        to->n_indexes = from->n_indexes;
        int idxresult = riak_pairs_copy_to_pb(cfg, &(to->indexes), from->indexes, to->n_indexes);
        if (idxresult) {
            return idxresult;
        }
    }

    // User-Metadave
    if (from->n_usermeta > 0) {
        to->n_usermeta = from->n_usermeta;
        riak_error err = riak_pairs_copy_to_pb(cfg, &(to->usermeta), from->usermeta, to->n_usermeta);
        if (err) {
            return err;
        }
    }

    // Links
    if (from->n_links > 0) {
        to->n_links = from->n_links;
        riak_error err = riak_links_copy_to_pb(cfg, &(to->links), from->links, to->n_links);
        if (err) {
            return err;
        }
    }

    return ERIAK_OK;
}


riak_error
riak_object_new_from_pb(riak_config  *cfg,
                        riak_object **target,
                        RpbContent   *from) {
    *target = riak_object_new(cfg);
    if (*target == NULL) {
        return ERIAK_OUT_OF_MEMORY;
    }
    riak_object *to = *target;

    to->value = riak_binary_copy_from_pb(cfg, &(from->value));
    if (to->value == NULL) {
        riak_free(cfg, target);
        return ERIAK_OUT_OF_MEMORY;
    }
    if (from->has_charset) {
        to->has_charset = RIAK_TRUE;
        to->charset= riak_binary_copy_from_pb(cfg, &(from->charset));
        if (to->charset == NULL) {
            riak_free(cfg, target);
            return ERIAK_OUT_OF_MEMORY;
        }
    }
    if (from->has_content_encoding) {
        to->has_content_encoding = RIAK_TRUE;
        to->encoding = riak_binary_copy_from_pb(cfg, &(from->content_encoding));
        if (to->encoding == NULL) {
            riak_free(cfg, target);
            return ERIAK_OUT_OF_MEMORY;
        }
    }
    if (from->has_content_type) {
        to->has_content_type = RIAK_TRUE;
        to->content_type = riak_binary_copy_from_pb(cfg, &(from->content_type));
        if (to->content_type == NULL) {
            riak_free(cfg, target);
            return ERIAK_OUT_OF_MEMORY;
        }
    }
    if (from->has_deleted) {
        to->has_deleted = RIAK_TRUE;
        to->deleted = from->deleted;
    }
    if (from->has_last_mod) {
        to->has_last_mod = RIAK_TRUE;
        to->last_mod = from->last_mod;
    }
    if (from->has_last_mod_usecs) {
        to->has_last_mod_usecs = RIAK_TRUE;
        to->last_mod_usecs = from->last_mod_usecs;
    }
    if (from->has_vtag) {
        to->has_vtag = RIAK_TRUE;
        to->vtag = riak_binary_copy_from_pb(cfg, &(from->vtag));
        if (to->vtag == NULL) {
            riak_free(cfg, target);
            return ERIAK_OUT_OF_MEMORY;
        }
    }

    // Indexes
    if (from->n_indexes > 0) {
        to->n_indexes = from->n_indexes;
        int idxresult = riak_pairs_copy_array_from_pb(cfg, &(to->indexes), from->indexes, to->n_indexes);
        if (idxresult) {
            return idxresult;
        }
    }

    // User-Metadave
    if (from->n_usermeta > 0) {
        to->n_usermeta = from->n_usermeta;
        int uresult = riak_pairs_copy_array_from_pb(cfg, &(to->usermeta), from->usermeta, to->n_usermeta);
        if (uresult) {
            return uresult;
        }
    }

    // Links
    if (from->n_links > 0) {
        to->n_links = from->n_links;
        int lresult = riak_links_copy_from_pb(cfg, &(to->links), from->links, to->n_links);
        if (lresult) {
            return lresult;
        }
    }
    return ERIAK_OK;
}

int
riak_object_print(riak_object  *obj,
                  char         *target,
                  riak_uint32_t len) {
    char buffer[2048];
    riak_binary_print(obj->bucket, buffer, sizeof(buffer));
    int total = 0;

    /* XXX: we do not check that riak_snprintf() did not return -1 below. */
    total += riak_snprintf_cat(&target, &len, "Bucket: %s\n", buffer);

    if (obj->has_key) {
        riak_binary_print(obj->key, buffer, sizeof(buffer));
	total += riak_snprintf_cat(&target, &len, "Key: %s\n", buffer);
    }

    {
      // TODO: Bigger buffer for full value
      riak_binary_print(obj->value, buffer, sizeof(buffer));
      total += riak_snprintf_cat(&target, &len, "Value: %s\n", buffer);
    }

    if (obj->has_charset) {
        riak_binary_print(obj->charset, buffer, sizeof(buffer));
        total += riak_snprintf_cat(&target, &len, "Charset: %s\n", buffer);
    }
    if (obj->has_last_mod) {
        time_t mod = (time_t)(obj->last_mod);
        strftime(buffer, 1024, "%Y-%m-%d %H:%M:%S", localtime(&mod));
        total += riak_snprintf_cat(&target, &len, "Last Mod: %s\n", buffer);
    }
    if (obj->has_last_mod_usecs) {
        total += riak_snprintf_cat(&target, &len, "Last Mod uSecs: %d\n", obj->last_mod_usecs);
    }
    if (obj->has_content_type) {
        riak_binary_print(obj->content_type, buffer, sizeof(buffer));
        total += riak_snprintf_cat(&target, &len, "Content Type: %s\n", buffer);
    }
    if (obj->has_content_encoding) {
        riak_binary_print(obj->encoding, buffer, sizeof(buffer));
        total += riak_snprintf_cat(&target, &len, "Content Encoding: %s\n", buffer);
    }
    if (obj->has_deleted) {
        total += riak_snprintf_cat(&target, &len, "Deleted: %s\n", (obj->deleted) ? "true" : "false");
    }
    if (obj->has_vtag) {
        riak_binary_print(obj->vtag, buffer, sizeof(buffer));
        total += riak_snprintf_cat(&target, &len, "VTag: %s\n", buffer);
    }

    int i;
    for(i = 0; i < obj->n_links; i++) {
        int wrote = riak_links_print(obj->links[i], target, len);
	total += wrote; /* we will return the number of character which would have been written */

	if (wrote >= len)
	  wrote = len - 1;

	target += wrote;
	len -= wrote;
    }
    return total;
}

void
riak_object_free(riak_config *cfg,
                 riak_object **obj) {
    if (obj == NULL) return;
    riak_object* object = *obj;
    if (object == NULL) return;

    riak_free(cfg, &(object->bucket));
    riak_free(cfg, &(object->charset));
    riak_free(cfg, &(object->content_type));
    riak_free(cfg, &(object->encoding));
    riak_free(cfg, &(object->key));
    riak_free(cfg, &(object->value));
    riak_free(cfg, &(object->vtag));
    riak_pairs_free(cfg, &(object->indexes), object->n_indexes);
    riak_pairs_free(cfg, &(object->usermeta), object->n_usermeta);
    riak_links_free(cfg, &(object->links), object->n_links);
    riak_free(cfg, obj);
}

void
riak_object_free_pb(riak_config *cfg,
                    RpbContent   *obj) {
    riak_pairs_free_pb(cfg, &(obj->indexes), obj->n_indexes);
    riak_pairs_free_pb(cfg, &(obj->usermeta), obj->n_usermeta);
    riak_links_free_pb(cfg, &(obj->links), obj->n_links);
}

//
// ACCESSORS
//
riak_binary*
riak_object_get_bucket(riak_object *obj)
{
    return obj->bucket;
}
riak_boolean_t
riak_object_get_has_key(riak_object *obj)
{
    return obj->has_key;
}
riak_binary*
riak_object_get_key(riak_object *obj)
{
    return obj->key;
}
riak_binary*
riak_object_get_value(riak_object *obj)
{
    return obj->value;
}
riak_boolean_t
riak_object_get_has_charset(riak_object *obj)
{
    return obj->has_charset;
}
riak_binary*
riak_object_get_charset(riak_object *obj)
{
    return obj->charset;
}
riak_boolean_t
riak_object_get_has_last_mod(riak_object *obj)
{
    return obj->has_last_mod;
}
riak_uint32_t
riak_object_get_last_mod(riak_object *obj)
{
    return obj->last_mod;
}
riak_boolean_t
riak_object_get_has_last_mod_usecs(riak_object *obj)
{
    return obj->has_last_mod_usecs;
}
riak_uint32_t
riak_object_get_last_mod_usecs(riak_object *obj)
{
    return obj->last_mod_usecs;
}
riak_boolean_t
riak_object_get_has_content_type(riak_object *obj)
{
    return obj->has_content_type;
}
riak_binary*
riak_object_get_content_type(riak_object *obj)
{
    return obj->content_type;
}
riak_boolean_t
riak_object_get_has_content_encoding(riak_object *obj)
{
    return obj->has_content_encoding;
}
riak_binary*
riak_object_get_encoding(riak_object *obj)
{
    return obj->encoding;
}
riak_boolean_t
riak_object_get_has_deleted(riak_object *obj)
{
    return obj->has_deleted;
}
riak_boolean_t
riak_object_get_deleted(riak_object *obj)
{
    return obj->deleted;
}
riak_boolean_t
riak_object_get_has_vtag(riak_object *obj)
{
    return obj->has_vtag;
}
riak_binary*
riak_object_get_vtag(riak_object *obj)
{
    return obj->vtag;
}
riak_int32_t
riak_object_get_n_links(riak_object *obj)
{
    return obj->n_links;
}
riak_link**
riak_object_get_links(riak_object *obj)
{
    return obj->links;
}
riak_int32_t
riak_object_get_n_usermeta(riak_object *obj)
{
    return obj->n_usermeta;
}
riak_pair**
riak_object_get_usermeta(riak_object *obj)
{
    return obj->usermeta;
}
riak_int32_t
riak_object_get_n_indexes(riak_object *obj)
{
    return obj->n_indexes;
}
riak_pair**
riak_object_get_indexes(riak_object *obj)
{
    return obj->indexes;
}

void
riak_object_set_bucket(riak_object *obj,
                       riak_binary *value) {
     obj->bucket = value;
}
void
riak_object_set_key(riak_object *obj,
                    riak_binary *value) {
     obj->key = value;
     obj->has_key = RIAK_TRUE;
}
void
riak_object_set_value(riak_object *obj,
                      riak_binary *value) {
     obj->value = value;
}
void
riak_object_set_charset(riak_object *obj,
                        riak_binary *value) {
     obj->charset = value;
     obj->has_charset = RIAK_TRUE;
}
void
riak_object_set_last_mod(riak_object  *obj,
                         riak_uint32_t value) {
     obj->last_mod = value;
     obj->has_last_mod = RIAK_TRUE;
}
void
riak_object_set_last_mod_usecs(riak_object  *obj,
                               riak_uint32_t value) {
     obj->last_mod_usecs = value;
     obj->has_last_mod_usecs = RIAK_TRUE;
}
void
riak_object_set_content_type(riak_object *obj,
                             riak_binary *value) {
     obj->content_type = value;
     obj->has_content_type = RIAK_TRUE;
}
void
riak_object_set_encoding(riak_object *obj,
                         riak_binary *value) {
     obj->encoding = value;
     obj->has_content_encoding = RIAK_TRUE;
}
void
riak_object_set_deleted(riak_object   *obj,
                        riak_boolean_t value) {
     obj->deleted = value;
     obj->has_deleted = RIAK_TRUE;
}
void
riak_object_set_vtag(riak_object *obj,
                     riak_binary *value) {
     obj->vtag = value;
     obj->has_vtag = RIAK_TRUE;
}
void
riak_object_set_n_links(riak_object *obj,
                        riak_int32_t value) {
     obj->n_links = value;
}
void
riak_object_set_links(riak_object *obj,
                      riak_link  **value) {
     obj->links = value;
}
void
riak_object_set_n_usermeta(riak_object *obj,
                           riak_int32_t value) {
     obj->n_usermeta = value;
}
void
riak_object_set_usermeta(riak_object *obj,
                         riak_pair  **value) {
     obj->usermeta = value;
}
void
riak_object_set_n_indexes(riak_object *obj,
                          riak_int32_t value) {
     obj->n_indexes = value;
}
void
riak_object_set_indexes(riak_object *obj,
                        riak_pair  **value) {
     obj->indexes = value;
}

riak_boolean_t
riak_link_get_has_bucket(riak_link *link)
{
    return link->has_bucket;
}
riak_binary*
riak_link_get_bucket(riak_link *link)
{
    return link->bucket;
}
riak_boolean_t
riak_link_get_has_key(riak_link *link)
{
    return link->has_key;
}
riak_binary*
riak_link_get_key(riak_link *link)
{
    return link->key;
}
riak_boolean_t
riak_link_get_has_tag(riak_link *link)
{
    return link->has_tag;
}
riak_binary*
riak_link_get_tag(riak_link *link)
{
    return link->tag;
}
void
riak_link_set_bucket(riak_link   *link,
                     riak_binary *value) {
     link->bucket = value;
     link->has_bucket = RIAK_TRUE;
}
void
riak_link_set_key(riak_link   *link,
                  riak_binary *value) {
     link->key = value;
     link->has_key = RIAK_TRUE;
}
void
riak_link_set_tag(riak_link   *link,
                  riak_binary *value) {
     link->tag = value;
     link->has_tag = RIAK_TRUE;
}


riak_binary*
riak_pair_get_key(riak_pair *pair)
{
    return pair->key;
}
riak_boolean_t
riak_pair_get_has_value(riak_pair *pair)
{
    return pair->has_value;
}
riak_binary*
riak_pair_get_value(riak_pair *pair)
{
    return pair->value;
}
void
riak_pair_set_key(riak_pair   *pair,
                  riak_binary *value) {
    pair->key = value;
}
void
riak_pair_set_value(riak_pair   *pair,
                    riak_binary *value) {
    pair->value = value;
    pair->has_value = RIAK_TRUE;
}

